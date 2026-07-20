#include <unity.h>

#include "app_config.hpp"

#include <cstring>

void test_project_metadata() {
    TEST_ASSERT_EQUAL_STRING("ESP32-Fermentationsschrank",
                             app_config::kProjectName);
    TEST_ASSERT_EQUAL_STRING("ManuEngineer", app_config::kProjectOwner);
    TEST_ASSERT_GREATER_THAN(0, std::strlen(app_config::kFirmwareVersion));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_project_metadata);
    return UNITY_END();
}
