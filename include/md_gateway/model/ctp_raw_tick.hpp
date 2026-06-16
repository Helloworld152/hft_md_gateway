#pragma once

#include "ThostFtdcUserApiStruct.h"

namespace md_gateway {

struct CtpRawTick {
    CThostFtdcDepthMarketDataField data {};
};

}  // namespace md_gateway
