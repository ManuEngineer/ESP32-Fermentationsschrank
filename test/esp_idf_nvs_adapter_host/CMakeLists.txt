cmake_minimum_required(VERSION 3.22)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
set(COMPONENTS main)
list(APPEND EXTRA_COMPONENT_DIRS "$ENV{IDF_PATH}/tools/mocks/freertos/")

idf_build_set_property(COMPILE_DEFINITIONS "NO_DEBUG_STORAGE" APPEND)
project(issue_90_nvs_adapter_host)
