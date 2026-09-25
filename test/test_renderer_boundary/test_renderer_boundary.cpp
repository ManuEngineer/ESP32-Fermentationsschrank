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
    return std::any_of(
        screen.commands.begin(), screen.commands.end(),
        [text](const auto& command) { return command.text == text; });
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
    // The last bottom slot's fill is the second-to-last command; its label
    // is the actual last command since no touch is held (BLOCKER 4: no
    // PressFeedback command without an actual held touch).
    const auto& lastSlotFill = de.commands[de.commands.size() - 2U];
    TEST_ASSERT_EQUAL_UINT16(80U, lastSlotFill.rect.width);
    TEST_ASSERT_EQUAL_UINT16(200U, lastSlotFill.rect.top);
    TEST_ASSERT_EQUAL_STRING("assets/branding/manuengineer/ManuEngineer.svg",
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
    TEST_ASSERT_EQUAL(
        static_cast<int>(device_platform::DeviceUiTargetKind::BottomSlot),
        static_cast<int>(target->kind));
    TEST_ASSERT_EQUAL_UINT8(0U, target->slotIndex);

    const auto press = fermentation::main_ui::routePress(workspace, snapshot,
                                                         screen, 20U, 220U);
    TEST_ASSERT_NOT_EQUAL(
        static_cast<int>(device_platform::DeviceUiInteractionOutcome::Ignored),
        static_cast<int>(press.interaction.outcome));
}

