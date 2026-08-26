idf_component_register(
    SRC_DIRS "src"
    INCLUDE_DIRS "src"
    REQUIRES device_platform
)

# ESP-IDF 6.0.2 kompiliert C++ standardmaessig mit -std=gnu++26; das Projekt
# haelt gnu++17 (siehe platformio.ini und
# docs/tasks/issue-72-implementation-plan.md, Abschnitt 5).
target_compile_options(${COMPONENT_LIB} PRIVATE "-std=gnu++17")

# The #121 static product-stack gate reuses the existing GCC stack-usage and
# callgraph artefacts for the product-reachable lifecycle and persistence
# paths.  These flags are analysis-only and do not alter product semantics,
# profile selection, or the configured task stack.  Clang's separate ESP-IDF
# analysis build does not accept GCC's callgraph option.
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set_source_files_properties(
        "src/fermentation_application.cpp"
        "src/program_document_codec.cpp"
        "src/program_model.cpp"
        "src/process_state_machine.cpp"
        "src/run_commands.cpp"
        "src/run_persistence_contract.cpp"
        "src/run_persistence_codec.cpp"
        "src/run_persistence_coordinator.cpp"
        "src/run_snapshot.cpp"
        "src/temperature_control_orchestrator.cpp"
        PROPERTIES COMPILE_OPTIONS "-fstack-usage;-fcallgraph-info=su"
    )
endif()

# Der #29-Fehler-Seam bleibt ein privates, bring-up-only Compile-Artefakt.
# Native und esp32_release erhalten weder die Definition noch den privaten
# Include-Pfad; die Fachkomponente behaelt damit ihre portable oeffentliche
# Oberflaeche und keine ESP-IDF-/Profilabhaengigkeit.
if(CONFIG_APP_PROFILE_ESP32_BRINGUP)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE
        APP_ISSUE_29_BRINGUP_PROBE=1
    )
    target_include_directories(${COMPONENT_LIB} PRIVATE "private")
endif()
