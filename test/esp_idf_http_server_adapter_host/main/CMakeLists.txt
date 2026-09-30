set(ADAPTER_SOURCE_DIR
    "${CMAKE_CURRENT_LIST_DIR}/../../../lib/device_platform_esp_idf/src")

idf_component_register(
    SRCS
        "test_main.cpp"
        "${ADAPTER_SOURCE_DIR}/esp_idf_http_server_lifecycle.cpp"
    INCLUDE_DIRS
        "${ADAPTER_SOURCE_DIR}"
    REQUIRES
        device_platform
        esp_http_server
        lwip
)

target_compile_options(${COMPONENT_LIB} PRIVATE "-std=gnu++17")
# Keep the real ESP-IDF server on an unprivileged local port in this host test.
idf_build_set_property(LINK_OPTIONS "-Wl,--wrap=httpd_start" APPEND)
idf_build_set_property(LINK_OPTIONS "-Wl,--wrap=httpd_get_raw_req_data" APPEND)
