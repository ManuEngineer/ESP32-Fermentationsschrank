idf_component_register(
    SRC_DIRS "src"
    INCLUDE_DIRS "src"
    REQUIRES device_platform nvs_flash
    PRIV_REQUIRES esp_event esp_http_server esp_netif esp_wifi mdns esp_timer
                  freertos
                  lwip esp-idf-lib__ds3231 esp-idf-lib__i2cdev esp_driver_gpio
                  esp_driver_spi esp_lcd espressif__esp_lcd_ili9341
                  espressif__esp_lcd_touch atanisoft__esp_lcd_touch_xpt2046
)

# ESP-IDF 6.1 kompiliert C++ standardmaessig mit -std=gnu++26; das Projekt
# haelt gnu++17 (siehe platformio.ini und
# docs/tasks/issue-72-implementation-plan.md, Abschnitt 5).
target_compile_options(${COMPONENT_LIB} PRIVATE "-std=gnu++17")
