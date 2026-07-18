cmake_minimum_required(VERSION 3.16)
project(hft_md_gateway CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(YAML_CPP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(YAML_CPP_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(YAML_CPP_INSTALL OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/yaml-cpp ${CMAKE_CURRENT_BINARY_DIR}/yaml-cpp-build)

find_library(PCAP_LIBRARY pcap REQUIRED)
find_library(EFVI_LIBRARY ciul)

set(HFT_BIN_DIR ${CMAKE_CURRENT_SOURCE_DIR}/bin)
set(HFT_CTP_LIB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/ctp/lib)

add_library(md_gateway INTERFACE)
target_include_directories(md_gateway INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/hft-common/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/ctp/include
)
target_link_libraries(md_gateway INTERFACE yaml-cpp pthread)

add_subdirectory(src/app)
add_subdirectory(data_transform)
add_subdirectory(tools)
