#pragma once

#include <memory>
#include <string>

#include <yaml-cpp/yaml.h>

#include "hft_common/ipc/shm_constants.h"
#include "hft_common/ipc/shm_ring_buffer.h"
#include "hft_common/protocol/protocol.h"

namespace md_gateway {

struct ShmTickPublisherConfig {
    std::string shm_name;
    uint64_t capacity {hft_common::ipc::kDefaultShmRingCapacity};
};

class ShmTickPublisher {
public:
    using config_type = ShmTickPublisherConfig;

    bool init(const ShmTickPublisherConfig& config) {
        if (config.shm_name.empty() || config.capacity == 0) {
            return false;
        }
        config_ = config;
        writer_ = std::make_unique<hft_common::ipc::ShmRingBuffer<TickRecord>>(
            config.shm_name, true, config.capacity);
        return true;
    }

    void publish(const TickRecord& tick) {
        if (!writer_) {
            return;
        }
        TickRecord* slot = writer_->claim();
        if (!slot) {
            return;
        }
        *slot = tick;
        writer_->publish();
    }

private:
    ShmTickPublisherConfig config_ {};
    std::unique_ptr<hft_common::ipc::ShmRingBuffer<TickRecord>> writer_;
};

inline bool load_component_config(const std::string& config_path, ShmTickPublisherConfig& config) {
    YAML::Node doc = YAML::LoadFile(config_path);
    YAML::Node publisher = doc["publisher"] ? doc["publisher"] : doc;

    if (!publisher["shm_name"]) {
        return false;
    }

    config.shm_name = publisher["shm_name"].as<std::string>();
    config.capacity = publisher["capacity"]
        ? publisher["capacity"].as<uint64_t>()
        : hft_common::ipc::kDefaultShmRingCapacity;
    return true;
}

}  // namespace md_gateway
