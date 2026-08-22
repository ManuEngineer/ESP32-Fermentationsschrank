idf_component_register(
    SRCS
        "test_nvs_state_store.cpp"
        "bdl_ramdisk.cpp"
    INCLUDE_DIRS
        "."
    REQUIRES device_platform_esp_idf fermentation_app
    PRIV_REQUIRES esp_blockdev
)

target_compile_options(${COMPONENT_LIB} PRIVATE "-std=gnu++17")

# The wrappers are test-only fault stimuli. The production component contains
# no fault seam and no BDL dependency.
idf_build_set_property(
    LINK_OPTIONS
        "-Wl,--wrap=nvs_get_blob"
    APPEND
)
idf_build_set_property(
    LINK_OPTIONS
        "-Wl,--wrap=nvs_set_blob"
    APPEND
)
idf_build_set_property(
    LINK_OPTIONS
        "-Wl,--wrap=nvs_commit"
    APPEND
)
