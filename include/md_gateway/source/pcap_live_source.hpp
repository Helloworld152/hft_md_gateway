#pragma once

#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>

#include <pcap.h>
#include <yaml-cpp/yaml.h>

#include "md_gateway/model/pcap_raw_frame.hpp"

namespace md_gateway {

struct PcapLiveSourceConfig {
    std::string interface;
    std::string bpf_filter;
    int snaplen {65535};
    int timeout_ms {1000};
    bool promiscuous {true};
    uint32_t trading_day {0};
};

class PcapLiveSource {
public:
    using config_type = PcapLiveSourceConfig;
    using output_type = PcapRawFrame;

    PcapLiveSource() = default;

    ~PcapLiveSource() {
        stop();
    }

    bool init(const config_type& config) {
        config_ = config;

        char errbuf[PCAP_ERRBUF_SIZE] {};
        handle_ = pcap_open_live(config_.interface.c_str(), config_.snaplen,
                                 config_.promiscuous ? 1 : 0, config_.timeout_ms, errbuf);
        if (!handle_) {
            std::cerr << "[PcapLiveSource] pcap_open_live failed: " << errbuf << std::endl;
            return false;
        }

        if (!config_.bpf_filter.empty()) {
            bpf_u_int32 net = 0;
            bpf_u_int32 mask = 0;
            if (pcap_lookupnet(config_.interface.c_str(), &net, &mask, errbuf) == -1) {
                std::cerr << "[PcapLiveSource] pcap_lookupnet failed: " << errbuf
                          << ", using 0/0" << std::endl;
            }
            struct bpf_program fp {};
            if (pcap_compile(handle_, &fp, config_.bpf_filter.c_str(), 0, net) == -1) {
                std::cerr << "[PcapLiveSource] pcap_compile failed: " << pcap_geterr(handle_) << std::endl;
                pcap_close(handle_);
                handle_ = nullptr;
                return false;
            }
            if (pcap_setfilter(handle_, &fp) == -1) {
                std::cerr << "[PcapLiveSource] pcap_setfilter failed: " << pcap_geterr(handle_) << std::endl;
                pcap_freecode(&fp);
                pcap_close(handle_);
                handle_ = nullptr;
                return false;
            }
            pcap_freecode(&fp);
        }
        return true;
    }

    bool poll(output_type& out) {
        if (!handle_) {
            return false;
        }
        struct pcap_pkthdr* header = nullptr;
        const u_char* packet = nullptr;
        int ret = pcap_next_ex(handle_, &header, &packet);

        if (ret != 1 || !packet || !header) {
            return false;
        }

        out.length = header->caplen > sizeof(out.data) ? sizeof(out.data)
                                                       : static_cast<uint32_t>(header->caplen);
        std::memcpy(out.data, packet, out.length);
        out.timestamp_sec = header->ts.tv_sec;
        out.timestamp_usec = header->ts.tv_usec;
        return true;
    }

    void stop() {
        if (handle_) {
            pcap_close(handle_);
            handle_ = nullptr;
        }
    }

private:
    PcapLiveSourceConfig config_ {};
    pcap_t* handle_ {nullptr};
};

inline bool load_component_config(const std::string& config_path, PcapLiveSourceConfig& config) {
    YAML::Node doc = YAML::LoadFile(config_path);
    YAML::Node source = doc["source"] ? doc["source"] : doc;

    if (!source["interface"]) {
        return false;
    }
    config.interface = source["interface"].as<std::string>();
    if (source["bpf_filter"]) {
        config.bpf_filter = source["bpf_filter"].as<std::string>();
    }
    if (source["snaplen"]) {
        config.snaplen = source["snaplen"].as<int>();
    }
    if (source["timeout_ms"]) {
        config.timeout_ms = source["timeout_ms"].as<int>();
    }
    if (source["promiscuous"]) {
        config.promiscuous = source["promiscuous"].as<bool>();
    }
    if (source["trading_day"]) {
        config.trading_day = static_cast<uint32_t>(std::stoul(source["trading_day"].as<std::string>()));
    }
    return true;
}

}  // namespace md_gateway
