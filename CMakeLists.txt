cmake_minimum_required(VERSION 3.22)

# Nur die beiden konkreten, bereits vorhandenen portablen Komponenten
# registrieren; lib/ wird nicht pauschal aufgenommen und
# lib/device_platform_test_support/ bleibt dadurch strukturell ausserhalb
# des ESP-IDF-Produktbuilds (siehe ADR-013 und
# docs/tasks/issue-72-implementation-plan.md).
set(EXTRA_COMPONENT_DIRS
    "${CMAKE_CURRENT_LIST_DIR}/lib/device_platform"
    "${CMAKE_CURRENT_LIST_DIR}/lib/fermentation_app"
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
idf_build_set_property(MINIMAL_BUILD ON)
project(esp32_fermentationsschrank)

# Fail-fast: #72 ist ausschliesslich gegen den gepinnten Release v6.0.2
# geprueft. IDF_VER (statt IDF_VERSION_MAJOR/MINOR/PATCH) unterscheidet auch
# Release-/RC-/Beta-/Dev-Staende mit sonst gleichen Versionszahlen.
idf_build_get_property(idf_ver IDF_VER)
if(NOT idf_ver STREQUAL "v6.0.2")
    message(FATAL_ERROR "ESP-IDF v6.0.2 required, active version is '${idf_ver}'")
endif()
