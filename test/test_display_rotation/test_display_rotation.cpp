#include <unity.h>

#include "../../lib/device_platform_esp_idf/src/esp_idf_display_touch_adapter.hpp"

namespace {

using device_platform::DisplayRotation;
using device_platform_esp_idf::DisplayRotationTransform;

void assertTransform(DisplayRotation rotation,
                     DisplayRotationTransform expected) {
    const auto actual =
        device_platform_esp_idf::displayRotationTransform(rotation);
    TEST_ASSERT_EQUAL_UINT8(expected.swap_xy, actual.swap_xy);
    TEST_ASSERT_EQUAL_UINT8(expected.mirror_x, actual.mirror_x);
    TEST_ASSERT_EQUAL_UINT8(expected.mirror_y, actual.mirror_y);
}

void test_rotate0_uses_native_panel_geometry() {
    assertTransform(DisplayRotation::Rotate0, {false, false, false});
}

void test_rotate90_uses_r1_landscape_geometry() {
    assertTransform(DisplayRotation::Rotate90, {true, true, false});
}

void test_rotate180_mirrors_without_axis_swap() {
    assertTransform(DisplayRotation::Rotate180, {false, true, true});
}

void test_rotate270_uses_opposite_landscape_geometry() {
    assertTransform(DisplayRotation::Rotate270, {true, false, true});
}

}  // namespace

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_rotate0_uses_native_panel_geometry);
    RUN_TEST(test_rotate90_uses_r1_landscape_geometry);
    RUN_TEST(test_rotate180_mirrors_without_axis_swap);
    RUN_TEST(test_rotate270_uses_opposite_landscape_geometry);
    return UNITY_END();
}
