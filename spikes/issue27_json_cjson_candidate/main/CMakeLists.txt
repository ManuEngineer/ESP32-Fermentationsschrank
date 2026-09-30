idf_component_register(SRCS "cjson_candidate_probe.cpp" PRIV_REQUIRES cjson)

target_compile_options(${COMPONENT_LIB} PRIVATE "-std=gnu++17")
