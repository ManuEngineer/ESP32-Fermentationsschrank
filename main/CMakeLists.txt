# Die Component muss registriert werden, bevor die aus Kconfig erzeugten
# Profilvariablen fuer die private Quellenisolierung verwendet werden.
idf_component_register(
    SRCS "app_main.cpp"
    PRIV_INCLUDE_DIRS "../include"
    PRIV_REQUIRES device_platform fermentation_app device_platform_esp_idf
                  nvs_flash
)

# ESP-IDF 6.0.2 kompiliert C++ standardmaessig mit -std=gnu++26; das Projekt
# haelt gnu++17 (siehe platformio.ini und
# docs/tasks/issue-72-implementation-plan.md, Abschnitt 5).
target_compile_options(${COMPONENT_LIB} PRIVATE "-std=gnu++17")

# The #121 static product-stack gate reuses the existing GCC stack-usage and
# callgraph artefacts.  This is analysis-only instrumentation; it does not
# change the product code, profile semantics, or configured task stack. Clang
# is used by the separate ESP-IDF analysis build and does not accept GCC's
# callgraph option.
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set_source_files_properties(
        "app_main.cpp"
        PROPERTIES COMPILE_OPTIONS "-fstack-usage;-fcallgraph-info=su"
    )
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

# Embed the exact checked-out source commit in the firmware metadata log. This
# is provenance only; it does not alter product behavior or persistence.
execute_process(
    COMMAND git rev-parse HEAD
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    OUTPUT_VARIABLE APP_SOURCE_GIT_SHA
    OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE APP_SOURCE_GIT_SHA_RESULT
)
string(LENGTH "${APP_SOURCE_GIT_SHA}" APP_SOURCE_GIT_SHA_LENGTH)
if(NOT APP_SOURCE_GIT_SHA_RESULT EQUAL 0 OR NOT APP_SOURCE_GIT_SHA_LENGTH EQUAL 40)
    message(FATAL_ERROR
        "Could not determine the exact 40-character application source SHA"
    )
endif()
set(APP_SOURCE_GIT_SHA_DEFINE "APP_SOURCE_GIT_SHA=\"${APP_SOURCE_GIT_SHA}\"")

set(APP_ISSUE_29_COMPILE_DEFINITIONS)
if(CONFIG_APP_PROFILE_ESP32_BRINGUP AND NOT APP_ISSUE_90_SLICE7_HARNESS)
    # The Issue #90 Slice-7 harness is a separate, independent hardware
    # runner and must not automatically compile or run the unrelated
    # Issue #29 diagnostic probe (real-hardware panic observed in the
    # probe's own resource-sampling code, tracked in #29; unaffected by
    # and unrelated to the Slice-7 harness). The normal esp32_bringup
    # path (without the harness flag) is unchanged.
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
endif()

# Issue #90 Slice-7 is a private, bring-up-only hardware runner.  It is
# enabled only by the dedicated harness build command; release and native
# builds do not compile or link any of its sources.
set(APP_ISSUE_90_SLICE7_COMPILE_DEFINITIONS)
if(APP_ISSUE_90_SLICE7_HARNESS)
    if(NOT CONFIG_APP_PROFILE_ESP32_BRINGUP)
        message(FATAL_ERROR
            "Issue #90 Slice-7 harness is allowed only in esp32_bringup"
        )
    endif()
    # The harness reads the console UART without changing the normal app
    # component graph.  The dedicated harness build makes these optional
    # components available; the release and native graphs do not.
    idf_component_optional_requires(PRIVATE esp_driver_uart vfs)
    target_sources(
        ${COMPONENT_LIB}
        PRIVATE "issue_90_slice7_harness.cpp"
    )
    list(APPEND APP_ISSUE_90_SLICE7_COMPILE_DEFINITIONS
        APP_ISSUE_90_SLICE7_HARNESS=1
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
    ${APP_ISSUE_90_SLICE7_COMPILE_DEFINITIONS}
    ${APP_SOURCE_GIT_SHA_DEFINE}
    APP_TARGET_FLASH_MB=4
    APP_REQUIRE_PSRAM=0
    APP_WEB_OTA_ENABLED=0
    APP_REAL_ACTUATORS_ENABLED=0
)
