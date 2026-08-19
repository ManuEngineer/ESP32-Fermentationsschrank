# The ESP-IDF component requirement pass runs before Kconfig variables are
# materialized. Read the two build-only switches from the cache/sdkconfig so
# harness-only requirements are still conditional in that pass.
set(APP_ISSUE_90_ENABLED OFF)
if(DEFINED ENV{APP_ISSUE_90_NVS_HARDWARE_TEST})
    set(APP_ISSUE_90_ENABLED ON)
endif()
if(DEFINED CACHE{APP_ISSUE_90_NVS_HARDWARE_TEST})
    get_property(APP_ISSUE_90_ENABLED CACHE APP_ISSUE_90_NVS_HARDWARE_TEST
        PROPERTY VALUE)
endif()
set(APP_PROFILE_BRINGUP_EARLY OFF)
if(DEFINED SDKCONFIG AND EXISTS "${SDKCONFIG}")
    file(STRINGS "${SDKCONFIG}" APP_PROFILE_BRINGUP_LINES
        REGEX "^CONFIG_APP_PROFILE_ESP32_BRINGUP=y$")
    if(APP_PROFILE_BRINGUP_LINES)
        set(APP_PROFILE_BRINGUP_EARLY ON)
    endif()
endif()

# Profilwahl kommt aus main/Kconfig.projbuild (choice APP_ESP32_PROFILE),
# gesetzt ueber sdkconfig.defaults.bringup/.release (siehe
# docs/tasks/issue-74-implementation-plan.md, Abschnitt 7.2). Genau eine
# der beiden CONFIG_APP_PROFILE_ESP32_*-Optionen darf aktiv sein; das ist
# bereits durch die Kconfig-"choice" erzwungen. Dieser Guard bricht
# zusaetzlich schon beim CMake-Konfigurationslauf ab, falls das dennoch
# nicht der Fall ist (z. B. widerspruechliche sdkconfig-Overlays), und
# ergaenzt damit den bestehenden `#error`-Vertrag in
# include/app_config.hpp, ersetzt ihn aber nicht.
if(NOT CMAKE_BUILD_EARLY_EXPANSION AND
        CONFIG_APP_PROFILE_ESP32_BRINGUP AND CONFIG_APP_PROFILE_ESP32_RELEASE)
    message(FATAL_ERROR
        "Exactly one ESP32 application profile must be active, "
        "both APP_PROFILE_ESP32_BRINGUP and APP_PROFILE_ESP32_RELEASE are set"
    )
elseif(NOT CMAKE_BUILD_EARLY_EXPANSION AND CONFIG_APP_PROFILE_ESP32_BRINGUP)
    set(APP_PROFILE_DEFINE "APP_PROFILE_ESP32_BRINGUP=1")
elseif(NOT CMAKE_BUILD_EARLY_EXPANSION AND CONFIG_APP_PROFILE_ESP32_RELEASE)
    set(APP_PROFILE_DEFINE "APP_PROFILE_ESP32_RELEASE=1")
elseif(NOT CMAKE_BUILD_EARLY_EXPANSION)
    message(FATAL_ERROR
        "Exactly one ESP32 application profile must be active, "
        "neither APP_PROFILE_ESP32_BRINGUP nor APP_PROFILE_ESP32_RELEASE is set"
    )
endif()

# The component is registered after the early profile checks so the
# test-path dependencies are present only for the bring-up harness.
set(APP_MAIN_PRIV_REQUIRES)
if(CONFIG_APP_PROFILE_ESP32_BRINGUP OR APP_PROFILE_BRINGUP_EARLY)
    list(APPEND APP_MAIN_PRIV_REQUIRES esp_driver_gpio)
endif()
if(APP_ISSUE_90_ENABLED)
    list(APPEND APP_MAIN_PRIV_REQUIRES
        esp_partition mbedtls nvs_flash esp_driver_uart spi_flash
    )
endif()

idf_component_register(
    SRCS "app_main.cpp"
    PRIV_INCLUDE_DIRS "../include"
    PRIV_REQUIRES device_platform fermentation_app device_platform_esp_idf
        ${APP_MAIN_PRIV_REQUIRES}
)

# ESP-IDF 6.0.2 kompiliert C++ standardmaessig mit -std=gnu++26; das Projekt
# haelt gnu++17 (siehe platformio.ini und
# docs/tasks/issue-72-implementation-plan.md, Abschnitt 5).
target_compile_options(${COMPONENT_LIB} PRIVATE "-std=gnu++17")

set(APP_ISSUE_29_COMPILE_DEFINITIONS)
set(APP_ISSUE_90_COMPILE_DEFINITIONS)
if(CONFIG_APP_PROFILE_ESP32_BRINGUP)
    target_sources(${COMPONENT_LIB} PRIVATE "issue_29_bringup_probe.cpp")
    set_source_files_properties(
        "issue_29_bringup_probe.cpp"
        PROPERTIES COMPILE_OPTIONS "-fstack-usage;-fcallgraph-info=su"
    )
    target_include_directories(${COMPONENT_LIB} PRIVATE
        "../lib/fermentation_app/private"
    )
    list(APPEND APP_ISSUE_29_COMPILE_DEFINITIONS
        APP_ISSUE_29_BRINGUP_PROBE=1
    )
    if(APP_ISSUE_90_NVS_HARDWARE_TEST)
        if(NOT APP_ISSUE_90_SOURCE_GIT_SHA OR NOT APP_ISSUE_90_PLAN_SHA)
            message(FATAL_ERROR
                "Issue-90 harness requires source and plan SHA definitions"
            )
        endif()
        target_sources(${COMPONENT_LIB} PRIVATE
            "issue_90_nvs_hardware_verification.cpp"
        )
        set_source_files_properties(
            "issue_90_nvs_hardware_verification.cpp"
            PROPERTIES COMPILE_OPTIONS "-fstack-usage;-fcallgraph-info=su"
        )
        list(APPEND APP_ISSUE_90_COMPILE_DEFINITIONS
            APP_ISSUE_90_NVS_HARDWARE_TEST=1
            "APP_ISSUE_90_SOURCE_GIT_SHA=\"${APP_ISSUE_90_SOURCE_GIT_SHA}\""
            "APP_ISSUE_90_PLAN_SHA=\"${APP_ISSUE_90_PLAN_SHA}\""
        )
    endif()
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
    ${APP_ISSUE_90_COMPILE_DEFINITIONS}
    APP_TARGET_FLASH_MB=4
    APP_REQUIRE_PSRAM=0
    APP_WEB_OTA_ENABLED=0
    APP_REAL_ACTUATORS_ENABLED=0
)
