cmake_minimum_required(VERSION 3.22)

set(EXTRA_COMPONENT_DIRS
    "${CMAKE_CURRENT_LIST_DIR}/../../lib/device_platform"
    "${CMAKE_CURRENT_LIST_DIR}/../../lib/fermentation_app"
)
set(COMPONENTS main)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(issue90_nvs_adapter_host)
