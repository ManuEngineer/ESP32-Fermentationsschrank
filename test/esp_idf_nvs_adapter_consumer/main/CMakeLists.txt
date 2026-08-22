idf_component_register(
    SRCS "consumer_compile.cpp"
    REQUIRES device_platform_esp_idf
)

target_compile_options(${COMPONENT_LIB} PRIVATE "-std=gnu++17")
