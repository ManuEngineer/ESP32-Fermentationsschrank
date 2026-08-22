cmake_minimum_required(VERSION 3.22)

# This is a separate component consumer: it declares only the adapter
# component, not nvs_flash. The public nvs_flash dependency is therefore
# checked by the ESP-IDF component graph rather than by manually including
# the adapter source in a host test.
set(EXTRA_COMPONENT_DIRS
    "${CMAKE_CURRENT_LIST_DIR}/../../lib/device_platform"
    "${CMAKE_CURRENT_LIST_DIR}/../../lib/device_platform_esp_idf"
)
set(COMPONENTS main)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(issue90_nvs_adapter_consumer)
