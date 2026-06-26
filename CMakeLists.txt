cmake_minimum_required(VERSION 3.16)
project(hft_md_gateway CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(YAML_CPP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(YAML_CPP_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(YAML_CPP_INSTALL OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/yaml-cpp ${CMAKE_CURRENT_BINARY_DIR}/yaml-cpp-build)

find_library(PCAP_LIBRARY pcap REQUIRED)

add_library(md_gateway INTERFACE)
target_include_directories(md_gateway INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/hft-common/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/ctp/include
)
target_link_libraries(md_gateway INTERFACE yaml-cpp pthread)

add_executable(ctp_md_demo src/app/ctp_dat_demo.cpp)
target_link_directories(ctp_md_demo PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/third_party/ctp/lib)
target_link_libraries(ctp_md_demo PRIVATE md_gateway thostmduserapi_se)
set_target_properties(ctp_md_demo PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bin
    BUILD_RPATH "${CMAKE_CURRENT_SOURCE_DIR}/third_party/ctp/lib"
    INSTALL_RPATH "${CMAKE_CURRENT_SOURCE_DIR}/third_party/ctp/lib"
)

add_executable(pcap_md_demo src/app/pcap_dat_demo.cpp)
target_link_libraries(pcap_md_demo PRIVATE md_gateway ${PCAP_LIBRARY})
set_target_properties(pcap_md_demo PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bin)

add_executable(pcap_live_demo src/app/pcap_live_demo.cpp)
target_link_libraries(pcap_live_demo PRIVATE md_gateway ${PCAP_LIBRARY})
set_target_properties(pcap_live_demo PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bin)

find_library(EFVI_LIBRARY ciul)
if(EFVI_LIBRARY)
    add_executable(efvi_md_demo src/app/efvi_dat_demo.cpp)
    target_link_libraries(efvi_md_demo PRIVATE md_gateway ${EFVI_LIBRARY})
    set_target_properties(efvi_md_demo PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bin)
endif()
