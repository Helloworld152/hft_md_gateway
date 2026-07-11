#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include <yaml-cpp/yaml.h>

#include "ThostFtdcMdApi.h"
#include "hft_common/base/queue.h"
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
    struct Impl : public CThostFtdcMdSpi {
        explicit Impl(std::size_t capacity) : queue(capacity) {}

        bool init(const CtpApiSourceConfig& in_config) {
            config = in_config;
            if (config.md_front.empty() || config.broker_id.empty() || config.symbols.empty()) {
                std::cerr << "[CtpApiSource] missing required runtime config" << std::endl;
                return false;
            }
            std::filesystem::create_directories("log");
            md_api = CThostFtdcMdApi::CreateFtdcMdApi("./log/");
            if (!md_api) {
                std::cerr << "[CtpApiSource] create md api failed" << std::endl;
                return false;
            }
            md_api->RegisterSpi(this);
            md_api->RegisterFront(const_cast<char*>(config.md_front.c_str()));
            running = true;
            md_api->Init();
            return true;
        }

        bool poll(CtpRawTick& out) { return queue.pop(out); }

        void stop() {
            if (!running.exchange(false)) return;
            if (md_api) {
                md_api->RegisterSpi(nullptr);
                md_api->Release();
                md_api = nullptr;
            }
        }

        void OnFrontConnected() override {
            std::clog << "[CtpApiSource] front connected, logging in" << std::endl;
            CThostFtdcReqUserLoginField req = {0};
            std::strncpy(req.BrokerID, config.broker_id.c_str(), sizeof(req.BrokerID) - 1);
            std::strncpy(req.UserID, config.user_id.c_str(), sizeof(req.UserID) - 1);
            std::strncpy(req.Password, config.password.c_str(), sizeof(req.Password) - 1);
            md_api->ReqUserLogin(&req, 0);
        }

        void OnRspUserLogin(CThostFtdcRspUserLoginField* pRspUserLogin,
                            CThostFtdcRspInfoField* pRspInfo, int, bool) override {
            if (pRspInfo && pRspInfo->ErrorID != 0) {
                std::cerr << "[CtpApiSource] login failed: " << pRspInfo->ErrorID << " "
                          << pRspInfo->ErrorMsg << std::endl;
                return;
            }
            if (pRspUserLogin && config.trading_day == 0 && pRspUserLogin->TradingDay[0] != '\0') {
                config.trading_day = static_cast<uint32_t>(std::stoi(pRspUserLogin->TradingDay));
            }
            std::vector<char*> subs;
            subs.reserve(config.symbols.size());
            for (auto& symbol : config.symbols)
                subs.push_back(const_cast<char*>(symbol.c_str()));
            if (!subs.empty())
                md_api->SubscribeMarketData(subs.data(), static_cast<int>(subs.size()));
            std::clog << "[CtpApiSource] login success, subscribed " << subs.size()
                      << " symbols" << std::endl;
        }

        void OnRtnDepthMarketData(CThostFtdcDepthMarketDataField* pData) override {
            if (!running || pData == nullptr) return;
            CtpRawTick raw {};
            raw.data = *pData;
            raw.local_ts = static_cast<uint64_t>(
                std::chrono::duration_cast<std::chrono::microseconds>(
                    std::chrono::system_clock::now().time_since_epoch())
                    .count());
            if (!queue.push(raw))
                std::cerr << "[CtpApiSource] queue full, dropping raw tick for "
                          << pData->InstrumentID << std::endl;
        }

        CtpApiSourceConfig config {};
        SpscQueue<CtpRawTick> queue;
        CThostFtdcMdApi* md_api {nullptr};
        std::atomic<bool> running {false};
    };

public:
    using config_type = CtpApiSourceConfig;
    using output_type = CtpRawTick;

    CtpApiSource() = default;
    CtpApiSource(CtpApiSource&&) = default;
    CtpApiSource& operator=(CtpApiSource&&) = default;
    ~CtpApiSource() { if (impl_) impl_->stop(); }

    bool init(const config_type& config) {
        if (!impl_ || impl_->queue.capacity() != config.queue_capacity)
            impl_ = std::make_unique<Impl>(config.queue_capacity);
        return impl_->init(config);
    }

    bool poll(CtpRawTick& out) { return impl_->poll(out); }
    void stop() { if (impl_) impl_->stop(); }

private:
    std::unique_ptr<Impl> impl_ {std::make_unique<Impl>(65536)};
};

inline bool load_component_config(const std::string& config_path, CtpApiSourceConfig& config) {
    YAML::Node doc = YAML::LoadFile(config_path);
    YAML::Node source = doc["source"] ? doc["source"] : doc;

    if (!source["md_front"] || !source["broker_id"]) return false;
    config.md_front = source["md_front"].as<std::string>();
    config.broker_id = source["broker_id"].as<std::string>();
    config.user_id = source["user_id"] ? source["user_id"].as<std::string>() : "";
    config.password = source["password"] ? source["password"].as<std::string>() : "";
    if (source["trading_day"])
        config.trading_day = static_cast<uint32_t>(std::stoul(source["trading_day"].as<std::string>()));
    if (source["symbols"] && source["symbols"].IsSequence())
        for (const auto& symbol : source["symbols"])
            config.symbols.push_back(symbol.as<std::string>());
    if (source["queue_capacity"])
        config.queue_capacity = source["queue_capacity"].as<std::size_t>();
    return !config.symbols.empty();
}

}  // namespace md_gateway
