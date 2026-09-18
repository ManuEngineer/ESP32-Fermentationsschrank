idf_component_register(
    SRC_DIRS "src"
    INCLUDE_DIRS "src"
    REQUIRES device_platform nvs_flash
    PRIV_REQUIRES esp_event esp_http_server esp_netif esp_wifi mdns esp_timer
                  lwip esp-idf-lib__ds3231 esp-idf-lib__i2cdev
)

# ESP-IDF 6.1 kompiliert C++ standardmaessig mit -std=gnu++26; das Projekt
# haelt gnu++17 (siehe platformio.ini und
# docs/tasks/issue-72-implementation-plan.md, Abschnitt 5).
target_compile_options(${COMPONENT_LIB} PRIVATE "-std=gnu++17")
