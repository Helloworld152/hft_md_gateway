#pragma once

#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>

#include <pcap.h>
#include <yaml-cpp/yaml.h>

#include "md_gateway/model/pcap_raw_frame.hpp"

namespace md_gateway {

struct PcapReplaySourceConfig {
    std::string pcap_file;
    std::string bpf_filter;
    bool repeat {false};
    uint32_t trading_day {0};
};

class PcapReplaySource {
public:
    using config_type = PcapReplaySourceConfig;
    using output_type = PcapRawFrame;

    PcapReplaySource() = default;

    ~PcapReplaySource() {
        stop();
    }

    bool init(const config_type& config) {
        config_ = config;
        return reopen();
    }

    bool poll(output_type& out) {
        if (!handle_) {
            return false;
        }
        struct pcap_pkthdr* header = nullptr;
        const u_char* packet = nullptr;
        int ret = pcap_next_ex(handle_, &header, &packet);

        if (ret == -2) {
            if (config_.repeat) {
                pcap_close(handle_);
                handle_ = nullptr;
                if (!reopen()) {
                    return false;
                }
                ret = pcap_next_ex(handle_, &header, &packet);
            } else {
                return false;
            }
        }
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
    bool reopen() {
        char errbuf[PCAP_ERRBUF_SIZE] {};
        handle_ = pcap_open_offline(config_.pcap_file.c_str(), errbuf);
        if (!handle_) {
            std::cerr << "[PcapReplaySource] pcap_open_offline failed: " << errbuf << std::endl;
            return false;
        }

        if (!config_.bpf_filter.empty()) {
            bpf_u_int32 net = 0;
            struct bpf_program fp {};
            if (pcap_compile(handle_, &fp, config_.bpf_filter.c_str(), 0, net) == -1) {
                std::cerr << "[PcapReplaySource] pcap_compile failed: " << pcap_geterr(handle_) << std::endl;
                pcap_close(handle_);
                handle_ = nullptr;
                return false;
            }
            if (pcap_setfilter(handle_, &fp) == -1) {
                std::cerr << "[PcapReplaySource] pcap_setfilter failed: " << pcap_geterr(handle_) << std::endl;
                pcap_freecode(&fp);
                pcap_close(handle_);
                handle_ = nullptr;
                return false;
            }
            pcap_freecode(&fp);
        }
        return true;
    }

    PcapReplaySourceConfig config_ {};
    pcap_t* handle_ {nullptr};
};

inline bool load_component_config(const std::string& config_path, PcapReplaySourceConfig& config) {
    YAML::Node doc = YAML::LoadFile(config_path);
    YAML::Node source = doc["source"] ? doc["source"] : doc;

    if (!source["pcap_file"]) {
        return false;
    }
    config.pcap_file = source["pcap_file"].as<std::string>();
    if (source["bpf_filter"]) {
        config.bpf_filter = source["bpf_filter"].as<std::string>();
    }
    if (source["repeat"]) {
        config.repeat = source["repeat"].as<bool>();
    }
    if (source["trading_day"]) {
        config.trading_day = static_cast<uint32_t>(std::stoul(source["trading_day"].as<std::string>()));
    }
    return true;
}

}  // namespace md_gateway
