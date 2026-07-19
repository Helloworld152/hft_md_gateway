#include <chrono>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <thread>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include "hft_common/ipc/shm_ring_buffer.h"
#include "md_gateway/model/ctp_shm_tick_record.hpp"

namespace {

void print_usage() {
    std::cerr << "usage: ctp_shm_reader --shm <shm_name> [--start <seq>]" << std::endl;
}

bool parse_uint64(const char* text, uint64_t& value) {
    try {
        const std::string s(text);
        std::size_t pos = 0;
        const auto parsed = std::stoull(s, &pos);
        if (pos != s.size()) {
            return false;
        }
        value = static_cast<uint64_t>(parsed);
        return true;
    } catch (...) {
        return false;
    }
}

uint64_t get_data_shm_size(const std::string& shm_name) {
    const std::string data_name = shm_name + "-Data";
    const int fd = shm_open(data_name.c_str(), O_RDWR, 0666);
    if (fd < 0) {
        return 0;
    }

    struct stat st {};
    const int rc = fstat(fd, &st);
    close(fd);
    if (rc != 0 || st.st_size < 0) {
        return 0;
    }
    return static_cast<uint64_t>(st.st_size);
}

std::string format_bytes(uint64_t bytes) {
    std::ostringstream oss;
    oss << bytes << " B";
    if (bytes >= (1ULL << 20)) {
        oss << " (" << std::fixed << std::setprecision(2)
            << (static_cast<double>(bytes) / static_cast<double>(1ULL << 20)) << " MiB)";
    } else if (bytes >= (1ULL << 10)) {
        oss << " (" << std::fixed << std::setprecision(2)
            << (static_cast<double>(bytes) / static_cast<double>(1ULL << 10)) << " KiB)";
    }
    return oss.str();
}

void print_basic_info(const std::string& shm_name,
                      const hft_common::ipc::ShmRingBuffer<md_gateway::CtpShmTickRecord>& reader) {
    const uint64_t capacity = reader.get_capacity();
    const uint64_t latest = reader.latest_seq();
    const uint64_t oldest = latest >= capacity ? (latest - capacity + 1) : 1;
    const uint64_t readable_begin = latest == 0 ? 0 : oldest;
    const uint64_t readable_end = latest;
    const uint64_t data_size = get_data_shm_size(shm_name);

    std::cout << "SHM Info" << '\n'
              << "  shm_name          : " << shm_name << '\n'
              << "  capacity          : " << capacity << '\n'
              << "  data_size         : " << format_bytes(data_size) << '\n'
              << "  latest_seq        : " << latest << '\n'
              << "  oldest_readable   : " << readable_begin << '\n'
              << "  readable_seq_range: [" << readable_begin << ", " << readable_end << "]"
              << std::endl;
}

}  // namespace

int main(int argc, char** argv) {
    std::string shm_name;
    uint64_t start_seq = 0;
    bool has_start = false;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--shm") {
            if (i + 1 >= argc) {
                print_usage();
                return 1;
            }
            shm_name = argv[++i];
            continue;
        }
        if (arg == "--start") {
            if (i + 1 >= argc || !parse_uint64(argv[i + 1], start_seq) || start_seq == 0) {
                print_usage();
                return 1;
            }
            has_start = true;
            ++i;
            continue;
        }

        print_usage();
        return 1;
    }

    if (shm_name.empty()) {
        print_usage();
        return 1;
    }

    try {
        hft_common::ipc::ShmRingBuffer<md_gateway::CtpShmTickRecord> reader(shm_name, false);
        if (!has_start) {
            print_basic_info(shm_name, reader);
            return 0;
        }

        uint64_t next_seq = start_seq;

        for (;;) {
            const uint64_t latest = reader.latest_seq();
            if (next_seq > latest) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }

            for (; next_seq <= latest; ++next_seq) {
                const md_gateway::CtpShmTickRecord* tick = reader.read(next_seq);
                if (!tick) {
                    continue;
                }

                char symbol[sizeof(tick->symbol) + 1] {};
                std::memcpy(symbol, tick->symbol, sizeof(tick->symbol));

                std::cout << "[seq " << std::right << std::setw(8) << next_seq << "] "
                          << std::left << std::setw(10) << symbol
                          << " td=" << tick->trading_day
                          << " t=" << tick->update_time
                          << " last=" << std::fixed << std::setprecision(2) << tick->last_price
                          << " pre_settle=" << tick->pre_settlement_price
                          << " settle_valid=" << static_cast<int>(tick->settlement_price_valid)
                          << " settle=" << tick->settlement_price
                          << " vol=" << tick->volume
                          << " bid1=" << tick->bid_price[0] << " x " << tick->bid_volume[0]
                          << " ask1=" << tick->ask_price[0] << " x " << tick->ask_volume[0]
                          << std::endl;
            }
        }
    } catch (const std::exception& ex) {
        std::cerr << "ctp_shm_reader failed: " << ex.what() << std::endl;
        return 1;
    }
}
