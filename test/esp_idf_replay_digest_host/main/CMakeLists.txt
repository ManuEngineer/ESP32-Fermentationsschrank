set(ADAPTER_SOURCE_DIR
    "${CMAKE_CURRENT_LIST_DIR}/../../../lib/device_platform_esp_idf/src")

idf_component_register(
    SRCS
        "test_main.cpp"
        "${ADAPTER_SOURCE_DIR}/esp_idf_replay_digest.cpp"
    INCLUDE_DIRS
        "${ADAPTER_SOURCE_DIR}"
    REQUIRES
        device_platform
        mbedtls
)

target_compile_options(${COMPONENT_LIB} PRIVATE "-std=gnu++17")
