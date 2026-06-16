#pragma once

#include <type_traits>
#include <tuple>
#include <string>
#include <utility>

namespace md_gateway {

namespace detail {

template <typename T, typename = void>
struct has_stop : std::false_type {};

template <typename T>
struct has_stop<T, std::void_t<decltype(std::declval<T&>().stop())>> : std::true_type {};

template <typename T>
void stop_if_supported(T& value) {
    if constexpr (has_stop<T>::value) {
        value.stop();
    }
}

}  // namespace detail

template <typename Source, typename Decoder, typename... Publishers>
class MarketDataPipeline {
public:
    using Input = typename Source::output_type;
    using Output = typename Decoder::output_type;

    MarketDataPipeline(Source source, Decoder decoder, Publishers... publishers)
        : source_(std::move(source)),
          decoder_(std::move(decoder)),
          publishers_(std::move(publishers)...) {}

    bool init(const std::string& config_path) {
        typename Source::config_type source_config {};
        typename Decoder::config_type decoder_config {};
        if (!load_component_config(config_path, source_config) ||
            !load_component_config(config_path, decoder_config)) {
            return false;
        }
        return init_publishers_from_path(config_path, source_config, decoder_config,
                                         std::index_sequence_for<Publishers...>{});
    }

    template <typename SourceConfig, typename DecoderConfig, typename... PublisherConfigs>
    bool init(const SourceConfig& source_config,
              const DecoderConfig& decoder_config,
              const PublisherConfigs&... publisher_configs) {
        static_assert(sizeof...(Publishers) == sizeof...(PublisherConfigs),
                      "publisher config count must match publisher count");
        if (!source_.init(source_config) || !decoder_.init(decoder_config)) {
            return false;
        }
        return init_publishers(std::index_sequence_for<Publishers...>{}, publisher_configs...);
    }

    void run_once() {
        Input input {};
        if (!source_.poll(input)) {
            return;
        }

        Output output {};
        if (!decoder_.decode(input, output)) {
            return;
        }
        publish_all(output);
    }

    void run() {
        while (running_) {
            run_once();
        }
    }

    void stop() {
        running_ = false;
        detail::stop_if_supported(source_);
    }

private:
    template <std::size_t I>
    auto load_publisher_config_from_path(const std::string& config_path) {
        using Publisher = std::tuple_element_t<I, std::tuple<Publishers...>>;
        typename Publisher::config_type config {};
        load_component_config(config_path, config);
        return config;
    }

    template <std::size_t... I>
    bool init_publishers_from_path(const std::string& config_path,
                                   const typename Source::config_type& source_config,
                                   const typename Decoder::config_type& decoder_config,
                                   std::index_sequence<I...>) {
        return init(source_config, decoder_config, load_publisher_config_from_path<I>(config_path)...);
    }

    template <std::size_t... I, typename... PublisherConfigs>
    bool init_publishers(std::index_sequence<I...>, const PublisherConfigs&... publisher_configs) {
        return (std::get<I>(publishers_).init(publisher_configs) && ...);
    }

    void publish_all(const Output& output) {
        std::apply(
            [&](auto&... publisher) {
                (publisher.publish(output), ...);
            },
            publishers_);
    }

    Source source_;
    Decoder decoder_;
    std::tuple<Publishers...> publishers_;
    bool running_ {true};
};

}  // namespace md_gateway
