#include <unity.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "../../main/fermentation_ui_renderer.hpp"

namespace {

class RecordingDisplay final : public device_platform::IDisplayTouchPort {
   public:
    bool initialize() override { return true; }
    bool setRotation(device_platform::DisplayRotation) override { return true; }
    bool setBacklight(bool) override { return true; }
    bool fillRect(device_platform::DisplayRect rect,
                  std::uint16_t) override {
        if (rect.left + rect.width > 320U || rect.top + rect.height > 240U ||
            rect.width == 0U || rect.height == 0U) {
            return false;
        }
        ++calls;
        pixels += static_cast<std::size_t>(rect.width) * rect.height;
        return true;
    }
    std::optional<device_platform::RawTouchSample> sampleTouch() override {
        return std::nullopt;
    }

    std::size_t calls{0U};
    std::size_t pixels{0U};
};

void test_representative_screen_uses_existing_workspace_and_three_locales() {
    fermentation::FermentationUiSnapshot snapshot;
    snapshot.home.mode = fermentation::FermentationHomeMode::Standby;
    snapshot.service.available = false;
    fermentation::FermentationTouchWorkspace workspace;
    const auto packs = fermentation::makeFermentationUiTextPacks();

    const auto de = fermentation::main_ui::makeRepresentativeScreen(
        snapshot, workspace, packs, device_platform::LocaleId{"de"});
    const auto en = fermentation::main_ui::makeRepresentativeScreen(
        snapshot, workspace, packs, device_platform::LocaleId{"en"});
    const auto es = fermentation::main_ui::makeRepresentativeScreen(
        snapshot, workspace, packs, device_platform::LocaleId{"es"});

    TEST_ASSERT_EQUAL(de.commands.size(), en.commands.size());
    TEST_ASSERT_EQUAL(de.commands.size(), es.commands.size());
    TEST_ASSERT_EQUAL_STRING("Bereit", de.commands[3].text.c_str());
    TEST_ASSERT_EQUAL_STRING("Ready", en.commands[3].text.c_str());
    TEST_ASSERT_EQUAL_STRING("Listo", es.commands[3].text.c_str());
    TEST_ASSERT_TRUE(de.workspace.bottomSlots[0].enabled);
    TEST_ASSERT_TRUE(de.workspace.bottomSlots[0].label.valid());
}

void test_lean_flush_is_bounded_and_bottom_press_returns_existing_target() {
    fermentation::FermentationUiSnapshot snapshot;
    snapshot.home.mode = fermentation::FermentationHomeMode::Standby;
    fermentation::FermentationTouchWorkspace workspace;
    const auto screen = fermentation::main_ui::makeRepresentativeScreen(
        snapshot, workspace, fermentation::makeFermentationUiTextPacks(),
        device_platform::LocaleId{"en"});
    RecordingDisplay display;
    const auto result =
        fermentation::main_ui::renderLean(display, screen);
    TEST_ASSERT_TRUE(result.success);
    TEST_ASSERT_TRUE(result.drawCommands > 0U);
    TEST_ASSERT_TRUE(result.textCommands > 0U);
    TEST_ASSERT_TRUE(result.textBytes > 0U);
    TEST_ASSERT_TRUE(display.calls > 0U);
    TEST_ASSERT_TRUE(display.pixels > 0U);

    const auto target = fermentation::main_ui::targetAt(screen, 20U, 220U);
    TEST_ASSERT_TRUE(target.has_value());
    TEST_ASSERT_EQUAL(static_cast<int>(device_platform::DeviceUiTargetKind::BottomSlot),
                      static_cast<int>(target->kind));
    TEST_ASSERT_EQUAL_UINT8(0U, target->slotIndex);

    const auto press = fermentation::main_ui::routePress(
        workspace, snapshot, screen, 20U, 220U);
    TEST_ASSERT_NOT_EQUAL(
        static_cast<int>(device_platform::DeviceUiInteractionOutcome::Ignored),
        static_cast<int>(press.interaction.outcome));
}

}  // namespace

// The native test target does not compile the ESP-IDF main component.  Include
// this small app-owned helper and its existing UI-contract implementations
// here so the renderer-independent contract is covered without making
// production code depend on test support.
#include "../../lib/device_platform/src/device_ui_interaction.cpp"
#include "../../lib/device_platform/src/device_ui_text.cpp"
#include "../../lib/fermentation_app/src/fermentation_touch_workspace.cpp"
#include "../../lib/fermentation_app/src/fermentation_ui_text.cpp"
#include "../../main/fermentation_ui_renderer.cpp"

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_representative_screen_uses_existing_workspace_and_three_locales);
    RUN_TEST(test_lean_flush_is_bounded_and_bottom_press_returns_existing_target);
    return UNITY_END();
}
