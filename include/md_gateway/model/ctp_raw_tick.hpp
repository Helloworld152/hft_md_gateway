#pragma once

#include <cstdint>
#include "ThostFtdcUserApiStruct.h"

namespace md_gateway {

struct CtpRawTick {
    CThostFtdcDepthMarketDataField data {};
    uint64_t local_ts {0};
};

}  // namespace md_gateway
