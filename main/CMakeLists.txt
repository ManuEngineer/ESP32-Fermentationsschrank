# Temporaerer Build-only-Compile-/Linkanker fuer Issue #72. Startet keine
# Anwendung, initialisiert keine Hardware und ruft keine Fachlogik auf.
# Wird in Issue #73 durch den echten ESP-IDF-Composition-Root ersetzt.
idf_component_register(
    SRCS "issue72_build_stub.c"
    PRIV_REQUIRES device_platform fermentation_app
)
