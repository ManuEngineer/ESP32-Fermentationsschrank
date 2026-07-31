idf_component_register(
    SRCS "app_main.cpp"
    PRIV_INCLUDE_DIRS "../include"
    PRIV_REQUIRES device_platform fermentation_app device_platform_esp_idf
)

# ESP-IDF 6.0.2 kompiliert C++ standardmaessig mit -std=gnu++26; das Projekt
# haelt gnu++17 (siehe platformio.ini und
# docs/tasks/issue-72-implementation-plan.md, Abschnitt 5).
target_compile_options(${COMPONENT_LIB} PRIVATE "-std=gnu++17")

# Sichere Bring-up-Definitionen, textidentisch mit platformio.inis
# [env:esp32_bringup] (siehe docs/tasks/issue-73-implementation-plan.md,
# Abschnitt 14). Nur hier gesetzt, keine Verteilung auf mehrere Stellen.
target_compile_definitions(${COMPONENT_LIB} PRIVATE
    APP_PROFILE_ESP32_BRINGUP=1
    APP_TARGET_FLASH_MB=4
    APP_REQUIRE_PSRAM=0
    APP_WEB_OTA_ENABLED=0
    APP_REAL_ACTUATORS_ENABLED=0
)
