#include "md_gateway/source/pcap_live_source.hpp"

#include <atomic>
#include <cstring>
#include <iostream>
#include <thread>

#include <pcap.h>

#include "hft_common/base/queue.h"

namespace md_gateway {

struct PcapLiveSource::Impl {
    explicit Impl(std::size_t queue_capacity)
        : queue(queue_capacity) {}

    bool init(const PcapLiveSourceConfig& in_config) {
        if (running.exchange(true)) {
            return false;
        }
        config = in_config;

        char errbuf[PCAP_ERRBUF_SIZE] {};
        handle = pcap_open_live(config.interface.c_str(), config.snaplen,
                                config.promiscuous ? 1 : 0, config.timeout_ms, errbuf);
        if (!handle) {
            std::cerr << "[PcapLiveSource] pcap_open_live failed: " << errbuf << std::endl;
            running = false;
            return false;
        }

        if (!config.bpf_filter.empty()) {
            bpf_u_int32 net = 0;
            bpf_u_int32 mask = 0;
            if (pcap_lookupnet(config.interface.c_str(), &net, &mask, errbuf) == -1) {
                std::cerr << "[PcapLiveSource] pcap_lookupnet failed: " << errbuf
                          << ", using 0/0" << std::endl;
            }
            struct bpf_program fp {};
            if (pcap_compile(handle, &fp, config.bpf_filter.c_str(), 0, net) == -1) {
                std::cerr << "[PcapLiveSource] pcap_compile failed: " << pcap_geterr(handle) << std::endl;
                pcap_close(handle);
                handle = nullptr;
                running = false;
                return false;
            }
            if (pcap_setfilter(handle, &fp) == -1) {
                std::cerr << "[PcapLiveSource] pcap_setfilter failed: " << pcap_geterr(handle) << std::endl;
                pcap_freecode(&fp);
                pcap_close(handle);
                handle = nullptr;
                running = false;
                return false;
            }
            pcap_freecode(&fp);
        }

        worker = std::thread([this] { read_loop(); });
        return true;
    }

    bool poll(output_type& out) {
        return queue.pop(out);
    }

    void stop() {
        if (!running.exchange(false)) {
            return;
        }
        if (handle) {
            pcap_breakloop(handle);
        }
        if (worker.joinable()) {
            worker.join();
        }
        if (handle) {
            pcap_close(handle);
            handle = nullptr;
        }
    }

    void read_loop() {
        while (running.load()) {
            struct pcap_pkthdr* header = nullptr;
            const u_char* packet = nullptr;
            int ret = pcap_next_ex(handle, &header, &packet);

            if (ret == 0) {
                continue;
            }
            if (ret == -2) {
                break;
            }
            if (ret == -1) {
                std::cerr << "[PcapLiveSource] read error: " << pcap_geterr(handle) << std::endl;
                break;
            }
            if (!packet || !header) {
                continue;
            }

            output_type raw {};
            raw.length = header->caplen > sizeof(raw.data) ? sizeof(raw.data)
                                                           : static_cast<uint32_t>(header->caplen);
            std::memcpy(raw.data, packet, raw.length);
            raw.timestamp_sec = header->ts.tv_sec;
            raw.timestamp_usec = header->ts.tv_usec;

            if (!queue.push(raw)) {
                std::cerr << "[PcapLiveSource] queue full, dropping packet" << std::endl;
            }
        }
        running = false;
    }

    PcapLiveSourceConfig config {};
    SpscQueue<output_type> queue;
    std::atomic<bool> running {false};
    std::thread worker;
    pcap_t* handle {nullptr};
};

PcapLiveSource::PcapLiveSource()
    : impl_(std::make_shared<Impl>(65536)) {}

PcapLiveSource::~PcapLiveSource() {
    stop();
}

bool PcapLiveSource::init(const config_type& config) {
    if (impl_->queue.capacity() != config.queue_capacity) {
        impl_ = std::make_shared<Impl>(config.queue_capacity);
    }
    return impl_->init(config);
}

bool PcapLiveSource::poll(output_type& out) {
    return impl_->poll(out);
}

void PcapLiveSource::stop() {
    impl_->stop();
}

}  // namespace md_gateway
