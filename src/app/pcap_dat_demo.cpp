#include <chrono>
#include <iostream>
#include <string>
#include <thread>

#include "md_gateway/decoder/pcap_tick_record_decoder.hpp"
#include "md_gateway/pipeline/market_data_pipeline.hpp"
#include "md_gateway/publisher/dat_tick_publisher.hpp"
#include "md_gateway/source/pcap_replay_source.hpp"

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: pcap_md_demo <config.yaml>" << std::endl;
        return 1;
    }

    const std::string config_path = argv[1];

    md_gateway::MarketDataPipeline<
        md_gateway::PcapReplaySource,
        md_gateway::PcapTickRecordDecoder,
        md_gateway::DatTickPublisher>
        pipeline(md_gateway::PcapReplaySource{},
                 md_gateway::PcapTickRecordDecoder{},
                 md_gateway::DatTickPublisher{});

    if (!pipeline.init(config_path)) {
        std::cerr << "failed to init pipeline" << std::endl;
        return 1;
    }

    for (;;) {
        pipeline.run_once();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}
