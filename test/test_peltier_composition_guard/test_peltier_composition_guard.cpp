#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

#include <unity.h>

namespace {

std::string readSource(const std::string& path) {
    std::ifstream stream(path, std::ios::binary);
    TEST_ASSERT_TRUE(stream.good());
    return std::string(std::istreambuf_iterator<char>(stream),
                       std::istreambuf_iterator<char>());
}

bool contains(const std::string& text, const char* token) {
    return text.find(token) != std::string::npos;
}

// SIM-33-guard: the product composition (main/) creates and starts the
// H-bridge adapter but never issues a direction command itself. Since #218 the
// bridge reaches the planner/driver only through the application-owned chain
// (attachActuationPorts, see the dedicated test below). Only the Issue #29
// bring-up probe (its own AllOff sinks) and the Issue #33 manual bridge idle
// harness (bring-up-only, see the dedicated tests below) may define or call
// setForward/setReverse.
void test_main_never_calls_peltier_direction_commands() {
    std::size_t scanned = 0U;
    for (const auto& entry : std::filesystem::directory_iterator("main")) {
        if (!entry.is_regular_file()) continue;
        const auto extension = entry.path().extension().string();
        if (extension != ".cpp" && extension != ".hpp") continue;
        const auto name = entry.path().filename().string();
        if (name == "issue_29_bringup_probe.cpp") continue;
        if (name == "issue_33_bridge_idle_harness.cpp") continue;
        ++scanned;
        const auto text = readSource(entry.path().string());
        TEST_ASSERT_FALSE(contains(text, "setForward("));
        TEST_ASSERT_FALSE(contains(text, "setReverse("));
    }
    TEST_ASSERT_TRUE(scanned >= 1U);
}

void test_app_main_composes_the_bridge_without_direct_planner_use() {
    const auto text = readSource("main/app_main.cpp");
    TEST_ASSERT_TRUE(contains(text, "EspIdfSharedEnableBridge"));
    TEST_ASSERT_TRUE(contains(text, "peltierBridge.begin()"));
    // Pins and polarities come from the generated SSOT header only.
    TEST_ASSERT_TRUE(contains(text, "r1_pins::kBtsEnablePin"));
    TEST_ASSERT_TRUE(contains(text, "r1_pins::kBtsRpwmPin"));
    TEST_ASSERT_TRUE(contains(text, "r1_pins::kBtsLpwmPin"));
    // No direct planner/driver use and no ADC/R_IS/L_IS handling.
    TEST_ASSERT_FALSE(contains(text, "ActuatorPlanSinkDriver"));
    TEST_ASSERT_FALSE(contains(text, "ActuatorPlanner"));
    TEST_ASSERT_FALSE(contains(text, "adc_"));
    TEST_ASSERT_FALSE(contains(text, "kBtsRis"));
    TEST_ASSERT_FALSE(contains(text, "kBtsLis"));
}

// SIM-218-guard: the only way the product reaches planner/driver is the single
// attachActuationPorts() call with the named unconfigured parameter constant
// (until #35). It is excluded in the builds of the manual output harnesses
// (#32/#33), which drive the same sinks directly, and no main/ file uses the
// orchestrator, the interlock or a regulator directly.
void test_control_chain_is_attached_only_via_application_ports() {
    const auto text = readSource("main/app_main.cpp");
    TEST_ASSERT_TRUE(contains(text, "application.attachActuationPorts("));
    TEST_ASSERT_TRUE(
        contains(text, "kControlChainParametersUnconfiguredUntilIssue35"));
    // The attach call sits inside a guard that excludes both manual output
    // harness builds (formatting-independent token check on the code before
    // it).
    const auto attach = text.find("application.attachActuationPorts(");
    TEST_ASSERT_TRUE(attach != std::string::npos);
    const auto before = text.substr(attach >= 700U ? attach - 700U : 0U,
                                    attach >= 700U ? 700U : attach);
    const auto guardPosition =
        before.rfind("#if !defined(APP_ISSUE_32_OUTPUT_PULSE)");
    TEST_ASSERT_TRUE(guardPosition != std::string::npos);
    TEST_ASSERT_TRUE(contains(before.substr(guardPosition).c_str(),
                              "!defined(APP_ISSUE_33_BRIDGE_IDLE_TEST)"));
    TEST_ASSERT_TRUE(
        contains(text, "kSensorQualityConfigUnconfiguredUntilIssue35"));
    for (const auto& entry : std::filesystem::directory_iterator("main")) {
        if (!entry.is_regular_file()) continue;
        const auto extension = entry.path().extension().string();
        if (extension != ".cpp" && extension != ".hpp") continue;
        const auto name = entry.path().filename().string();
        if (name == "issue_29_bringup_probe.cpp") continue;
        const auto source = readSource(entry.path().string());
        TEST_ASSERT_FALSE(
            contains(source, "TemperatureControlApplicationOrchestrator"));
        TEST_ASSERT_FALSE(contains(source, "ActuationInterlock"));
        TEST_ASSERT_FALSE(contains(source, "TemperatureController"));
    }
}

// SIM-33-guard (Issue #33): the one allowed direction caller is the manual
// bring-up-only idle harness. It is fully wrapped in its bring-up build flag,
// has no planner/driver and no GPIO control of its own (only the existing
// bridge port), and appears in the build only inside the dedicated bring-up
// block. app_main includes it only under the same flag.
void test_bridge_idle_harness_is_bringup_only_and_planner_free() {
    for (const char* path : {"main/issue_33_bridge_idle_harness.hpp",
                             "main/issue_33_bridge_idle_harness.cpp"}) {
        const auto text = readSource(path);
        TEST_ASSERT_TRUE(
            contains(text, "#if defined(APP_ISSUE_33_BRIDGE_IDLE_TEST)"));
        TEST_ASSERT_FALSE(contains(text, "ActuatorPlanSinkDriver"));
        TEST_ASSERT_FALSE(contains(text, "ActuatorPlanner"));
        TEST_ASSERT_FALSE(contains(text, "gpio_set_level"));
        TEST_ASSERT_FALSE(contains(text, "gpio_config"));
        TEST_ASSERT_FALSE(contains(text, "adc_"));
        TEST_ASSERT_FALSE(contains(text, "kBtsRis"));
        TEST_ASSERT_FALSE(contains(text, "kBtsLis"));
        const auto end = text.rfind("#endif");
        TEST_ASSERT_TRUE(end != std::string::npos);
        // Nothing but whitespace after the closing #endif of the flag guard.
        TEST_ASSERT_TRUE(text.find_first_not_of(" \t\r\n", end + 6U) ==
                         std::string::npos);
    }
}

void test_bridge_idle_harness_is_built_and_included_only_with_its_flag() {
    const auto cmake = readSource("main/CMakeLists.txt");
    const auto block = cmake.find("if(APP_ISSUE_33_BRIDGE_IDLE_TEST)");
    const auto source = cmake.find("issue_33_bridge_idle_harness.cpp");
    TEST_ASSERT_TRUE(block != std::string::npos);
    TEST_ASSERT_TRUE(source != std::string::npos);
    TEST_ASSERT_TRUE(source > block);
    // Listed exactly once and only inside the dedicated bring-up-only block.
    TEST_ASSERT_TRUE(cmake.find("issue_33_bridge_idle_harness.cpp",
                                source + 1U) == std::string::npos);
    TEST_ASSERT_TRUE(contains(
        cmake, "Issue #33 bridge idle test is allowed only in esp32_bringup"));
    const auto app = readSource("main/app_main.cpp");
    const auto include =
        app.find("#include \"issue_33_bridge_idle_harness.hpp\"");
    TEST_ASSERT_TRUE(include != std::string::npos);
    TEST_ASSERT_TRUE(app.rfind("#ifdef APP_ISSUE_33_BRIDGE_IDLE_TEST",
                               include) != std::string::npos);
    TEST_ASSERT_TRUE(
        include - app.rfind("#ifdef APP_ISSUE_33_BRIDGE_IDLE_TEST", include) <
        80U);
}

}  // namespace

int main(int argc, char** argv) {
    static_cast<void>(argc);
    static_cast<void>(argv);
    UNITY_BEGIN();
    RUN_TEST(test_main_never_calls_peltier_direction_commands);
    RUN_TEST(test_app_main_composes_the_bridge_without_direct_planner_use);
    RUN_TEST(test_control_chain_is_attached_only_via_application_ports);
    RUN_TEST(test_bridge_idle_harness_is_bringup_only_and_planner_free);
    RUN_TEST(test_bridge_idle_harness_is_built_and_included_only_with_its_flag);
    return UNITY_END();
}
