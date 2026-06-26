#include "md_gateway/source/pcap_replay_source.hpp"

#include <atomic>
#include <cstring>
#include <iostream>
#include <thread>

#include <pcap.h>

#include "hft_common/base/queue.h"

namespace md_gateway {

struct PcapReplaySource::Impl {
    explicit Impl(std::size_t queue_capacity)
        : queue(queue_capacity) {}

    bool init(const PcapReplaySourceConfig& in_config) {
        if (running.exchange(true)) {
            return false;
        }
        config = in_config;

        char errbuf[PCAP_ERRBUF_SIZE] {};
        handle = pcap_open_offline(config.pcap_file.c_str(), errbuf);
        if (!handle) {
            std::cerr << "[PcapReplaySource] pcap_open_offline failed: " << errbuf << std::endl;
            running = false;
            return false;
        }

        if (!config.bpf_filter.empty()) {
            bpf_u_int32 net = 0;
            bpf_u_int32 mask = 0;
            struct bpf_program fp {};
            if (pcap_compile(handle, &fp, config.bpf_filter.c_str(), 0, net) == -1) {
                std::cerr << "[PcapReplaySource] pcap_compile failed: " << pcap_geterr(handle) << std::endl;
                pcap_close(handle);
                handle = nullptr;
                running = false;
                return false;
            }
            if (pcap_setfilter(handle, &fp) == -1) {
                std::cerr << "[PcapReplaySource] pcap_setfilter failed: " << pcap_geterr(handle) << std::endl;
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
                if (config.repeat) {
                    char errbuf[PCAP_ERRBUF_SIZE] {};
                    pcap_close(handle);
                    handle = pcap_open_offline(config.pcap_file.c_str(), errbuf);
                    if (!handle) {
                        std::cerr << "[PcapReplaySource] reopen failed: " << errbuf << std::endl;
                        break;
                    }
                    if (!config.bpf_filter.empty()) {
                        bpf_u_int32 net = 0;
                        struct bpf_program fp {};
                        if (pcap_compile(handle, &fp, config.bpf_filter.c_str(), 0, net) != -1) {
                            pcap_setfilter(handle, &fp);
                            pcap_freecode(&fp);
                        }
                    }
                    continue;
                }
                break;
            }
            if (ret == -1) {
                std::cerr << "[PcapReplaySource] read error: " << pcap_geterr(handle) << std::endl;
                break;
            }
            if (!packet || !header) {
                continue;
            }

            output_type raw {};
            raw.length = header->caplen > sizeof(raw.data) ? sizeof(raw.data) : static_cast<uint32_t>(header->caplen);
            std::memcpy(raw.data, packet, raw.length);
            raw.timestamp_sec = header->ts.tv_sec;
            raw.timestamp_usec = header->ts.tv_usec;

            if (!queue.push(raw)) {
                std::cerr << "[PcapReplaySource] queue full, dropping packet" << std::endl;
            }
        }
        running = false;
    }

    PcapReplaySourceConfig config {};
    SpscQueue<output_type> queue;
    std::atomic<bool> running {false};
    std::thread worker;
    pcap_t* handle {nullptr};
};

PcapReplaySource::PcapReplaySource()
    : impl_(std::make_shared<Impl>(65536)) {}

PcapReplaySource::~PcapReplaySource() {
    stop();
}

bool PcapReplaySource::init(const config_type& config) {
    if (impl_->queue.capacity() != config.queue_capacity) {
        impl_ = std::make_shared<Impl>(config.queue_capacity);
    }
    return impl_->init(config);
}

bool PcapReplaySource::poll(output_type& out) {
    return impl_->poll(out);
}

void PcapReplaySource::stop() {
    impl_->stop();
}

}  // namespace md_gateway
