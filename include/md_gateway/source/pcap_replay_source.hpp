#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

#include <yaml-cpp/yaml.h>

#include "md_gateway/model/pcap_raw_frame.hpp"

namespace md_gateway {

struct PcapReplaySourceConfig {
    std::string pcap_file;
    std::string bpf_filter;
    std::size_t queue_capacity {65536};
    bool repeat {false};
    uint32_t trading_day {0};
};

class PcapReplaySource {
public:
    using config_type = PcapReplaySourceConfig;
    using output_type = PcapRawFrame;

    PcapReplaySource();
    ~PcapReplaySource();

    PcapReplaySource(const PcapReplaySource&) = default;
    PcapReplaySource& operator=(const PcapReplaySource&) = default;

    bool init(const config_type& config);
    bool poll(output_type& out);
    void stop();

private:
    struct Impl;
    std::shared_ptr<Impl> impl_;
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
    if (source["queue_capacity"]) {
        config.queue_capacity = source["queue_capacity"].as<std::size_t>();
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
