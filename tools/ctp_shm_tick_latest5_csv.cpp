#include <chrono>
#include <cstring>
#include <deque>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <string>
#include <thread>

#include "hft_common/ipc/shm_ring_buffer.h"
#include "md_gateway/model/ctp_shm_tick_record.hpp"

namespace {

constexpr std::size_t kKeepCount = 5;

struct TickSnapshot {
    uint64_t seq {0};
    md_gateway::CtpShmTickRecord tick {};
};

std::string get_symbol(const md_gateway::CtpShmTickRecord& tick) {
    char symbol[sizeof(tick.symbol) + 1] {};
    std::memcpy(symbol, tick.symbol, sizeof(tick.symbol));
    return symbol;
}

void write_csv(const std::string& output_path,
               const std::map<std::string, std::deque<TickSnapshot>>& latest_by_symbol) {
    const std::string tmp_path = output_path + ".tmp";
    std::ofstream out(tmp_path, std::ios::trunc);
    if (!out.is_open()) {
        throw std::runtime_error("failed to open temp csv: " + tmp_path);
    }

    out << "symbol,rank,seq,trading_day,update_time,last_price,pre_settlement_price,settlement_price_valid,settlement_price,"
           "volume,turnover,open_interest,bid1_price,bid1_volume,ask1_price,ask1_volume\n";

    out << std::fixed << std::setprecision(2);
    for (const auto& [symbol, ticks] : latest_by_symbol) {
        std::size_t rank = 1;
        for (auto it = ticks.rbegin(); it != ticks.rend(); ++it, ++rank) {
            const auto& tick = it->tick;
            out << symbol << ','
                << rank << ','
                << it->seq << ','
                << tick.trading_day << ','
                << tick.update_time << ','
                << tick.last_price << ','
                << tick.pre_settlement_price << ','
                << static_cast<int>(tick.settlement_price_valid) << ','
                << tick.settlement_price << ','
                << tick.volume << ','
                << tick.turnover << ','
                << tick.open_interest << ','
                << tick.bid_price[0] << ','
                << tick.bid_volume[0] << ','
                << tick.ask_price[0] << ','
                << tick.ask_volume[0] << '\n';
        }
    }

    out.close();
    std::filesystem::rename(tmp_path, output_path);
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "usage: ctp_shm_tick_latest5_csv <shm_name> <output.csv>" << std::endl;
        return 1;
    }

    const std::string shm_name = argv[1];
    const std::string output_path = argv[2];

    try {
        hft_common::ipc::ShmRingBuffer<md_gateway::CtpShmTickRecord> reader(shm_name, false);
        std::map<std::string, std::deque<TickSnapshot>> latest_by_symbol;
        uint64_t next_seq = 1;
        auto last_flush = std::chrono::steady_clock::now();
        bool dirty = false;

        for (;;) {
            const uint64_t latest = reader.latest_seq();
            if (next_seq <= latest) {
                for (; next_seq <= latest; ++next_seq) {
                    const md_gateway::CtpShmTickRecord* tick = reader.read(next_seq);
                    if (!tick) {
                        continue;
                    }

                    auto& queue = latest_by_symbol[get_symbol(*tick)];
                    queue.push_back(TickSnapshot {.seq = next_seq, .tick = *tick});
                    if (queue.size() > kKeepCount) {
                        queue.pop_front();
                    }
                    dirty = true;
                }
            }

            const auto now = std::chrono::steady_clock::now();
            if (dirty && (next_seq > latest || now - last_flush >= std::chrono::milliseconds(200))) {
                write_csv(output_path, latest_by_symbol);
                last_flush = now;
                dirty = false;
            }

            if (next_seq > latest) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }
    } catch (const std::exception& ex) {
        std::cerr << "ctp_shm_tick_latest5_csv failed: " << ex.what() << std::endl;
        return 1;
    }
}
