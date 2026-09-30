set(FERMENTATION_APP_SRC_DIR
    "${CMAKE_CURRENT_LIST_DIR}/../../../lib/fermentation_app/src")

idf_component_register(
    SRCS "cjson_candidate_probe.cpp"
         "${FERMENTATION_APP_SRC_DIR}/configuration_text.cpp"
    INCLUDE_DIRS "${FERMENTATION_APP_SRC_DIR}"
    PRIV_REQUIRES cjson)

target_compile_options(${COMPONENT_LIB} PRIVATE "-std=gnu++17")