void test_network_header_target_matches_rendered_status_icon_rect() {
    fermentation::FermentationUiSnapshot snapshot;
    snapshot.home.mode = fermentation::FermentationHomeMode::Standby;
    fermentation::FermentationTouchWorkspace workspace;
    const auto screen = fermentation::main_ui::makeRepresentativeScreen(
        snapshot, workspace, fermentation::makeFermentationUiTextPacks(),
        device_platform::LocaleId{"en"});

    const auto icon = std::find_if(
        screen.commands.begin(), screen.commands.end(), [](const auto& command) {
            return command.kind ==
                   fermentation::main_ui::ScreenDrawKind::NetworkStatusIcon;
        });
    TEST_ASSERT_TRUE(icon != screen.commands.end());
    TEST_ASSERT_EQUAL_UINT16(220U, icon->rect.left);
    TEST_ASSERT_EQUAL_UINT16(4U, icon->rect.top);
    TEST_ASSERT_EQUAL_UINT16(44U, icon->rect.width);
    TEST_ASSERT_EQUAL_UINT16(18U, icon->rect.height);

    const auto assertNetworkTarget = [&screen](std::uint16_t x,
                                               std::uint16_t y) {
        const auto target = fermentation::main_ui::targetAt(screen, x, y);
        TEST_ASSERT_TRUE(target.has_value());
        TEST_ASSERT_EQUAL(
            static_cast<int>(device_platform::DeviceUiTargetKind::HeaderNetwork),
            static_cast<int>(target->kind));
    };
    assertNetworkTarget(220U, 4U);
    assertNetworkTarget(263U, 21U);
    assertNetworkTarget(240U, 12U);

    const auto assertNoTarget = [&screen](std::uint16_t x, std::uint16_t y) {
        TEST_ASSERT_FALSE(
            fermentation::main_ui::targetAt(screen, x, y).has_value());
    };
    assertNoTarget(219U, 12U);
    assertNoTarget(264U, 12U);
    assertNoTarget(240U, 3U);
    assertNoTarget(240U, 22U);

    const auto bottom = fermentation::main_ui::targetAt(screen, 20U, 220U);
    TEST_ASSERT_TRUE(bottom.has_value());
    TEST_ASSERT_EQUAL(
        static_cast<int>(device_platform::DeviceUiTargetKind::BottomSlot),
        static_cast<int>(bottom->kind));
    TEST_ASSERT_EQUAL_UINT8(0U, bottom->slotIndex);
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

void test_render_key_stable_for_same_snapshot_and_workspace() {
    fermentation::FermentationUiSnapshot snapshot;
    snapshot.home.mode = fermentation::FermentationHomeMode::Standby;
    fermentation::FermentationTouchWorkspace workspace;
    const auto packs = fermentation::makeFermentationUiTextPacks();

    const auto first = fermentation::main_ui::makeRepresentativeScreen(
        snapshot, workspace, packs, device_platform::LocaleId{"en"});
    const auto second = fermentation::main_ui::makeRepresentativeScreen(
        snapshot, workspace, packs, device_platform::LocaleId{"en"});

    TEST_ASSERT_TRUE(fermentation::main_ui::makeScreenRenderKey(first) ==
                     fermentation::main_ui::makeScreenRenderKey(second));
}

void test_render_key_changes_on_workspace_navigation() {
    fermentation::FermentationUiSnapshot snapshot;
    snapshot.home.mode = fermentation::FermentationHomeMode::Standby;
    fermentation::FermentationTouchWorkspace workspace;
    const auto packs = fermentation::makeFermentationUiTextPacks();

    const auto home = fermentation::main_ui::makeRepresentativeScreen(
        snapshot, workspace, packs, device_platform::LocaleId{"en"});
    workspace.setPage(fermentation::FermentationUiPage::Messages);
    const auto messages = fermentation::main_ui::makeRepresentativeScreen(
        snapshot, workspace, packs, device_platform::LocaleId{"en"});

    TEST_ASSERT_FALSE(fermentation::main_ui::makeScreenRenderKey(home) ==
                      fermentation::main_ui::makeScreenRenderKey(messages));
}

void test_render_key_changes_on_pager_move() {
    fermentation::FermentationUiSnapshot snapshot;
    snapshot.home.mode = fermentation::FermentationHomeMode::Standby;
    fermentation::RuntimeMessage first;
    first.id = 1U;
    first.active = true;
    fermentation::RuntimeMessage second;
    second.id = 2U;
    second.active = true;
    snapshot.messages.push_back({first});
    snapshot.messages.push_back({second});
    fermentation::FermentationTouchWorkspace workspace;
    workspace.setPage(fermentation::FermentationUiPage::Messages);
    const auto packs = fermentation::makeFermentationUiTextPacks();

    const auto beforeMove = fermentation::main_ui::makeRepresentativeScreen(
        snapshot, workspace, packs, device_platform::LocaleId{"en"});
    // Real navigation goes through the existing typed press path (the "down"
    // bottom slot), which is what synchronizes the workspace-local pager with
    // the current item count; calling movePagerDown() directly without a
    // prior press leaves it at its default zero item count.
    const auto press = fermentation::main_ui::routePress(
        workspace, snapshot, beforeMove, 180U, 220U);
    TEST_ASSERT_TRUE(press.navigated);
    const auto afterMove = fermentation::main_ui::makeRepresentativeScreen(
        snapshot, workspace, packs, device_platform::LocaleId{"en"});

    TEST_ASSERT_FALSE(fermentation::main_ui::makeScreenRenderKey(beforeMove) ==
                      fermentation::main_ui::makeScreenRenderKey(afterMove));
}

void test_render_key_changes_on_locale_change() {
    fermentation::FermentationUiSnapshot snapshot;
    snapshot.home.mode = fermentation::FermentationHomeMode::Standby;
    fermentation::FermentationTouchWorkspace workspace;
    const auto packs = fermentation::makeFermentationUiTextPacks();

    const auto de = fermentation::main_ui::makeRepresentativeScreen(
        snapshot, workspace, packs, device_platform::LocaleId{"de"});
    const auto en = fermentation::main_ui::makeRepresentativeScreen(
        snapshot, workspace, packs, device_platform::LocaleId{"en"});

    TEST_ASSERT_FALSE(fermentation::main_ui::makeScreenRenderKey(de) ==
                      fermentation::main_ui::makeScreenRenderKey(en));
}

void test_no_touch_means_no_press_feedback_command() {
    fermentation::FermentationUiSnapshot snapshot;
    snapshot.home.mode = fermentation::FermentationHomeMode::Standby;
    fermentation::FermentationTouchWorkspace workspace;
    const auto screen = fermentation::main_ui::makeRepresentativeScreen(
        snapshot, workspace, fermentation::makeFermentationUiTextPacks(),
        device_platform::LocaleId{"en"});

    TEST_ASSERT_FALSE(std::any_of(
        screen.commands.begin(), screen.commands.end(),
        [](const auto& command) {
            return command.kind ==
                   fermentation::main_ui::ScreenDrawKind::PressFeedback;
        }));
}

void test_held_bottom_slot_renders_press_feedback_for_that_slot_only() {
    fermentation::FermentationUiSnapshot snapshot;
    snapshot.home.mode = fermentation::FermentationHomeMode::Standby;
    fermentation::FermentationTouchWorkspace workspace;
    const device_platform::DeviceUiTarget held{
        device_platform::DeviceUiTargetKind::BottomSlot, 2U};
    const auto screen = fermentation::main_ui::makeRepresentativeScreen(
        snapshot, workspace, fermentation::makeFermentationUiTextPacks(),
        device_platform::LocaleId{"en"}, held);

    const auto feedback = std::find_if(
        screen.commands.begin(), screen.commands.end(),
        [](const auto& command) {
            return command.kind ==
                   fermentation::main_ui::ScreenDrawKind::PressFeedback;
        });
    TEST_ASSERT_TRUE(feedback != screen.commands.end());
    TEST_ASSERT_EQUAL_UINT16(160U, feedback->rect.left);
}

void test_program_list_page_shows_catalog_program_names() {
    fermentation::FermentationUiSnapshot snapshot;
    snapshot.home.mode = fermentation::FermentationHomeMode::Standby;
    fermentation::ProgramCatalog catalog;
    fermentation::ProgramDocument document;
    document.program.id = "p1";
    document.program.name = "Sauerkraut";
    catalog.programs.push_back(document);
    fermentation::FermentationTouchWorkspace workspace;
    workspace.setPage(fermentation::FermentationUiPage::ProgramList);
    const auto screen = fermentation::main_ui::makeRepresentativeScreen(
        snapshot, workspace, fermentation::makeFermentationUiTextPacks(),
        device_platform::LocaleId{"en"}, std::nullopt, &catalog);

    TEST_ASSERT_TRUE(hasText(screen, "Sauerkraut"));
}

fermentation::ProgramCatalog makeRunnableCatalogForTest() {
    // Mirrors the runnable catalog fixture used by the workspace tests:
    // the factory catalog needs its stage/qualification numbers filled in
    // to pass ValidationPurpose::Runnable, which selectProgram() requires.
    auto catalog = fermentation::makeFactoryProgramCatalog();
    auto& program = catalog.programs.back().program;
    program.name = "Miso";
    program.fermentationStages.front().targetTemperatureCelsius = 25.0;
    program.fermentationStages.front().durationMinutes = 60U;
    program.targetQualification.bandCelsius = 0.5;
    program.targetQualification.durationMinutes = 10U;
    program.maximumTargetReachMinutes = 180U;
    program.productSensorFailure.fallbackDelaySeconds = 60U;
    return catalog;
}

void test_delete_confirmation_page_shows_selected_program_name() {
    fermentation::FermentationUiSnapshot snapshot;
    snapshot.home.mode = fermentation::FermentationHomeMode::Standby;
    auto catalog = makeRunnableCatalogForTest();
    const auto selectedId = catalog.programs.back().program.id;
    fermentation::FermentationTouchWorkspace workspace;
    TEST_ASSERT_TRUE(workspace.selectProgram(selectedId, catalog));
    workspace.setPage(
        fermentation::FermentationUiPage::ProgramDeleteConfirmation);
    const auto screen = fermentation::main_ui::makeRepresentativeScreen(
        snapshot, workspace, fermentation::makeFermentationUiTextPacks(),
        device_platform::LocaleId{"en"}, std::nullopt, &catalog);

    TEST_ASSERT_TRUE(hasText(screen, "Miso"));
}

void test_service_page_shows_blocked_reason() {
    fermentation::FermentationUiSnapshot snapshot;
    snapshot.home.mode = fermentation::FermentationHomeMode::Standby;
    snapshot.service.available = false;
    fermentation::FermentationTouchWorkspace workspace;
    workspace.setPage(fermentation::FermentationUiPage::Service);
    const auto packs = fermentation::makeFermentationUiTextPacks();
    const auto screen = fermentation::main_ui::makeRepresentativeScreen(
        snapshot, workspace, packs, device_platform::LocaleId{"en"});

    const auto expected = device_platform::resolveText(
        packs, device_platform::LocaleId{"en"},
        fermentation::fermentationTextKey("service-locked"));
    TEST_ASSERT_TRUE(hasText(screen, expected.value));
}

void test_home_service_status_uses_compact_locale_projection() {
    fermentation::FermentationUiSnapshot snapshot;
    snapshot.home.mode = fermentation::FermentationHomeMode::Standby;
    snapshot.service.available = false;
    fermentation::FermentationTouchWorkspace workspace;
    const auto packs = fermentation::makeFermentationUiTextPacks();

    for (const auto locale : {"de", "en", "es"}) {
        const auto screen = fermentation::main_ui::makeRepresentativeScreen(
            snapshot, workspace, packs, device_platform::LocaleId{locale});
        const auto expected = device_platform::resolveText(
            packs, device_platform::LocaleId{locale},
            fermentation::fermentationTextKey("service-home-locked"));
        const auto expectedValue =
            std::string_view{locale} == "de"
                ? "Service aus"
                : (std::string_view{locale} == "en" ? "Service off"
                                                    : "Servicio off");
        TEST_ASSERT_EQUAL_STRING(expectedValue, expected.value.c_str());
        TEST_ASSERT_TRUE(hasText(screen, expected.value));
        TEST_ASSERT_FALSE(hasText(
            screen, device_platform::resolveText(
                        packs, device_platform::LocaleId{locale},
                        fermentation::fermentationTextKey("service-locked"))
                        .value));
    }
}

void test_recovery_page_shows_unavailable_capability_count() {
    fermentation::FermentationUiSnapshot snapshot;
    snapshot.home.mode = fermentation::FermentationHomeMode::Recovery;
    snapshot.home.processState = fermentation::ProcessState::SafeBoot;
    snapshot.recovery.mode =
        fermentation::RecoveryViewMode::FallbackSelectionRequired;
    fermentation::FermentationTouchWorkspace workspace;
    workspace.setPage(fermentation::FermentationUiPage::Recovery);
    const auto packs = fermentation::makeFermentationUiTextPacks();
    const auto screen = fermentation::main_ui::makeRepresentativeScreen(
        snapshot, workspace, packs, device_platform::LocaleId{"en"});

    const auto expected = device_platform::resolveText(
        packs, device_platform::LocaleId{"en"},
        fermentation::fermentationTextKey("unavailable"));
    TEST_ASSERT_TRUE(hasText(screen, expected.value + " 4"));
}

void test_clock_text_dash_when_untrusted() {
    fermentation::FermentationUiSnapshot snapshot;
    fermentation::FermentationTouchWorkspace workspace;
    const auto screen = fermentation::main_ui::makeRepresentativeScreen(
        snapshot, workspace, fermentation::makeFermentationUiTextPacks(),
        device_platform::LocaleId{"en"});

    TEST_ASSERT_EQUAL_STRING("--:--", screen.clockText.c_str());
}

void test_clock_text_formats_trusted_utc_as_hh_mm() {
    fermentation::FermentationUiSnapshot snapshot;
    fermentation::FermentationTouchWorkspace workspace;
    const device_platform::ClockViewInput clock{3661, {}};  // 01:01:01 UTC
    const auto screen = fermentation::main_ui::makeRepresentativeScreen(
        snapshot, workspace, fermentation::makeFermentationUiTextPacks(),
        device_platform::LocaleId{"en"}, std::nullopt, nullptr,
        device_platform::DeviceUiNetworkStatus::Unavailable, clock);

    TEST_ASSERT_EQUAL_STRING("01:01", screen.clockText.c_str());
}

void test_network_status_icon_changes_token_and_has_r1_line_height() {
    fermentation::FermentationUiSnapshot snapshot;
    fermentation::FermentationTouchWorkspace workspace;
    const auto packs = fermentation::makeFermentationUiTextPacks();
    const auto connected = fermentation::main_ui::makeRepresentativeScreen(
        snapshot, workspace, packs, device_platform::LocaleId{"en"},
        std::nullopt, nullptr,
        device_platform::DeviceUiNetworkStatus::Connected);
    const auto unavailable = fermentation::main_ui::makeRepresentativeScreen(
        snapshot, workspace, packs, device_platform::LocaleId{"en"},
        std::nullopt, nullptr,
        device_platform::DeviceUiNetworkStatus::Unavailable);

    const auto findIcon = [](const auto& screen) {
        return std::find_if(
            screen.commands.begin(), screen.commands.end(),
            [](const auto& command) {
                return command.kind ==
                       fermentation::main_ui::ScreenDrawKind::NetworkStatusIcon;
            });
    };
    const auto connectedIcon = findIcon(connected);
    const auto unavailableIcon = findIcon(unavailable);
    TEST_ASSERT_TRUE(connectedIcon != connected.commands.end());
    TEST_ASSERT_TRUE(unavailableIcon != unavailable.commands.end());
    TEST_ASSERT_EQUAL_UINT16(
        fermentation::main_ui::RepresentativeScreen::kTextLineHeight,
        connectedIcon->rect.height);
    TEST_ASSERT_FALSE(connectedIcon->token == unavailableIcon->token);
}

void test_all_single_line_commands_have_r1_text_line_height() {
    fermentation::FermentationUiSnapshot snapshot;
    fermentation::FermentationTouchWorkspace workspace;
    const auto screen = fermentation::main_ui::makeRepresentativeScreen(
        snapshot, workspace, fermentation::makeFermentationUiTextPacks(),
        device_platform::LocaleId{"en"});

    for (const auto& command : screen.commands) {
        if (command.kind == fermentation::main_ui::ScreenDrawKind::Text ||
            command.kind ==
                fermentation::main_ui::ScreenDrawKind::NetworkStatusIcon) {
            TEST_ASSERT_TRUE(
                command.rect.height >=
                fermentation::main_ui::RepresentativeScreen::kTextLineHeight);
        }
    }
}

void test_theme_is_sourced_from_canonical_r1_catalog() {
    fermentation::FermentationUiSnapshot snapshot;
    fermentation::FermentationTouchWorkspace workspace;
    const auto screen = fermentation::main_ui::makeRepresentativeScreen(
        snapshot, workspace, fermentation::makeFermentationUiTextPacks(),
        device_platform::LocaleId{"en"});

    const auto buildCatalog =
        fermentation::makeFermentationR1DeviceUiBuildCatalog();
    TEST_ASSERT_TRUE(screen.theme.id == buildCatalog.defaultTheme);
    TEST_ASSERT_FALSE(screen.theme.declaredTokens.empty());
}

void test_render_key_changes_on_network_status_and_clock() {
    fermentation::FermentationUiSnapshot snapshot;
    fermentation::FermentationTouchWorkspace workspace;
    const auto packs = fermentation::makeFermentationUiTextPacks();
    const auto unavailable = fermentation::main_ui::makeRepresentativeScreen(
        snapshot, workspace, packs, device_platform::LocaleId{"en"});
    const auto connected = fermentation::main_ui::makeRepresentativeScreen(
        snapshot, workspace, packs, device_platform::LocaleId{"en"},
        std::nullopt, nullptr,
        device_platform::DeviceUiNetworkStatus::Connected);
    const device_platform::ClockViewInput clock{3661, {}};
    const auto clocked = fermentation::main_ui::makeRepresentativeScreen(
        snapshot, workspace, packs, device_platform::LocaleId{"en"},
        std::nullopt, nullptr,
        device_platform::DeviceUiNetworkStatus::Unavailable, clock);

    TEST_ASSERT_FALSE(fermentation::main_ui::makeScreenRenderKey(unavailable) ==
                      fermentation::main_ui::makeScreenRenderKey(connected));
    TEST_ASSERT_FALSE(fermentation::main_ui::makeScreenRenderKey(unavailable) ==
                      fermentation::main_ui::makeScreenRenderKey(clocked));
}

void test_render_key_unchanged_workspace_remains_equal() {
    fermentation::FermentationUiSnapshot snapshot;
    snapshot.home.mode = fermentation::FermentationHomeMode::Standby;
    fermentation::FermentationTouchWorkspace workspace;
    const auto packs = fermentation::makeFermentationUiTextPacks();

    const auto first = fermentation::main_ui::makeRepresentativeScreen(
        snapshot, workspace, packs, device_platform::LocaleId{"en"});
    const auto second = fermentation::main_ui::makeRepresentativeScreen(
        snapshot, workspace, packs, device_platform::LocaleId{"en"});

    TEST_ASSERT_TRUE(fermentation::main_ui::makeScreenRenderKey(first) ==
                     fermentation::main_ui::makeScreenRenderKey(second));
}

void test_render_key_changes_when_manual_holding_values_enable_confirm() {
    fermentation::FermentationUiSnapshot snapshot;
    fermentation::FermentationTouchWorkspace workspace;
    workspace.setPage(fermentation::FermentationUiPage::ManualHolding);
    const auto packs = fermentation::makeFermentationUiTextPacks();

    const auto beforeValues = fermentation::main_ui::makeRepresentativeScreen(
        snapshot, workspace, packs, device_platform::LocaleId{"en"});
    TEST_ASSERT_FALSE(beforeValues.workspace.bottomSlots[2].enabled);

    workspace.setManualHoldingValues(
        fermentation::FermentationUiManualRunPlanValues{});
    const auto afterValues = fermentation::main_ui::makeRepresentativeScreen(
        snapshot, workspace, packs, device_platform::LocaleId{"en"});
    TEST_ASSERT_TRUE(afterValues.workspace.bottomSlots[2].enabled);

    TEST_ASSERT_FALSE(
        fermentation::main_ui::makeScreenRenderKey(beforeValues) ==
        fermentation::main_ui::makeScreenRenderKey(afterValues));
}

void test_render_key_changes_when_program_edit_candidate_enables_save() {
    fermentation::FermentationUiSnapshot snapshot;
    fermentation::FermentationTouchWorkspace workspace;
    workspace.setPage(fermentation::FermentationUiPage::ProgramEdit);
    const auto packs = fermentation::makeFermentationUiTextPacks();

    const auto beforeCandidate =
        fermentation::main_ui::makeRepresentativeScreen(
            snapshot, workspace, packs, device_platform::LocaleId{"en"});
    TEST_ASSERT_FALSE(beforeCandidate.workspace.bottomSlots[3].enabled);

    workspace.setProgramEditCandidate(fermentation::ProgramDocument{});
    const auto afterCandidate = fermentation::main_ui::makeRepresentativeScreen(
        snapshot, workspace, packs, device_platform::LocaleId{"en"});
    TEST_ASSERT_TRUE(afterCandidate.workspace.bottomSlots[3].enabled);

    TEST_ASSERT_FALSE(
        fermentation::main_ui::makeScreenRenderKey(beforeCandidate) ==
        fermentation::main_ui::makeScreenRenderKey(afterCandidate));
}

// Branding FOLLOW-UP layout proof: the Logo command's rect must be exactly
// the generated asset's native 168x24 size (no stretch/crop - see
// scripts/generate_branding_asset.py) and must not overlap the DE/EN/ES,
// WLAN or clock header boxes. This is a pure command-rect check; it needs
// no LVGL/LVGL image asset to run natively.
void test_logo_command_is_native_size_and_does_not_overlap_header_boxes() {
    fermentation::FermentationUiSnapshot snapshot;
    fermentation::FermentationTouchWorkspace workspace;
    const auto screen = fermentation::main_ui::makeRepresentativeScreen(
        snapshot, workspace, fermentation::makeFermentationUiTextPacks(),
        device_platform::LocaleId{"en"});

    const auto logo = std::find_if(
        screen.commands.begin(), screen.commands.end(),
        [](const auto& command) {
            return command.kind == fermentation::main_ui::ScreenDrawKind::Logo;
        });
    TEST_ASSERT_TRUE(logo != screen.commands.end());
    TEST_ASSERT_EQUAL_UINT16(168U, logo->rect.width);
    TEST_ASSERT_EQUAL_UINT16(24U, logo->rect.height);

    const auto overlapsLogo =
        [&logo](const device_platform::DisplayRect& other) {
            const auto logoRight = logo->rect.left + logo->rect.width;
            const auto logoBottom = logo->rect.top + logo->rect.height;
            const auto otherRight = other.left + other.width;
            const auto otherBottom = other.top + other.height;
            return logo->rect.left < otherRight && other.left < logoRight &&
                   logo->rect.top < otherBottom && other.top < logoBottom;
        };
    std::size_t headerBoxesChecked = 0U;
    for (const auto& command : screen.commands) {
        if (command.rect.top != logo->rect.top || &command == &*logo) continue;
        // Every other command sharing the logo's header row must not
        // overlap it (the DE/EN/ES, WLAN and clock boxes all sit at the
        // same top=4U row per fermentation_ui_renderer.cpp).
        TEST_ASSERT_FALSE(overlapsLogo(command.rect));
        ++headerBoxesChecked;
    }
    TEST_ASSERT_TRUE(headerBoxesChecked >= 3U);
}

void test_network_page_projects_softap_data_only_in_local_display_model() {
    fermentation::FermentationUiSnapshot snapshot;
    snapshot.network.currentMode = device_platform::NetworkMode::HOME_WIFI;
    fermentation::FermentationTouchWorkspace workspace;
    workspace.setPage(fermentation::FermentationUiPage::HeaderNetwork);
    const auto packs = fermentation::makeFermentationUiTextPacks();
    const device_platform::NetworkAccessPointInfo accessPoint{
        "Ferment-Setup", "local-only-password", 0x0104A8C0U};
    const auto screen = fermentation::main_ui::makeRepresentativeScreen(
        snapshot, workspace, packs, device_platform::LocaleId{"en"},
        std::nullopt, nullptr,
        device_platform::DeviceUiNetworkStatus::Unavailable, {}, accessPoint);

    TEST_ASSERT_TRUE(hasText(screen, "Home Wi-Fi"));
    TEST_ASSERT_TRUE(hasText(screen, "SSID: Ferment-Setup"));
    TEST_ASSERT_TRUE(hasText(screen, "Password: local-only-password"));
    TEST_ASSERT_TRUE(hasText(screen, "IP: 192.168.4.1"));
    const auto qr =
        std::find_if(screen.commands.begin(), screen.commands.end(),
                     [](const auto& command) {
                         return command.kind ==
                                fermentation::main_ui::ScreenDrawKind::QrCode;
                     });
    TEST_ASSERT_TRUE(qr != screen.commands.end());
    TEST_ASSERT_EQUAL_STRING(
        "WIFI:T:WPA;S:Ferment-Setup;P:local-only-password;;", qr->text.c_str());
    TEST_ASSERT_EQUAL_UINT16(200U, qr->rect.left);
    TEST_ASSERT_EQUAL_UINT16(68U, qr->rect.top);
    TEST_ASSERT_EQUAL_UINT16(112U, qr->rect.width);
    TEST_ASSERT_EQUAL_UINT16(112U, qr->rect.height);
    TEST_ASSERT_TRUE(qr->text.find("http") == std::string::npos);
    TEST_ASSERT_TRUE(qr->text.find("192.168.4.1") == std::string::npos);
    TEST_ASSERT_NOT_EQUAL(0U, screen.localNetworkInfoFingerprint);
    const auto ssid =
        std::find_if(screen.commands.begin(), screen.commands.end(),
                     [](const auto& command) {
                         return command.text == "SSID: Ferment-Setup";
                     });
    const auto password =
        std::find_if(screen.commands.begin(), screen.commands.end(),
                     [](const auto& command) {
                         return command.text == "Password: local-only-password";
                     });
    TEST_ASSERT_TRUE(ssid != screen.commands.end());
    TEST_ASSERT_TRUE(password != screen.commands.end());
    TEST_ASSERT_TRUE(ssid->wrapText);
    TEST_ASSERT_TRUE(password->wrapText);
    TEST_ASSERT_EQUAL_UINT16(184U, password->rect.width);
    TEST_ASSERT_EQUAL_UINT16(72U, password->rect.height);

    auto changedAccessPoint = accessPoint;
    changedAccessPoint.password += "-rotated";
    const auto changedScreen = fermentation::main_ui::makeRepresentativeScreen(
        snapshot, workspace, packs, device_platform::LocaleId{"en"},
        std::nullopt, nullptr,
        device_platform::DeviceUiNetworkStatus::Unavailable, {},
        changedAccessPoint);
    TEST_ASSERT_FALSE(
        fermentation::main_ui::makeScreenRenderKey(screen) ==
        fermentation::main_ui::makeScreenRenderKey(changedScreen));
    const auto changedQr =
        std::find_if(changedScreen.commands.begin(),
                     changedScreen.commands.end(), [](const auto& command) {
                         return command.kind ==
                                fermentation::main_ui::ScreenDrawKind::QrCode;
                     });
    TEST_ASSERT_TRUE(changedQr != changedScreen.commands.end());
    TEST_ASSERT_TRUE(changedQr->text != qr->text);

    workspace.setPage(fermentation::FermentationUiPage::Home);
    const auto ordinaryScreen = fermentation::main_ui::makeRepresentativeScreen(
        snapshot, workspace, packs, device_platform::LocaleId{"en"},
        std::nullopt, nullptr,
        device_platform::DeviceUiNetworkStatus::Unavailable, {}, accessPoint);
    TEST_ASSERT_FALSE(hasText(ordinaryScreen, "Ferment-Setup"));
    TEST_ASSERT_FALSE(hasText(ordinaryScreen, "local-only-password"));
    TEST_ASSERT_EQUAL_UINT64(0U, ordinaryScreen.localNetworkInfoFingerprint);

    for (const auto& command : screen.commands) {
        TEST_ASSERT_LESS_OR_EQUAL_UINT16(
            320U, command.rect.left + command.rect.width);
        TEST_ASSERT_LESS_OR_EQUAL_UINT16(
            240U, command.rect.top + command.rect.height);
    }
}

void test_softap_wifi_qr_escapes_reserved_characters_deterministically() {
    const device_platform::NetworkAccessPointInfo accessPoint{
        R"(semi;comma,colon:quote"slash\end)", R"(pass;word,:"\x)",
        0x0104A8C0U};
    const auto first =
        fermentation::main_ui::makeSoftApWifiQrPayload(accessPoint);
    const auto second =
        fermentation::main_ui::makeSoftApWifiQrPayload(accessPoint);
    TEST_ASSERT_TRUE(first.has_value());
    TEST_ASSERT_TRUE(second.has_value());
    TEST_ASSERT_EQUAL_STRING(
        R"(WIFI:T:WPA;S:semi\;comma\,colon\:quote\"slash\\end;P:pass\;word\,\:\"\\x;;)",
        first->c_str());
    TEST_ASSERT_EQUAL_STRING(first->c_str(), second->c_str());
    TEST_ASSERT_TRUE(first->find("http") == std::string::npos);
    TEST_ASSERT_TRUE(first->find("192.168.4.1") == std::string::npos);

    auto changed = accessPoint;
    changed.ssid += "-other";
    const auto changedPayload =
        fermentation::main_ui::makeSoftApWifiQrPayload(changed);
    TEST_ASSERT_TRUE(changedPayload.has_value());
    TEST_ASSERT_TRUE(*changedPayload != *first);

    changed = accessPoint;
    changed.password += "-other";
    const auto changedPasswordPayload =
        fermentation::main_ui::makeSoftApWifiQrPayload(changed);
    TEST_ASSERT_TRUE(changedPasswordPayload.has_value());
    TEST_ASSERT_TRUE(*changedPasswordPayload != *first);

    device_platform::NetworkAccessPointInfo incomplete;
    incomplete.ssid = "no-password";
    TEST_ASSERT_FALSE(
        fermentation::main_ui::makeSoftApWifiQrPayload(incomplete).has_value());
}

void test_network_page_missing_softap_info_is_explicit() {
    fermentation::FermentationUiSnapshot snapshot;
    fermentation::FermentationTouchWorkspace workspace;
    workspace.setPage(fermentation::FermentationUiPage::HeaderNetwork);
    const auto screen = fermentation::main_ui::makeRepresentativeScreen(
        snapshot, workspace, fermentation::makeFermentationUiTextPacks(),
        device_platform::LocaleId{"en"});
    TEST_ASSERT_TRUE(hasText(screen, "Access data unavailable"));
    TEST_ASSERT_EQUAL_UINT64(0U, screen.localNetworkInfoFingerprint);
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
#include "../../lib/fermentation_app/src/fermentation_ui_models.cpp"
#include "../../main/fermentation_ui_renderer.cpp"

int main() {
    UNITY_BEGIN();
    RUN_TEST(
        test_representative_screen_uses_existing_workspace_and_three_locales);
    RUN_TEST(test_bottom_press_returns_existing_target);
    RUN_TEST(test_network_header_target_matches_rendered_status_icon_rect);
    RUN_TEST(test_empty_home_omits_empty_pager_and_messages_pager_is_rendered);
    RUN_TEST(test_render_key_stable_for_same_snapshot_and_workspace);
    RUN_TEST(test_render_key_changes_on_workspace_navigation);
    RUN_TEST(test_render_key_changes_on_pager_move);
    RUN_TEST(test_render_key_changes_on_locale_change);
    RUN_TEST(test_no_touch_means_no_press_feedback_command);
    RUN_TEST(test_held_bottom_slot_renders_press_feedback_for_that_slot_only);
    RUN_TEST(test_program_list_page_shows_catalog_program_names);
    RUN_TEST(test_delete_confirmation_page_shows_selected_program_name);
    RUN_TEST(test_service_page_shows_blocked_reason);
    RUN_TEST(test_home_service_status_uses_compact_locale_projection);
    RUN_TEST(test_recovery_page_shows_unavailable_capability_count);
    RUN_TEST(test_clock_text_dash_when_untrusted);
    RUN_TEST(test_clock_text_formats_trusted_utc_as_hh_mm);
    RUN_TEST(test_network_status_icon_changes_token_and_has_r1_line_height);
    RUN_TEST(test_all_single_line_commands_have_r1_text_line_height);
    RUN_TEST(test_theme_is_sourced_from_canonical_r1_catalog);
    RUN_TEST(test_render_key_changes_on_network_status_and_clock);
    RUN_TEST(test_render_key_unchanged_workspace_remains_equal);
    RUN_TEST(test_render_key_changes_when_manual_holding_values_enable_confirm);
    RUN_TEST(test_render_key_changes_when_program_edit_candidate_enables_save);
    RUN_TEST(
        test_logo_command_is_native_size_and_does_not_overlap_header_boxes);
    RUN_TEST(
        test_network_page_projects_softap_data_only_in_local_display_model);
    RUN_TEST(test_softap_wifi_qr_escapes_reserved_characters_deterministically);
    RUN_TEST(test_network_page_missing_softap_info_is_explicit);
    return UNITY_END();
}
