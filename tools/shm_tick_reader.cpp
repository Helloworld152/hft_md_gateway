#include <chrono>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <thread>

#include "hft_common/ipc/shm_ring_buffer.h"
#include "hft_common/protocol/protocol.h"

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: shm_tick_reader <shm_name>" << std::endl;
        return 1;
    }

    try {
        hft_common::ipc::ShmRingBuffer<TickRecord> reader(argv[1], false);
        uint64_t next_seq = 1;

        for (;;) {
            const uint64_t latest = reader.latest_seq();
            if (next_seq > latest) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }

            for (; next_seq <= latest; ++next_seq) {
                const TickRecord* tick = reader.read(next_seq);
                if (!tick) {
                    continue;
                }

                char symbol[sizeof(tick->symbol) + 1] {};
                std::memcpy(symbol, tick->symbol, sizeof(tick->symbol));

                std::cout << "[seq " << std::setw(8) << next_seq << "] "
                          << std::left << std::setw(10) << symbol
                          << " td=" << tick->trading_day
                          << " t=" << tick->update_time
                          << " last=" << std::fixed << std::setprecision(2) << tick->last_price
                          << " vol=" << tick->volume
                          << " bid1=" << tick->bid_price[0] << " x " << tick->bid_volume[0]
                          << " ask1=" << tick->ask_price[0] << " x " << tick->ask_volume[0]
                          << std::endl;
            }
        }
    } catch (const std::exception& ex) {
        std::cerr << "shm_tick_reader failed: " << ex.what() << std::endl;
        return 1;
    }
}
