#pragma once

#include <chrono>
#include <cstdint>
#include <iostream>
#include <string>

#include <yaml-cpp/yaml.h>

#include "hft_common/net/udp_multicast_receiver.h"
#include "md_gateway/model/udp_multicast_raw_packet.hpp"

namespace md_gateway {

using UdpMulticastSourceConfig = hft_common::net::UdpMulticastReceiverConfig;

class UdpMulticastSource {
public:
    using config_type = UdpMulticastSourceConfig;
    using output_type = UdpMulticastRawPacket;

    bool init(const config_type& config) {
        stop();
        try {
            receiver_.init(config);
        } catch (const std::exception& ex) {
            std::cerr << "[UdpMulticastSource] init failed: " << ex.what() << std::endl;
            return false;
        }
        return true;
    }

    bool poll(output_type& out) {
        const ssize_t received = receiver_.recv(out.data, output_type::kMaxLen);
        if (received <= 0) {
            return false;
        }

        const auto now = std::chrono::system_clock::now().time_since_epoch();
        const uint64_t now_us = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(now).count());
        out.length = static_cast<uint32_t>(received);
        out.timestamp_sec = now_us / 1000000;
        out.timestamp_usec = now_us % 1000000;
        return true;
    }

    void stop() {
        receiver_.close();
    }

    hft_common::net::UdpMulticastReceiver receiver_ {};
};

inline bool load_component_config(const std::string& config_path, UdpMulticastSourceConfig& config) {
    YAML::Node doc = YAML::LoadFile(config_path);
    YAML::Node source = doc["source"] ? doc["source"] : doc;

    if (!source["multicast_ip"] || !source["port"]) {
        return false;
    }

    config.multicast_ip = source["multicast_ip"].as<std::string>();
    config.port = source["port"].as<uint16_t>();
    if (source["interface_ip"]) {
        config.interface_ip = source["interface_ip"].as<std::string>();
    }
    if (source["reuse_addr"]) {
        config.reuse_addr = source["reuse_addr"].as<bool>();
    }
    return true;
}

}  // namespace md_gateway
