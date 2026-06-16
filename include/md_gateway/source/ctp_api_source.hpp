#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <yaml-cpp/yaml.h>

#include "md_gateway/model/ctp_raw_tick.hpp"

namespace md_gateway {

struct CtpApiSourceConfig {
    std::string md_front;
    std::string broker_id;
    std::string user_id;
    std::string password;
    std::vector<std::string> symbols;
    uint32_t trading_day {0};
    uint32_t source_id {1};
    uint32_t channel_id {1};
    std::size_t queue_capacity {65536};
};

class CtpApiSource {
public:
    using config_type = CtpApiSourceConfig;
    using output_type = CtpRawTick;

    CtpApiSource();
    ~CtpApiSource();

    CtpApiSource(const CtpApiSource&) = default;
    CtpApiSource& operator=(const CtpApiSource&) = default;

    bool init(const CtpApiSourceConfig& config);
    bool poll(output_type& out);
    void stop();

private:
    struct Impl;
    std::shared_ptr<Impl> impl_;
};

inline bool load_component_config(const std::string& config_path, CtpApiSourceConfig& config) {
    YAML::Node doc = YAML::LoadFile(config_path);
    YAML::Node source = doc["source"] ? doc["source"] : doc;

    if (!source["md_front"] || !source["broker_id"]) {
        return false;
    }
    config.md_front = source["md_front"].as<std::string>();
    config.broker_id = source["broker_id"].as<std::string>();
    config.user_id = source["user_id"] ? source["user_id"].as<std::string>() : "";
    config.password = source["password"] ? source["password"].as<std::string>() : "";
    if (source["trading_day"]) {
        config.trading_day = static_cast<uint32_t>(std::stoul(source["trading_day"].as<std::string>()));
    }
    if (source["symbols"] && source["symbols"].IsSequence()) {
        for (const auto& symbol : source["symbols"]) {
            config.symbols.push_back(symbol.as<std::string>());
        }
    }
    if (source["queue_capacity"]) {
        config.queue_capacity = source["queue_capacity"].as<std::size_t>();
    }
    return !config.symbols.empty();
}

}  // namespace md_gateway
