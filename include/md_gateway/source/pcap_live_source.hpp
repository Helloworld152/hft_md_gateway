#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

#include <yaml-cpp/yaml.h>

#include "md_gateway/model/pcap_raw_frame.hpp"

namespace md_gateway {

struct PcapLiveSourceConfig {
    std::string interface;
    std::string bpf_filter;
    int snaplen {65535};
    int timeout_ms {1000};
    bool promiscuous {true};
    std::size_t queue_capacity {65536};
    uint32_t trading_day {0};
};

class PcapLiveSource {
public:
    using config_type = PcapLiveSourceConfig;
    using output_type = PcapRawFrame;

    PcapLiveSource();
    ~PcapLiveSource();

    PcapLiveSource(const PcapLiveSource&) = default;
    PcapLiveSource& operator=(const PcapLiveSource&) = default;

    bool init(const config_type& config);
    bool poll(output_type& out);
    void stop();

private:
    struct Impl;
    std::shared_ptr<Impl> impl_;
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
    if (source["queue_capacity"]) {
        config.queue_capacity = source["queue_capacity"].as<std::size_t>();
    }
    if (source["trading_day"]) {
        config.trading_day = static_cast<uint32_t>(std::stoul(source["trading_day"].as<std::string>()));
    }
    return true;
}

}  // namespace md_gateway
