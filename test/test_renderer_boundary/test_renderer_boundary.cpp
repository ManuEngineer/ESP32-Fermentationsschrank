#include <unity.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

#include "../../main/fermentation_ui_renderer.hpp"

namespace {

bool hasText(const fermentation::main_ui::RepresentativeScreen& screen,
             std::string_view text) {
    return std::any_of(screen.commands.begin(), screen.commands.end(),
                       [text](const auto& command) {
                           return command.text == text;
                       });
}

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
    TEST_ASSERT_EQUAL_STRING("DE", de.commands[3].text.c_str());
    TEST_ASSERT_EQUAL_STRING("EN", en.commands[3].text.c_str());
    TEST_ASSERT_EQUAL_STRING("ES", es.commands[3].text.c_str());
    TEST_ASSERT_EQUAL_UINT16(168U, de.commands[2].rect.width);
    TEST_ASSERT_EQUAL_UINT16(24U, de.commands[2].rect.height);
    TEST_ASSERT_EQUAL_UINT16(80U, de.commands.back().rect.width);
    TEST_ASSERT_EQUAL_UINT16(200U, de.commands.back().rect.top);
    TEST_ASSERT_EQUAL_STRING(
        "assets/branding/manuengineer/ManuEngineer.svg",
        de.commands[2].assetPath.c_str());
    TEST_ASSERT_TRUE(de.workspace.bottomSlots[0].enabled);
    TEST_ASSERT_TRUE(de.workspace.bottomSlots[0].label.valid());
}

void test_bottom_press_returns_existing_target() {
    fermentation::FermentationUiSnapshot snapshot;
    snapshot.home.mode = fermentation::FermentationHomeMode::Standby;
    fermentation::FermentationTouchWorkspace workspace;
    const auto screen = fermentation::main_ui::makeRepresentativeScreen(
        snapshot, workspace, fermentation::makeFermentationUiTextPacks(),
        device_platform::LocaleId{"en"});
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

void test_empty_home_omits_empty_pager_and_messages_pager_is_rendered() {
    fermentation::FermentationUiSnapshot homeSnapshot;
    homeSnapshot.home.mode = fermentation::FermentationHomeMode::Standby;
    fermentation::FermentationTouchWorkspace homeWorkspace;
    const auto packs = fermentation::makeFermentationUiTextPacks();
    const auto home = fermentation::main_ui::makeRepresentativeScreen(
        homeSnapshot, homeWorkspace, packs, device_platform::LocaleId{"en"});
    TEST_ASSERT_FALSE(hasText(home, "1/0"));

    auto messagesSnapshot = homeSnapshot;
    fermentation::RuntimeMessage message;
    message.id = 1U;
    message.active = true;
    messagesSnapshot.messages.push_back({message});
    fermentation::FermentationTouchWorkspace messagesWorkspace;
    messagesWorkspace.setPage(fermentation::FermentationUiPage::Messages);
    const auto messages = fermentation::main_ui::makeRepresentativeScreen(
        messagesSnapshot, messagesWorkspace, packs,
        device_platform::LocaleId{"en"});
    TEST_ASSERT_EQUAL_UINT32(1U, messages.workspace.pager.itemCount);
    TEST_ASSERT_TRUE(hasText(messages, "1/1"));

    const auto target = fermentation::main_ui::targetAt(messages, 20U, 220U);
    TEST_ASSERT_TRUE(target.has_value());
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
    RUN_TEST(test_bottom_press_returns_existing_target);
    RUN_TEST(test_empty_home_omits_empty_pager_and_messages_pager_is_rendered);
    return UNITY_END();
}
