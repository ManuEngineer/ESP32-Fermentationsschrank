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
// H-bridge adapter but never issues a direction command and never connects it
// to the actuator planner/driver. Only the Issue #29 bring-up probe (its own
// AllOff sinks) may define or call setForward/setReverse.
void test_main_never_calls_peltier_direction_commands() {
    std::size_t scanned = 0U;
    for (const auto& entry : std::filesystem::directory_iterator("main")) {
        if (!entry.is_regular_file()) continue;
        const auto extension = entry.path().extension().string();
        if (extension != ".cpp" && extension != ".hpp") continue;
        const auto name = entry.path().filename().string();
        if (name == "issue_29_bringup_probe.cpp") continue;
        ++scanned;
        const auto text = readSource(entry.path().string());
        TEST_ASSERT_FALSE(contains(text, "setForward("));
        TEST_ASSERT_FALSE(contains(text, "setReverse("));
    }
    TEST_ASSERT_TRUE(scanned >= 1U);
}

void test_app_main_composes_the_bridge_without_planner_connection() {
    const auto text = readSource("main/app_main.cpp");
    TEST_ASSERT_TRUE(contains(text, "EspIdfSharedEnableBridge"));
    TEST_ASSERT_TRUE(contains(text, "peltierBridge.begin()"));
    // Pins and polarities come from the generated SSOT header only.
    TEST_ASSERT_TRUE(contains(text, "r1_pins::kBtsEnablePin"));
    TEST_ASSERT_TRUE(contains(text, "r1_pins::kBtsRpwmPin"));
    TEST_ASSERT_TRUE(contains(text, "r1_pins::kBtsLpwmPin"));
    // No connection to the planner/driver and no ADC/R_IS/L_IS handling.
    TEST_ASSERT_FALSE(contains(text, "ActuatorPlanSinkDriver"));
    TEST_ASSERT_FALSE(contains(text, "ActuatorPlanner"));
    TEST_ASSERT_FALSE(contains(text, "adc_"));
    TEST_ASSERT_FALSE(contains(text, "kBtsRis"));
    TEST_ASSERT_FALSE(contains(text, "kBtsLis"));
}

}  // namespace

int main(int argc, char** argv) {
    static_cast<void>(argc);
    static_cast<void>(argv);
    UNITY_BEGIN();
    RUN_TEST(test_main_never_calls_peltier_direction_commands);
    RUN_TEST(test_app_main_composes_the_bridge_without_planner_connection);
    return UNITY_END();
}
