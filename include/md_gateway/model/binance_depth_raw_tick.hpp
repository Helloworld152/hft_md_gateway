#pragma once

#include <cstdint>

namespace md_gateway {

struct BinanceDepthRawTick {
    char symbol[32] {};
    uint32_t trading_day {0};
    uint64_t event_time_ms {0};
    double bid_price[5] {};
    double bid_size[5] {};
    double ask_price[5] {};
    double ask_size[5] {};
};

}  // namespace md_gateway
