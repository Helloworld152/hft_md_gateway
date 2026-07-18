#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iostream>

#include "hft_common/net/udp_multicast_receiver.h"
#include "md_gateway/model/ctp_shm_tick_record.hpp"

namespace {

void print_tick(const md_gateway::CtpShmTickRecord& tick) {
    char symbol[sizeof(tick.symbol) + 1] {};
    std::memcpy(symbol, tick.symbol, sizeof(tick.symbol));

    std::cout << std::left << std::setw(10) << symbol
              << " td=" << tick.trading_day
              << " t=" << tick.update_time
              << " last=" << std::fixed << std::setprecision(2) << tick.last_price
              << " vol=" << tick.volume
              << " bid1=" << tick.bid_price[0] << " x " << tick.bid_volume[0]
              << " ask1=" << tick.ask_price[0] << " x " << tick.ask_volume[0]
              << std::endl;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "usage: shm2mcast_receiver <multicast_ip> <port> [interface_ip]" << std::endl;
        return 1;
    }

    const char* multicast_ip = argv[1];
    const uint16_t port = static_cast<uint16_t>(std::stoi(argv[2]));
    const char* interface_ip = argc >= 4 ? argv[3] : "0.0.0.0";

    try {
        hft_common::net::UdpMulticastReceiver receiver;
        receiver.init({multicast_ip, interface_ip, port, true});

        for (;;) {
            md_gateway::CtpShmTickRecord tick {};
            const ssize_t received = receiver.recv(&tick, sizeof(tick));
            if (received < 0) {
                perror("recv");
                return 1;
            }
            if (received != static_cast<ssize_t>(sizeof(tick))) {
                std::cerr << "ignored packet with unexpected size: " << received << std::endl;
                continue;
            }
            print_tick(tick);
        }
    } catch (const std::exception& ex) {
        std::cerr << "shm2mcast_receiver failed: " << ex.what() << std::endl;
        return 1;
    }
}
