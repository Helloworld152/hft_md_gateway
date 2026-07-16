#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <string>

#include "hft_common/ipc/shm_ring_buffer.h"
#include "md_gateway/model/ctp_shm_tick_record.hpp"

namespace {

struct SettlementSnapshot {
    uint64_t seq {0};
    md_gateway::CtpShmTickRecord tick {};
};

struct SettlementSummary {
    SettlementSnapshot first {};
    SettlementSnapshot latest {};
    bool has_value {false};
};

std::string get_symbol(const md_gateway::CtpShmTickRecord& tick) {
    char symbol[sizeof(tick.symbol) + 1] {};
    std::memcpy(symbol, tick.symbol, sizeof(tick.symbol));
    return symbol;
}

bool has_valid_settlement(const md_gateway::CtpShmTickRecord& tick) noexcept {
    return tick.settlement_price_valid != 0;
}

void write_csv(const std::string& output_path,
               const std::map<std::string, SettlementSummary>& summary_by_symbol) {
    std::ofstream out(output_path, std::ios::trunc);
    if (!out.is_open()) {
        throw std::runtime_error("failed to open output csv: " + output_path);
    }

    out << "symbol,first_seq,first_update_time,first_settlement_price_valid,first_settlement_price,"
           "latest_seq,latest_update_time,latest_settlement_price_valid,latest_settlement_price,latest_pre_settlement_price\n";
    out << std::fixed << std::setprecision(2);

    for (const auto& [symbol, summary] : summary_by_symbol) {
        if (!summary.has_value) {
            continue;
        }
        out << symbol << ','
            << summary.first.seq << ','
            << summary.first.tick.update_time << ','
            << static_cast<int>(summary.first.tick.settlement_price_valid) << ','
            << summary.first.tick.settlement_price << ','
            << summary.latest.seq << ','
            << summary.latest.tick.update_time << ','
            << static_cast<int>(summary.latest.tick.settlement_price_valid) << ','
            << summary.latest.tick.settlement_price << ','
            << summary.latest.tick.pre_settlement_price << '\n';
    }
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "usage: ctp_shm_settlement_summary <shm_name> <output.csv>" << std::endl;
        return 1;
    }

    const std::string shm_name = argv[1];
    const std::string output_path = argv[2];

    try {
        hft_common::ipc::ShmRingBuffer<md_gateway::CtpShmTickRecord> reader(shm_name, false);
        const uint64_t latest = reader.latest_seq();
        const uint64_t capacity = reader.get_capacity();
        const uint64_t begin = latest > capacity ? latest - capacity + 1 : 1;

        std::map<std::string, SettlementSummary> summary_by_symbol;
        for (uint64_t seq = begin; seq <= latest; ++seq) {
            const md_gateway::CtpShmTickRecord* tick = reader.read(seq);
            if (!tick || !has_valid_settlement(*tick)) {
                continue;
            }

            auto& summary = summary_by_symbol[get_symbol(*tick)];
            if (!summary.has_value) {
                summary.first = SettlementSnapshot {.seq = seq, .tick = *tick};
                summary.has_value = true;
            }
            summary.latest = SettlementSnapshot {.seq = seq, .tick = *tick};
        }

        write_csv(output_path, summary_by_symbol);
        std::cout << "wrote " << summary_by_symbol.size()
                  << " symbol summaries from seq " << begin
                  << " to " << latest << std::endl;
    } catch (const std::exception& ex) {
        std::cerr << "ctp_shm_settlement_summary failed: " << ex.what() << std::endl;
        return 1;
    }

    return 0;
}
