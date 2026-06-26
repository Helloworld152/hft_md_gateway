#pragma once

#include <cstdint>

namespace md_gateway {

struct PcapRawFrame {
    static constexpr uint32_t kMaxLen = 4096;
    char data[kMaxLen] {};
    uint32_t length {0};
    uint64_t timestamp_sec {0};
    uint64_t timestamp_usec {0};
};

}  // namespace md_gateway
