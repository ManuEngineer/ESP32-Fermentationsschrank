idf_component_register(
    SRC_DIRS "src"
    INCLUDE_DIRS "src"
)

# ESP-IDF 6.0.2 kompiliert C++ standardmaessig mit -std=gnu++26; das Projekt
# haelt gnu++17 (siehe platformio.ini und
# docs/tasks/issue-72-implementation-plan.md, Abschnitt 5).
target_compile_options(${COMPONENT_LIB} PRIVATE "-std=gnu++17")

# Issue #29 (docs/tasks/issue-29-panic-requalification-correction-plan.md,
# Abschnitt 4.1.1): der erweiterte Diagnose-Stack-Gate-Analyzer traversiert
# den vollstaendigen vom Bring-up-Probe erreichbaren Callgraph; dieser
# erreicht device_platform-Funktionen (u. a. decodeEnvelope/encodeEnvelope/
# computeCrc32IsoHdlc). Analysis-only, wie bereits fuer fermentation_app
# (#121) angewendet; aendert keine Portsemantik oder Produktverhalten. Clangs
# separater ESP-IDF-Analysebuild akzeptiert GCCs Callgraph-Option nicht.
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(${COMPONENT_LIB} PRIVATE
        -fstack-usage
        -fcallgraph-info=su
    )
endif()
