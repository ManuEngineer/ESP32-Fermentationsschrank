# Die Component muss registriert werden, bevor die aus Kconfig erzeugten
# Profilvariablen fuer die private Quellenisolierung verwendet werden.
idf_component_register(
    SRCS "app_main.cpp"
    PRIV_INCLUDE_DIRS "../include"
    PRIV_REQUIRES device_platform fermentation_app device_platform_esp_idf
)

# ESP-IDF 6.0.2 kompiliert C++ standardmaessig mit -std=gnu++26; das Projekt
# haelt gnu++17 (siehe platformio.ini und
# docs/tasks/issue-72-implementation-plan.md, Abschnitt 5).
target_compile_options(${COMPONENT_LIB} PRIVATE "-std=gnu++17")

# Profilwahl kommt aus main/Kconfig.projbuild (choice APP_ESP32_PROFILE),
# gesetzt ueber sdkconfig.defaults.bringup/.release (siehe
# docs/tasks/issue-74-implementation-plan.md, Abschnitt 7.2). Genau eine
# der beiden CONFIG_APP_PROFILE_ESP32_*-Optionen darf aktiv sein; das ist
# bereits durch die Kconfig-"choice" erzwungen. Dieser Guard bricht
# zusaetzlich schon beim CMake-Konfigurationslauf ab, falls das dennoch
# nicht der Fall ist (z. B. widerspruechliche sdkconfig-Overlays), und
# ergaenzt damit den bestehenden `#error`-Vertrag in
# include/app_config.hpp, ersetzt ihn aber nicht.
if(CONFIG_APP_PROFILE_ESP32_BRINGUP AND CONFIG_APP_PROFILE_ESP32_RELEASE)
    message(FATAL_ERROR
        "Exactly one ESP32 application profile must be active, "
        "both APP_PROFILE_ESP32_BRINGUP and APP_PROFILE_ESP32_RELEASE are set"
    )
elseif(CONFIG_APP_PROFILE_ESP32_BRINGUP)
    set(APP_PROFILE_DEFINE "APP_PROFILE_ESP32_BRINGUP=1")
elseif(CONFIG_APP_PROFILE_ESP32_RELEASE)
    set(APP_PROFILE_DEFINE "APP_PROFILE_ESP32_RELEASE=1")
else()
    message(FATAL_ERROR
        "Exactly one ESP32 application profile must be active, "
        "neither APP_PROFILE_ESP32_BRINGUP nor APP_PROFILE_ESP32_RELEASE is set"
    )
endif()

set(APP_ISSUE_29_COMPILE_DEFINITIONS)
if(CONFIG_APP_PROFILE_ESP32_BRINGUP)
    target_sources(${COMPONENT_LIB} PRIVATE "issue_29_bringup_probe.cpp")
    target_include_directories(${COMPONENT_LIB} PRIVATE
        "../lib/fermentation_app/private"
    )
    list(APPEND APP_ISSUE_29_COMPILE_DEFINITIONS
        APP_ISSUE_29_BRINGUP_PROBE=1
    )
endif()

# Firmwarefeste Release-1-Grenzen bleiben feste Compile-Definitionen, keine
# Kconfig-Menuoptionen (siehe docs/tasks/issue-74-implementation-plan.md,
# Abschnitt 7.2 Punkt 4). Textidentisch mit platformio.inis
# [env:esp32_bringup]/[env:esp32_release] (siehe
# docs/tasks/issue-73-implementation-plan.md, Abschnitt 14). Nur hier
# gesetzt, keine Verteilung auf mehrere Stellen.
target_compile_definitions(${COMPONENT_LIB} PRIVATE
    ${APP_PROFILE_DEFINE}
    ${APP_ISSUE_29_COMPILE_DEFINITIONS}
    APP_TARGET_FLASH_MB=4
    APP_REQUIRE_PSRAM=0
    APP_WEB_OTA_ENABLED=0
    APP_REAL_ACTUATORS_ENABLED=0
)
