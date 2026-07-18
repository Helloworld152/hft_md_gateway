add_executable(ctp_md_demo ctp_dat_demo.cpp)
target_link_directories(ctp_md_demo PRIVATE ${HFT_CTP_LIB_DIR})
target_link_libraries(ctp_md_demo PRIVATE md_gateway thostmduserapi_se)
set_target_properties(ctp_md_demo PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${HFT_BIN_DIR}
    BUILD_RPATH "${HFT_CTP_LIB_DIR}"
    INSTALL_RPATH "${HFT_CTP_LIB_DIR}"
)

add_executable(ctp_shm_demo ctp_shm_demo.cpp)
target_link_directories(ctp_shm_demo PRIVATE ${HFT_CTP_LIB_DIR})
target_link_libraries(ctp_shm_demo PRIVATE md_gateway thostmduserapi_se)
set_target_properties(ctp_shm_demo PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${HFT_BIN_DIR}
    BUILD_RPATH "${HFT_CTP_LIB_DIR}"
    INSTALL_RPATH "${HFT_CTP_LIB_DIR}"
)

add_executable(pcap_md_demo pcap_dat_demo.cpp)
target_link_libraries(pcap_md_demo PRIVATE md_gateway ${PCAP_LIBRARY})
set_target_properties(pcap_md_demo PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${HFT_BIN_DIR})

add_executable(pcap_live_demo pcap_live_demo.cpp)
target_link_libraries(pcap_live_demo PRIVATE md_gateway ${PCAP_LIBRARY})
set_target_properties(pcap_live_demo PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${HFT_BIN_DIR})

if(EFVI_LIBRARY)
    add_executable(efvi_md_demo efvi_dat_demo.cpp)
    target_link_libraries(efvi_md_demo PRIVATE md_gateway ${EFVI_LIBRARY})
    set_target_properties(efvi_md_demo PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${HFT_BIN_DIR})
endif()
