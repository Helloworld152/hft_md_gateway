#pragma once

#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

#include <yaml-cpp/yaml.h>

#include "md_gateway/model/pcap_raw_frame.hpp"

namespace md_gateway {

struct EfviSourceConfig {
    std::string interface;
    std::string bind_ip;
    std::vector<uint16_t> ports;
    uint32_t trading_day {0};
    int evq_capacity {1024};
    int evq_timeout_us {100};
};

class EfviSource {
public:
    using config_type = EfviSourceConfig;
    using output_type = PcapRawFrame;

    EfviSource() = default;

    ~EfviSource() {
        stop();
    }

    bool init(const config_type& config) {
        config_ = config;

        if (ef_driver_open(&dh_) < 0) {
            std::cerr << "[EfviSource] ef_driver_open failed" << std::endl;
            return false;
        }

        int ifindex = ef_ifindex_get(dh_, config_.interface.c_str());
        if (ifindex < 0) {
            std::cerr << "[EfviSource] interface not found: " << config_.interface << std::endl;
            ef_driver_close(dh_);
            dh_ = -1;
            return false;
        }

        if (ef_pd_alloc(&pd_, dh_, ifindex, -1) < 0) {
            std::cerr << "[EfviSource] ef_pd_alloc failed" << std::endl;
            ef_driver_close(dh_);
            dh_ = -1;
            return false;
        }

        if (ef_vi_alloc_from_pd(&vi_, dh_, &pd_, dh_, -1, 0, 0, EF_VI_RX, 0, nullptr) < 0) {
            std::cerr << "[EfviSource] ef_vi_alloc_from_pd failed" << std::endl;
            ef_pd_free(&pd_, dh_);
            ef_driver_close(dh_);
            dh_ = -1;
            return false;
        }

        rx_prefix_len_ = ef_vi_receive_prefix_len(&vi_);

        for (auto port : config_.ports) {
            ef_filter_spec fs {};
            fs.ip_proto = EF_IP_PROTO_UDP;
            fs.dmac_is_mc = 1;
            if (!config_.bind_ip.empty()) {
                ef_address_set(&fs.dest_ip4, config_.bind_ip.c_str());
            } else {
                fs.dest_ip4 = EF_IP4_ADDR_ANY;
            }
            fs.dest_port = port;

            if (ef_vi_filter_add(&vi_, dh_, &fs, nullptr) < 0) {
                std::cerr << "[EfviSource] filter add failed for port " << port << std::endl;
            }
        }

        ef_vi_receive_init(&vi_, config_.evq_capacity, 0);

        for (int i = 0; i < config_.evq_capacity; ++i) {
            ef_vi_receive_post(&vi_, reinterpret_cast<ef_addr>(rx_buf_ + i * kBufSize), kBufSize);
        }

        running_ = true;
        return true;
    }

    bool poll(output_type& out) {
        if (!running_) return false;

        ef_event ev {};
        if (ef_eventq_poll(&vi_, &ev, config_.evq_timeout_us) == 0) {
            return false;
        }

        if (ev.type != EF_EVENT_TYPE_RX && ev.type != EF_EVENT_TYPE_RX_DISCARD) {
            ef_vi_receive_post(&vi_, ev.rx.prefix_id, kBufSize);
            return false;
        }

        const char* data = reinterpret_cast<const char*>(ev.rx.prefix) + rx_prefix_len_;
        int len = ev.rx.frame_len - rx_prefix_len_;

        if (len < 0) len = 0;
        out.length = static_cast<uint32_t>(len) > sizeof(out.data) ? sizeof(out.data)
                                                                   : static_cast<uint32_t>(len);
        std::memcpy(out.data, data, out.length);
        out.timestamp_sec = 0;
        out.timestamp_usec = 0;

        ef_vi_receive_post(&vi_, ev.rx.prefix_id, kBufSize);
        return true;
    }

    void stop() {
        if (!running_) return;
        running_ = false;
        if (vi_.initialised) {
            ef_vi_free(&vi_);
        }
        if (pd_.initialised) {
            ef_pd_free(&pd_, dh_);
        }
        if (dh_ >= 0) {
            ef_driver_close(dh_);
            dh_ = -1;
        }
    }

private:
    static constexpr int kBufSize = 4096;

    EfviSourceConfig config_ {};
    int dh_ {-1};
    struct ef_pd pd_ {};
    struct ef_vi vi_ {};
    int rx_prefix_len_ {0};
    bool running_ {false};
    alignas(4096) char rx_buf_[4096 * 1024] {};
};

inline bool load_component_config(const std::string& config_path, EfviSourceConfig& config) {
    YAML::Node doc = YAML::LoadFile(config_path);
    YAML::Node source = doc["source"] ? doc["source"] : doc;

    if (!source["interface"]) return false;
    config.interface = source["interface"].as<std::string>();
    if (source["bind_ip"]) config.bind_ip = source["bind_ip"].as<std::string>();
    if (source["ports"] && source["ports"].IsSequence())
        for (const auto& p : source["ports"])
            config.ports.push_back(p.as<uint16_t>());
    if (source["trading_day"])
        config.trading_day = static_cast<uint32_t>(std::stoul(source["trading_day"].as<std::string>()));
    if (source["evq_capacity"])
        config.evq_capacity = source["evq_capacity"].as<int>();
    if (source["evq_timeout_us"])
        config.evq_timeout_us = source["evq_timeout_us"].as<int>();
    return !config.ports.empty();
}

}  // namespace md_gateway
