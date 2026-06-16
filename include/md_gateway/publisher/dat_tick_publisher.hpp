#pragma once

#include <memory>
#include <string>
#include <filesystem>

#include <yaml-cpp/yaml.h>

#include "hft_common/ipc/mmap.h"
#include "hft_common/ipc/mmap_protocol_traits.h"
#include "hft_common/protocol/protocol.h"

namespace md_gateway {

struct DatTickPublisherConfig {
    std::string base_path;
    uint64_t capacity {1 << 20};
    bool prefault {false};
    std::string symbol;
    uint32_t trading_day {0};
};

class DatTickPublisher {
public:
    DatTickPublisher() = default;
    using config_type = DatTickPublisherConfig;

    bool init(const DatTickPublisherConfig& config) {
        if (config.base_path.empty() || config.capacity == 0) {
            return false;
        }
        config_ = config;
        std::filesystem::create_directories(std::filesystem::path(config.base_path).parent_path());
        writer_ = std::make_unique<hft_common::ipc::MmapWriter<TickRecord>>(
            config.base_path, config.capacity, config.prefault);
        return true;
    }

    void publish(const TickRecord& tick) {
        if (!writer_) {
            return;
        }
        writer_->write(tick);
    }

private:
    DatTickPublisherConfig config_ {};
    std::unique_ptr<hft_common::ipc::MmapWriter<TickRecord>> writer_;
};

inline bool load_component_config(const std::string& config_path, DatTickPublisherConfig& config) {
    YAML::Node doc = YAML::LoadFile(config_path);
    YAML::Node publisher = doc["publisher"] ? doc["publisher"] : doc;

    if (!publisher["trading_day"]) {
        return false;
    }

    const std::string output_path =
        publisher["output_path"] ? publisher["output_path"].as<std::string>() : "data";
    const std::string file_suffix =
        publisher["file_suffix"] ? publisher["file_suffix"].as<std::string>() : "";
    const std::string trading_day = publisher["trading_day"].as<std::string>();

    config.base_path = output_path + "/market_data_" + trading_day + file_suffix;
    config.capacity =
        publisher["initial_capacity"] ? publisher["initial_capacity"].as<uint64_t>() : (1ULL << 20);
    config.trading_day = static_cast<uint32_t>(std::stoul(trading_day));
    return true;
}

}  // namespace md_gateway
