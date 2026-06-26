#pragma once

#include <algorithm>
#include <cstdint>
#include <ctime>
#include <cstdio>
#include <cstring>
#include <string>

#include <yaml-cpp/yaml.h>

#include "hft_common/protocol/protocol.h"
#include "md_gateway/model/binance_depth_raw_tick.hpp"

namespace md_gateway {

class BinanceDepthRecordDecoder {
public:
    using output_type = CryptoTickRecord;

    struct Config {};
    using config_type = Config;

    bool init(const Config&) {
        return true;
    }

    bool decode(const BinanceDepthRawTick& in, CryptoTickRecord& out) const noexcept {
        std::memset(&out, 0, sizeof(out));
        std::strncpy(out.symbol, in.symbol, sizeof(out.symbol) - 1);
        out.symbol_id = 0;
        out.trading_day = in.trading_day;
        out.update_time = epoch_ms_to_hhmmssmmm_utc(in.event_time_ms);

        std::copy(std::begin(in.bid_price), std::end(in.bid_price), std::begin(out.bid_price));
        std::copy(std::begin(in.bid_size), std::end(in.bid_size), std::begin(out.bid_size));
        std::copy(std::begin(in.ask_price), std::end(in.ask_price), std::begin(out.ask_price));
        std::copy(std::begin(in.ask_size), std::end(in.ask_size), std::begin(out.ask_size));
        return true;
    }

private:
    static uint64_t epoch_ms_to_hhmmssmmm_utc(uint64_t epoch_ms) noexcept {
        std::time_t seconds = static_cast<std::time_t>(epoch_ms / 1000);
        std::tm tm_utc {};
        gmtime_r(&seconds, &tm_utc);
        const uint64_t ms = epoch_ms % 1000;
        const uint64_t hhmmss = static_cast<uint64_t>(tm_utc.tm_hour) * 10000
            + static_cast<uint64_t>(tm_utc.tm_min) * 100
            + static_cast<uint64_t>(tm_utc.tm_sec);
        return hhmmss * 1000 + ms;
    }
};

inline bool load_component_config(const std::string&, BinanceDepthRecordDecoder::Config&) {
    return true;
}

}  // namespace md_gateway
