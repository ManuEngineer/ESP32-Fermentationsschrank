idf_component_register(
    SRCS
        "stateful_block_device.cpp"
        "nvs_api_fault_seam.cpp"
        "test_nvs_state_store_host.cpp"
        "../../../lib/device_platform_esp_idf/src/nvs_state_store.cpp"
    INCLUDE_DIRS
        "."
        "../../../lib/device_platform/src"
        "../../../lib/device_platform_esp_idf/src"
    REQUIRES nvs_flash
    PRIV_REQUIRES esp_partition spi_flash
)

target_compile_options(${COMPONENT_LIB} PRIVATE "-std=gnu++17")
target_link_libraries(${COMPONENT_LIB} INTERFACE
    "-Wl,--wrap=nvs_open_from_partition"
    "-Wl,--wrap=nvs_get_blob"
    "-Wl,--wrap=nvs_set_blob"
    "-Wl,--wrap=nvs_commit"
)
