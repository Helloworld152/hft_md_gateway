#pragma once

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <string>
#include <vector>

#include <yaml-cpp/yaml.h>

#include "hft_common/protocol/protocol.h"
#include "md_gateway/model/pcap_raw_frame.hpp"

namespace md_gateway {

class PcapTickRecordDecoder {
public:
    using output_type = TickRecord;

    struct Config {
        uint32_t trading_day {0};
        std::vector<uint32_t> enabled_channel_ids;
    };
    using config_type = Config;

    bool init(const Config& config) {
        config_ = config;
        return true;
    }

    bool decode(const PcapRawFrame& in, TickRecord& out) const noexcept {
        if (in.length < 44) {
            return false;
        }

        // Skip Ethernet (14) + IP (20) + UDP (8) to get application payload.
        const uint8_t* payload = reinterpret_cast<const uint8_t*>(in.data) + 42;
        const uint32_t payload_len = in.length - 42;

        std::memset(&out, 0, sizeof(out));
        out.update_time = pcap_ts_to_hhmmssmmm(in.timestamp_sec, in.timestamp_usec);
        out.trading_day = config_.trading_day;

        parse_payload(payload, payload_len, out);
        return true;
    }

private:
    static uint64_t pcap_ts_to_hhmmssmmm(uint64_t sec, uint64_t usec) noexcept {
        const std::time_t s = static_cast<std::time_t>(sec);
        std::tm tm_local {};
        localtime_r(&s, &tm_local);
        const uint64_t hhmmss = static_cast<uint64_t>(tm_local.tm_hour) * 10000
            + static_cast<uint64_t>(tm_local.tm_min) * 100
            + static_cast<uint64_t>(tm_local.tm_sec);
        return hhmmss * 1000 + (usec / 1000);
    }

    // Override this to implement protocol-specific payload parsing.
    static void parse_payload(const uint8_t* payload, uint32_t len, TickRecord& out) noexcept {
        (void)payload;
        (void)len;
        (void)out;
    }

    Config config_ {};
};

inline bool load_component_config(const std::string& config_path, PcapTickRecordDecoder::Config& config) {
    YAML::Node doc = YAML::LoadFile(config_path);
    YAML::Node source = doc["source"] ? doc["source"] : doc;

    if (source["trading_day"]) {
        config.trading_day = static_cast<uint32_t>(std::stoul(source["trading_day"].as<std::string>()));
    }
    return true;
}

}  // namespace md_gateway
