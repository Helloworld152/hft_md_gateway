#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <yaml-cpp/yaml.h>

#include "md_gateway/model/binance_depth_raw_tick.hpp"

namespace md_gateway {

struct BinanceCcapiSourceConfig {
    std::vector<std::string> symbols;
    std::string proxy;
    std::string depth_options {"MARKET_DEPTH_MAX=5&CONFLATE_INTERVAL_MILLISECONDS=1000"};
    uint32_t trading_day {0};
    std::size_t queue_capacity {65536};
};

class BinanceCcapiSource {
public:
    using config_type = BinanceCcapiSourceConfig;
    using output_type = BinanceDepthRawTick;

    BinanceCcapiSource();
    ~BinanceCcapiSource();

    BinanceCcapiSource(const BinanceCcapiSource&) = default;
    BinanceCcapiSource& operator=(const BinanceCcapiSource&) = default;

    bool init(const config_type& config);
    bool poll(output_type& out);
    void stop();

private:
    struct Impl;
    std::shared_ptr<Impl> impl_;
};

inline bool load_component_config(const std::string& config_path, BinanceCcapiSourceConfig& config) {
    YAML::Node doc = YAML::LoadFile(config_path);
    YAML::Node source = doc["source"] ? doc["source"] : doc;

    if (source["symbols"] && source["symbols"].IsSequence()) {
        config.symbols.clear();
        for (const auto& symbol : source["symbols"]) {
            config.symbols.push_back(symbol.as<std::string>());
        }
    }
    if (source["proxy"]) {
        config.proxy = source["proxy"].as<std::string>();
    }
    if (source["depth_options"]) {
        config.depth_options = source["depth_options"].as<std::string>();
    }
    if (source["trading_day"]) {
        config.trading_day = static_cast<uint32_t>(std::stoul(source["trading_day"].as<std::string>()));
    }
    if (source["queue_capacity"]) {
        config.queue_capacity = source["queue_capacity"].as<std::size_t>();
    }
    return !config.symbols.empty();
}

}  // namespace md_gateway
