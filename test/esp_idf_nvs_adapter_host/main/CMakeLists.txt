set(DEVICE_PLATFORM_ESP_IDF_SOURCE_DIR
    "${CMAKE_CURRENT_LIST_DIR}/../../../lib/device_platform_esp_idf/src"
)

# The Linux NVS host test uses these target-neutral production sources directly.
# It deliberately does not require the complete ESP-IDF adapter component,
# whose RTC/I2C dependencies support ESP32 targets only.
idf_component_register(
    SRCS
        "test_nvs_state_store.cpp"
        "bdl_ramdisk.cpp"
        "${DEVICE_PLATFORM_ESP_IDF_SOURCE_DIR}/nvs_state_store.cpp"
        "${DEVICE_PLATFORM_ESP_IDF_SOURCE_DIR}/esp_time_zone_resolver.cpp"
    INCLUDE_DIRS
        "."
        "${DEVICE_PLATFORM_ESP_IDF_SOURCE_DIR}"
    REQUIRES device_platform fermentation_app nvs_flash
    PRIV_REQUIRES esp_blockdev
)

target_compile_options(${COMPONENT_LIB} PRIVATE "-std=gnu++17")

# The wrappers are test-only fault stimuli. The production component contains
# no fault seam and no BDL dependency.
idf_build_set_property(
    LINK_OPTIONS
        "-Wl,--wrap=nvs_get_blob"
    APPEND
)
idf_build_set_property(
    LINK_OPTIONS
        "-Wl,--wrap=nvs_set_blob"
    APPEND
)
idf_build_set_property(
    LINK_OPTIONS
        "-Wl,--wrap=nvs_commit"
    APPEND
)
