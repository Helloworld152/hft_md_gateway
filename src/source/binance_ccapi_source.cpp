#include "md_gateway/source/binance_ccapi_source.hpp"

#include <atomic>
#include <chrono>
#include <cstring>
#include <map>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include "ccapi_cpp/ccapi_session.h"
#include "hft_common/base/queue.h"

namespace md_gateway {

namespace {

uint64_t to_epoch_ms(const ccapi::TimePoint& tp) {
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(tp.time_since_epoch()).count();
    return ms < 0 ? 0 : static_cast<uint64_t>(ms);
}

template <typename MapT>
bool try_get_double(const MapT& m, std::string_view key, double& out) {
    for (const auto& kv : m) {
        if (kv.first != key) {
            continue;
        }
        try {
            out = std::stod(kv.second);
            return true;
        } catch (...) {
            return false;
        }
    }
    return false;
}

template <typename MapT>
bool try_get_any_double(const MapT& m, const std::vector<std::string_view>& keys, double& out) {
    for (const auto& key : keys) {
        if (try_get_double(m, key, out)) {
            return true;
        }
    }
    return false;
}

bool is_perp_symbol(const std::string& symbol) {
    return symbol.size() > 5 && symbol.compare(symbol.size() - 5, 5, "_PERP") == 0;
}

std::string to_ccapi_symbol(const std::string& symbol) {
    return is_perp_symbol(symbol) ? symbol.substr(0, symbol.size() - 5) : symbol;
}

const char* to_exchange(const std::string& symbol) {
    return is_perp_symbol(symbol) ? "binance-usds-futures" : "binance";
}

}  // namespace

ccapi::Logger* ccapi::Logger::logger = nullptr;

struct BinanceCcapiSource::Impl {
    class EventHandler : public ccapi::EventHandler {
    public:
        explicit EventHandler(Impl* owner)
            : owner_(owner) {}

        void processEvent(const ccapi::Event& event, ccapi::Session* session) override {
            (void)session;
            owner_->handle_event(event);
        }

    private:
        Impl* owner_ {nullptr};
    };

    explicit Impl(std::size_t queue_capacity)
        : queue(queue_capacity) {}

    bool init(const BinanceCcapiSourceConfig& in_config) {
        if (running.exchange(true)) {
            return false;
        }
        config = in_config;
        worker = std::thread([this] { connect_loop(); });
        return true;
    }

    bool poll(BinanceCcapiSource::output_type& out) {
        return queue.pop(out);
    }

    void stop() {
        if (!running.exchange(false)) {
            return;
        }
        if (session) {
            session->stop();
        }
        if (worker.joinable()) {
            worker.join();
        }
    }

    void connect_loop() {
        ccapi::SessionOptions session_options;
        if (!config.proxy.empty()) {
            session_options.websocketConnectTimeoutMilliseconds = 30000;
        }
        ccapi::SessionConfigs session_configs;
        EventHandler event_handler(this);
        ccapi::Session current_session(session_options, session_configs, &event_handler);
        session = &current_session;

        std::vector<ccapi::Subscription> subscriptions;
        subscriptions.reserve(config.symbols.size());
        for (const auto& symbol : config.symbols) {
            const std::string ccapi_symbol = to_ccapi_symbol(symbol);
            const std::string cid = "md:" + symbol;
            if (config.proxy.empty()) {
                subscriptions.emplace_back(
                    to_exchange(symbol), ccapi_symbol, "MARKET_DEPTH", config.depth_options, cid);
            } else {
                std::map<std::string, std::string> credential;
                subscriptions.emplace_back(
                    to_exchange(symbol), ccapi_symbol, "MARKET_DEPTH", config.depth_options, cid,
                    credential, config.proxy);
            }
        }

        current_session.subscribe(subscriptions);
        while (running.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }

        current_session.stop();
        session = nullptr;
    }

    void handle_event(const ccapi::Event& event) {
        if (event.getType() != ccapi::Event::Type::SUBSCRIPTION_DATA) {
            return;
        }

        for (const auto& message : event.getMessageList()) {
            const auto& correlation_ids = message.getCorrelationIdList();
            if (correlation_ids.empty()) {
                continue;
            }
            const std::string& cid = correlation_ids.front();
            if (cid.rfind("md:", 0) != 0) {
                continue;
            }
            handle_depth_message(message, cid.substr(3));
        }
    }

    void handle_depth_message(const ccapi::Message& message, const std::string& symbol) {
        BinanceCcapiSource::output_type raw {};
        std::strncpy(raw.symbol, symbol.c_str(), sizeof(raw.symbol) - 1);
        raw.trading_day = config.trading_day;
        raw.event_time_ms = to_epoch_ms(message.getTime());

        std::vector<std::pair<double, double>> bids;
        std::vector<std::pair<double, double>> asks;
        bids.reserve(5);
        asks.reserve(5);

        for (const auto& element : message.getElementList()) {
            const auto& m = element.getNameValueMap();

            double bid_price = 0.0;
            double bid_size = 0.0;
            if (try_get_any_double(m, {"BID_PRICE", "PRICE"}, bid_price) &&
                try_get_any_double(m, {"BID_SIZE", "BID_QUANTITY", "SIZE", "QUANTITY"}, bid_size)) {
                bids.emplace_back(bid_price, bid_size);
            }

            double ask_price = 0.0;
            double ask_size = 0.0;
            if (try_get_any_double(m, {"ASK_PRICE", "PRICE"}, ask_price) &&
                try_get_any_double(m, {"ASK_SIZE", "ASK_QUANTITY", "SIZE", "QUANTITY"}, ask_size)) {
                asks.emplace_back(ask_price, ask_size);
            }
        }

        for (std::size_t i = 0; i < 5 && i < bids.size(); ++i) {
            raw.bid_price[i] = bids[i].first;
            raw.bid_size[i] = bids[i].second;
        }
        for (std::size_t i = 0; i < 5 && i < asks.size(); ++i) {
            raw.ask_price[i] = asks[i].first;
            raw.ask_size[i] = asks[i].second;
        }

        queue.push(raw);
    }

    BinanceCcapiSourceConfig config {};
    SpscQueue<BinanceCcapiSource::output_type> queue;
    std::atomic<bool> running {false};
    std::thread worker;
    ccapi::Session* session {nullptr};
};

BinanceCcapiSource::BinanceCcapiSource()
    : impl_(std::make_shared<Impl>(65536)) {}

BinanceCcapiSource::~BinanceCcapiSource() {
    stop();
}

bool BinanceCcapiSource::init(const config_type& config) {
    if (impl_->queue.capacity() != config.queue_capacity) {
        impl_ = std::make_shared<Impl>(config.queue_capacity);
    }
    return impl_->init(config);
}

bool BinanceCcapiSource::poll(output_type& out) {
    return impl_->poll(out);
}

void BinanceCcapiSource::stop() {
    impl_->stop();
}

}  // namespace md_gateway
