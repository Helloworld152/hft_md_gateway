#include <chrono>
#include <iostream>
#include <string>
#include <thread>

#include "md_gateway/decoder/ctp_shm_tick_record_decoder.hpp"
#include "md_gateway/model/ctp_shm_tick_record.hpp"
#include "md_gateway/pipeline/market_data_pipeline.hpp"
#include "md_gateway/publisher/shm_publisher.hpp"
#include "md_gateway/source/ctp_api_source.hpp"

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: md_gateway_ctp_shm_demo <config.yaml>" << std::endl;
        return 1;
    }

    const std::string config_path = argv[1];

    md_gateway::MarketDataPipeline<
        md_gateway::CtpApiSource,
        md_gateway::CtpShmTickRecordDecoder,
        md_gateway::ShmPublisher<md_gateway::CtpShmTickRecord>>
        pipeline(md_gateway::CtpApiSource{},
                 md_gateway::CtpShmTickRecordDecoder{},
                 md_gateway::ShmPublisher<md_gateway::CtpShmTickRecord>{});

    if (!pipeline.init(config_path)) {
        std::cerr << "failed to init pipeline" << std::endl;
        return 1;
    }

    for (;;) {
        pipeline.run_once();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}
