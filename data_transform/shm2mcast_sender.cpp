#include <chrono>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>

#include <yaml-cpp/yaml.h>

#include "hft_common/ipc/shm_ring_buffer.h"
#include "hft_common/net/udp_multicast_sender.h"
#include "md_gateway/model/ctp_shm_tick_record.hpp"

namespace {

struct Config {
    std::string shm_name;
    std::string multicast_ip;
    std::string interface_ip;
    uint16_t port {0};
    uint32_t poll_interval_ms {1};
    int ttl {1};
    bool loopback {false};
};

bool load_config(const std::string& config_path, Config& config) {
    const YAML::Node doc = YAML::LoadFile(config_path);
    const YAML::Node source = doc["source"] ? doc["source"] : doc;
    const YAML::Node publisher = doc["publisher"] ? doc["publisher"] : doc;

    if (!source["shm_name"] || !publisher["multicast_ip"] || !publisher["port"]) {
        return false;
    }

    config.shm_name = source["shm_name"].as<std::string>();
    config.multicast_ip = publisher["multicast_ip"].as<std::string>();
    config.port = publisher["port"].as<uint16_t>();
    config.interface_ip = publisher["interface_ip"] ? publisher["interface_ip"].as<std::string>() : "";
    config.poll_interval_ms = publisher["poll_interval_ms"] ? publisher["poll_interval_ms"].as<uint32_t>() : 1;
    config.ttl = publisher["ttl"] ? publisher["ttl"].as<int>() : 1;
    config.loopback = publisher["loopback"] ? publisher["loopback"].as<bool>() : false;
    return !config.shm_name.empty() && !config.multicast_ip.empty() && config.port != 0;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: shm2mcast_sender <config.yaml>" << std::endl;
        return 1;
    }

    Config config {};
    if (!load_config(argv[1], config)) {
        std::cerr << "failed to load config: " << argv[1] << std::endl;
        return 1;
    }

    try {
        hft_common::ipc::ShmRingBuffer<md_gateway::CtpShmTickRecord> reader(config.shm_name, false);
        hft_common::net::UdpMulticastSender sender;
        sender.init({config.multicast_ip, config.interface_ip, config.port, config.ttl, config.loopback});

        uint64_t next_seq = 1;
        for (;;) {
            const uint64_t latest = reader.latest_seq();
            if (next_seq > latest) {
                std::this_thread::sleep_for(std::chrono::milliseconds(config.poll_interval_ms));
                continue;
            }

            for (; next_seq <= latest; ++next_seq) {
                const md_gateway::CtpShmTickRecord* tick = reader.read(next_seq);
                if (!tick) {
                    continue;
                }
                sender.send(tick, sizeof(*tick));
            }
        }
    } catch (const std::exception& ex) {
        std::cerr << "shm2mcast_sender failed: " << ex.what() << std::endl;
        return 1;
    }
}
