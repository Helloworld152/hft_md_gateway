#include "md_gateway/source/ctp_api_source.hpp"

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <utility>

#include "ThostFtdcMdApi.h"
#include "hft_common/base/queue.h"
#include <yaml-cpp/yaml.h>

namespace md_gateway {

struct CtpApiSource::Impl : public CThostFtdcMdSpi {
    explicit Impl(std::size_t queue_capacity)
        : queue(queue_capacity) {}

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

    bool poll(output_type& out) {
        return queue.pop(out);
    }

    void stop() {
        if (!running.exchange(false)) {
            return;
        }
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
                        CThostFtdcRspInfoField* pRspInfo,
                        int,
        bool) override {
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
        for (auto& symbol : config.symbols) {
            subs.push_back(const_cast<char*>(symbol.c_str()));
        }
        if (!subs.empty()) {
            md_api->SubscribeMarketData(subs.data(), static_cast<int>(subs.size()));
        }
        std::clog << "[CtpApiSource] login success, subscribed " << subs.size()
                  << " symbols" << std::endl;
    }

    void OnRtnDepthMarketData(CThostFtdcDepthMarketDataField* pData) override {
        if (!running || pData == nullptr) {
            return;
        }

        output_type raw {};
        raw.data = *pData;
        if (!queue.push(raw)) {
            std::cerr << "[CtpApiSource] queue full, dropping raw tick for "
                      << pData->InstrumentID << std::endl;
        }
    }

    CtpApiSourceConfig config {};
    SpscQueue<output_type> queue;
    CThostFtdcMdApi* md_api {nullptr};
    std::atomic<bool> running {false};
};

CtpApiSource::CtpApiSource()
    : impl_(std::make_shared<Impl>(65536)) {}

CtpApiSource::~CtpApiSource() {
    stop();
}

bool CtpApiSource::init(const CtpApiSourceConfig& config) {
    if (impl_->queue.capacity() != config.queue_capacity) {
        impl_ = std::make_shared<Impl>(config.queue_capacity);
    }
    return impl_->init(config);
}

bool CtpApiSource::poll(output_type& out) {
    return impl_->poll(out);
}

void CtpApiSource::stop() {
    impl_->stop();
}

}  // namespace md_gateway
