cmake_minimum_required(VERSION 3.16)
project(hft_md_gateway CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(YAML_CPP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(YAML_CPP_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(YAML_CPP_INSTALL OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/yaml-cpp ${CMAKE_CURRENT_BINARY_DIR}/yaml-cpp-build)

find_path(PCAP_INCLUDE_DIR pcap.h)
find_library(PCAP_LIBRARY pcap)

add_library(md_gateway STATIC
    src/source/ctp_api_source.cpp
    src/source/pcap_replay_source.cpp
    src/source/pcap_live_source.cpp
)
target_include_directories(md_gateway
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/hft-common/include
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/ctp/include
        ${PCAP_INCLUDE_DIR}
)
target_link_libraries(md_gateway
    PUBLIC
        yaml-cpp
        pthread
        ${PCAP_LIBRARY}
)

set(CCAPI_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/ccapi/include")
if(NOT EXISTS "${CCAPI_INCLUDE_DIR}/ccapi_cpp/ccapi_session.h")
    find_path(CCAPI_INCLUDE_DIR ccapi_cpp/ccapi_session.h)
endif()

set(RAPIDJSON_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/rapidjson/include")
if(NOT EXISTS "${RAPIDJSON_INCLUDE_DIR}/rapidjson/document.h")
    find_path(RAPIDJSON_INCLUDE_DIR rapidjson/document.h)
endif()
if(CCAPI_INCLUDE_DIR AND RAPIDJSON_INCLUDE_DIR)
    find_package(Threads QUIET)
    find_package(Boost QUIET COMPONENTS system)
    find_package(OpenSSL QUIET)

    if(Threads_FOUND AND Boost_FOUND AND OPENSSL_FOUND)
        target_sources(md_gateway
            PRIVATE
                src/source/binance_ccapi_source.cpp
        )
        target_include_directories(md_gateway
            PUBLIC
                ${CCAPI_INCLUDE_DIR}
                ${RAPIDJSON_INCLUDE_DIR}
        )
        target_link_libraries(md_gateway
            PUBLIC
                Boost::system
                OpenSSL::SSL
                OpenSSL::Crypto
                Threads::Threads
        )
        target_compile_definitions(md_gateway
            PUBLIC
                CCAPI_ENABLE_SERVICE_MARKET_DATA
                CCAPI_ENABLE_EXCHANGE_BINANCE
                CCAPI_ENABLE_EXCHANGE_BINANCE_USDS_FUTURES
        )
    else()
        message(STATUS "Threads/Boost/OpenSSL not all found, binance_ccapi_source disabled")
    endif()
endif()

add_executable(ctp_md_demo
    src/app/ctp_dat_demo.cpp
)
target_link_directories(ctp_md_demo
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/ctp/lib
)
set_target_properties(ctp_md_demo PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bin
    BUILD_RPATH "${CMAKE_CURRENT_SOURCE_DIR}/third_party/ctp/lib"
    INSTALL_RPATH "${CMAKE_CURRENT_SOURCE_DIR}/third_party/ctp/lib"
)
target_link_libraries(ctp_md_demo
    PRIVATE
        md_gateway
        thostmduserapi_se
)

if(CCAPI_INCLUDE_DIR AND RAPIDJSON_INCLUDE_DIR)
    add_executable(binance_md_demo
        src/app/binance_dat_demo.cpp
    )
    set_target_properties(binance_md_demo PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bin
    )
    target_link_libraries(binance_md_demo
        PRIVATE
            md_gateway
    )
else()
    message(STATUS "CCAPI or RapidJSON headers not found, binance_md_demo target disabled")
endif()

add_executable(pcap_md_demo
    src/app/pcap_dat_demo.cpp
)
set_target_properties(pcap_md_demo PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bin
)
target_link_libraries(pcap_md_demo
    PRIVATE
        md_gateway
)

add_executable(pcap_live_demo
    src/app/pcap_live_demo.cpp
)
set_target_properties(pcap_live_demo PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bin
)
target_link_libraries(pcap_live_demo
    PRIVATE
        md_gateway
)
