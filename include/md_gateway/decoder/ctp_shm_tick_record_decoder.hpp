#pragma once

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

#include <yaml-cpp/yaml.h>

#include "md_gateway/model/ctp_raw_tick.hpp"
#include "md_gateway/model/ctp_shm_tick_record.hpp"

namespace md_gateway {

class CtpShmTickRecordDecoder {
public:
    using output_type = CtpShmTickRecord;

    struct Config {
        uint32_t trading_day {0};
    };
    using config_type = Config;

    bool init(const Config& config) {
        config_ = config;
        return true;
    }

    bool decode(const CtpRawTick& in, CtpShmTickRecord& out) const noexcept {
        const auto& data = in.data;

        int hh = 0;
        int mm = 0;
        int ss = 0;
        if (std::sscanf(data.UpdateTime, "%d:%d:%d", &hh, &mm, &ss) != 3) {
            return false;
        }

        std::memset(&out, 0, sizeof(out));
        out.update_time = (static_cast<uint64_t>(hh) * 10000 + mm * 100 + ss) * 1000 + data.UpdateMillisec;
        std::strncpy(out.symbol, data.InstrumentID, sizeof(out.symbol) - 1);
        out.symbol_id = 0;
        out.trading_day = config_.trading_day;
        if (data.TradingDay[0] != '\0') {
            out.trading_day = static_cast<uint32_t>(std::stoi(data.TradingDay));
        }

        assign_valid_price(data.LastPrice, out.last_price);
        out.volume = data.Volume;
        out.turnover = data.Turnover;
        out.open_interest = data.OpenInterest;
        assign_valid_price(data.UpperLimitPrice, out.upper_limit);
        assign_valid_price(data.LowerLimitPrice, out.lower_limit);
        assign_valid_price(data.OpenPrice, out.open_price);
        assign_valid_price(data.HighestPrice, out.highest_price);
        assign_valid_price(data.LowestPrice, out.lowest_price);
        assign_valid_price(data.PreClosePrice, out.pre_close_price);
        assign_valid_price(data.PreSettlementPrice, out.pre_settlement_price);
        out.settlement_price_valid = assign_optional_price(data.SettlementPrice, out.settlement_price) ? 1 : 0;

        assign_valid_price(data.BidPrice1, out.bid_price[0]);
        out.bid_volume[0] = data.BidVolume1;
        assign_valid_price(data.BidPrice2, out.bid_price[1]);
        out.bid_volume[1] = data.BidVolume2;
        assign_valid_price(data.BidPrice3, out.bid_price[2]);
        out.bid_volume[2] = data.BidVolume3;
        assign_valid_price(data.BidPrice4, out.bid_price[3]);
        out.bid_volume[3] = data.BidVolume4;
        assign_valid_price(data.BidPrice5, out.bid_price[4]);
        out.bid_volume[4] = data.BidVolume5;

        assign_valid_price(data.AskPrice1, out.ask_price[0]);
        out.ask_volume[0] = data.AskVolume1;
        assign_valid_price(data.AskPrice2, out.ask_price[1]);
        out.ask_volume[1] = data.AskVolume2;
        assign_valid_price(data.AskPrice3, out.ask_price[2]);
        out.ask_volume[2] = data.AskVolume3;
        assign_valid_price(data.AskPrice4, out.ask_price[3]);
        out.ask_volume[3] = data.AskVolume4;
        assign_valid_price(data.AskPrice5, out.ask_price[4]);
        out.ask_volume[4] = data.AskVolume5;
        return true;
    }

private:
    static bool is_valid_price(double price) noexcept {
        return price > 1e-6 && price < 1e300;
    }

    static void assign_valid_price(double src, double& dst) noexcept {
        if (is_valid_price(src)) {
            dst = src;
        }
    }

    static bool assign_optional_price(double src, double& dst) noexcept {
        if (src > -1e300 && src < 1e300) {
            dst = src;
            return true;
        }
        return false;
    }

    Config config_ {};
};

inline bool load_component_config(const std::string& config_path, CtpShmTickRecordDecoder::Config& config) {
    YAML::Node doc = YAML::LoadFile(config_path);
    YAML::Node source = doc["source"] ? doc["source"] : doc;

    if (source["trading_day"]) {
        config.trading_day = static_cast<uint32_t>(std::stoul(source["trading_day"].as<std::string>()));
    }
    return true;
}

}  // namespace md_gateway
