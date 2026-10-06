#include <unity.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
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

bool overlaps(const device_platform::DisplayRect& left,
              const device_platform::DisplayRect& right) {
    return left.left < right.left + right.width &&
           right.left < left.left + left.width &&
           left.top < right.top + right.height &&
           right.top < left.top + left.height;
}

void assertWithinDisplay(const device_platform::DisplayRect& rect) {
    TEST_ASSERT_LESS_OR_EQUAL_UINT16(320U, rect.left + rect.width);
    TEST_ASSERT_LESS_OR_EQUAL_UINT16(240U, rect.top + rect.height);
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
        screen.commands.begin(), screen.commands.end(),
        [](const auto& command) {
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
            static_cast<int>(
                device_platform::DeviceUiTargetKind::HeaderNetwork),
            static_cast<int>(target->kind));
    };
    assertNetworkTarget(220U, 4U);
    assertNetworkTarget(263U, 21U);
    assertNetworkTarget(240U, 12U);

    const auto assertNoTarget = [&screen](std::uint16_t x, std::uint16_t y) {
        TEST_ASSERT_FALSE(
            fermentation::main_ui::targetAt(screen, x, y).has_value());
    };
    assertNoTarget(264U, 12U);
    assertNoTarget(240U, 3U);
    assertNoTarget(240U, 22U);

    // The language zone (x=176..219, y=0..31) ends right before the unchanged
    // network zone and never reaches the logo (x=4..172).
    const auto assertLanguageTarget = [&screen](std::uint16_t x,
                                                std::uint16_t y) {
        const auto target = fermentation::main_ui::targetAt(screen, x, y);
        TEST_ASSERT_TRUE(target.has_value());
        TEST_ASSERT_EQUAL(
            static_cast<int>(
                device_platform::DeviceUiTargetKind::HeaderLanguage),
            static_cast<int>(target->kind));
    };
    assertLanguageTarget(176U, 0U);
    assertLanguageTarget(200U, 12U);
    assertLanguageTarget(219U, 12U);
    assertLanguageTarget(219U, 31U);
    assertNoTarget(175U, 12U);
    assertNoTarget(100U, 12U);
    assertNoTarget(200U, 32U);
    assertNoTarget(240U, 31U);

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

using fermentation::main_ui::ScreenRenderKey;

// Builds the allocation-free pre-render key for the given visible inputs
// (S4). The catalog identity comes from a default presentation cache, i.e.
// "no copy", unless a test passes its own.
ScreenRenderKey keyFor(
    const fermentation::FermentationUiSnapshot& snapshot,
    const fermentation::FermentationTouchWorkspace& workspace,
    const char* locale = "en",
    std::optional<device_platform::DeviceUiTarget> pressed = std::nullopt,
    device_platform::DeviceUiNetworkStatus network =
        device_platform::DeviceUiNetworkStatus::Unavailable,
    std::optional<std::int64_t> utc = std::nullopt,
    std::uint64_t apFingerprint = 0U,
    const fermentation::FermentationUiPresentationCache& presentation =
        fermentation::FermentationUiPresentationCache{}) {
    return fermentation::main_ui::makeScreenRenderKey(
        snapshot, workspace, device_platform::LocaleId{locale}, pressed,
        presentation, network, utc, apFingerprint);
}

void test_render_key_stable_for_same_snapshot_and_workspace() {
    fermentation::FermentationUiSnapshot snapshot;
    snapshot.home.mode = fermentation::FermentationHomeMode::Standby;
    fermentation::FermentationTouchWorkspace workspace;

    TEST_ASSERT_TRUE(keyFor(snapshot, workspace) ==
                     keyFor(snapshot, workspace));
}

void test_render_key_changes_on_workspace_navigation() {
    fermentation::FermentationUiSnapshot snapshot;
    snapshot.home.mode = fermentation::FermentationHomeMode::Standby;
    fermentation::FermentationTouchWorkspace workspace;

    const auto home = keyFor(snapshot, workspace);
    workspace.setPage(fermentation::FermentationUiPage::Messages);
    const auto messages = keyFor(snapshot, workspace);

    TEST_ASSERT_FALSE(home == messages);
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
    const auto keyBefore = keyFor(snapshot, workspace);
    // Real navigation goes through the existing typed press path (the "down"
    // bottom slot), which is what synchronizes the workspace-local pager with
    // the current item count; calling movePagerDown() directly without a
    // prior press leaves it at its default zero item count.
    const auto press = fermentation::main_ui::routePress(
        workspace, snapshot, beforeMove, 180U, 220U);
    TEST_ASSERT_TRUE(press.navigated);

    TEST_ASSERT_FALSE(keyBefore == keyFor(snapshot, workspace));
}

void test_render_key_changes_on_locale_change() {
    fermentation::FermentationUiSnapshot snapshot;
    snapshot.home.mode = fermentation::FermentationHomeMode::Standby;
    fermentation::FermentationTouchWorkspace workspace;

    TEST_ASSERT_FALSE(keyFor(snapshot, workspace, "de") ==
                      keyFor(snapshot, workspace, "en"));
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

// S1: program list rows. Entries come from the factory catalog plus copies of
// the runnable last program so the three-row window must scroll.
fermentation::ProgramCatalog makeScrollableCatalogForTest() {
    auto catalog = makeRunnableCatalogForTest();
    const auto templateDocument = catalog.programs.back();
    for (std::size_t index = 0U; index < 3U; ++index) {
        auto document = templateDocument;
        document.program.id = "scroll-" + std::to_string(index);
        document.program.name = "Scroll " + std::to_string(index);
        catalog.programs.push_back(std::move(document));
    }
    return catalog;
}

fermentation::main_ui::RepresentativeScreen listScreen(
    const fermentation::FermentationUiSnapshot& snapshot,
    fermentation::FermentationTouchWorkspace& workspace,
    const fermentation::ProgramCatalog& catalog,
    std::optional<device_platform::DeviceUiTarget> pressed = std::nullopt) {
    return fermentation::main_ui::makeRepresentativeScreen(
        snapshot, workspace, fermentation::makeFermentationUiTextPacks(),
        device_platform::LocaleId{"en"}, pressed, &catalog);
}

void test_program_list_rows_are_40px_touch_rows_with_exact_hit_zones() {
    fermentation::FermentationUiSnapshot snapshot;
    snapshot.home.mode = fermentation::FermentationHomeMode::Standby;
    const auto catalog = makeScrollableCatalogForTest();
    fermentation::FermentationTouchWorkspace workspace;
    workspace.setPage(fermentation::FermentationUiPage::ProgramList);
    const auto screen = listScreen(snapshot, workspace, catalog);
    const auto& names = screen.workspace.programList;
    TEST_ASSERT_TRUE(names.size() >= 5U);

    // Drawn rows: 304 px wide, 40 px pitch starting at y=64, three visible.
    for (std::size_t row = 0U; row < 3U; ++row) {
        const auto& name = names[row].program.program.name;
        const auto text = std::find_if(
            screen.commands.begin(), screen.commands.end(),
            [&name](const auto& command) { return command.text == name; });
        TEST_ASSERT_TRUE(text != screen.commands.end());
        // Text is vertically centred inside its 40 px row.
        TEST_ASSERT_EQUAL_UINT16(64U + row * 40U + 11U, text->rect.top);
        assertWithinDisplay(text->rect);
    }
    TEST_ASSERT_FALSE(hasText(screen, names[3].program.program.name));

    const auto assertCell = [&screen](std::uint16_t x, std::uint16_t y,
                                      std::uint8_t row) {
        const auto target = fermentation::main_ui::targetAt(screen, x, y);
        TEST_ASSERT_TRUE(target.has_value());
        TEST_ASSERT_EQUAL(
            static_cast<int>(device_platform::DeviceUiTargetKind::ContentCell),
            static_cast<int>(target->kind));
        TEST_ASSERT_EQUAL_UINT8(row, target->row);
        TEST_ASSERT_EQUAL_UINT8(0U, target->column);
    };
    assertCell(8U, 64U, 0U);
    assertCell(311U, 103U, 0U);
    assertCell(8U, 104U, 1U);
    assertCell(160U, 143U, 1U);
    assertCell(8U, 144U, 2U);
    assertCell(311U, 183U, 2U);

    const auto assertNoTarget = [&screen](std::uint16_t x, std::uint16_t y) {
        TEST_ASSERT_FALSE(
            fermentation::main_ui::targetAt(screen, x, y).has_value());
    };
    assertNoTarget(7U, 80U);
    assertNoTarget(312U, 80U);
    assertNoTarget(160U, 63U);
    assertNoTarget(160U, 184U);
    assertNoTarget(160U, 199U);
    // Header and bottom-slot targets are untouched by the rows.
    assertCell(100U, 70U, 0U);
    const auto bottomSlot = fermentation::main_ui::targetAt(screen, 100U, 210U);
    TEST_ASSERT_TRUE(bottomSlot.has_value());
    TEST_ASSERT_EQUAL(
        static_cast<int>(device_platform::DeviceUiTargetKind::BottomSlot),
        static_cast<int>(bottomSlot->kind));
}

void test_program_list_window_follows_the_pager_and_hits_follow_the_window() {
    fermentation::FermentationUiSnapshot snapshot;
    snapshot.home.mode = fermentation::FermentationHomeMode::Standby;
    const auto catalog = makeScrollableCatalogForTest();
    fermentation::FermentationTouchWorkspace workspace;
    workspace.setPage(fermentation::FermentationUiPage::ProgramList);
    auto screen = listScreen(snapshot, workspace, catalog);
    const auto entries = screen.workspace.programList;
    const auto total = entries.size();

    // "down" bottom slot, twice: window becomes entries 2..4.
    for (int step = 0; step < 2; ++step) {
        const auto press = fermentation::main_ui::routePress(
            workspace, snapshot, screen, 180U, 220U, &catalog);
        TEST_ASSERT_TRUE(press.navigated);
        screen = listScreen(snapshot, workspace, catalog);
    }
    TEST_ASSERT_FALSE(hasText(screen, entries[0].program.program.name));
    TEST_ASSERT_FALSE(hasText(screen, entries[1].program.program.name));
    for (std::size_t row = 0U; row < 3U && 2U + row < total; ++row)
        TEST_ASSERT_TRUE(
            hasText(screen, entries[2U + row].program.program.name));

    // Row 2 now selects entry 4 through the real hit-test and press route.
    const auto selected = fermentation::main_ui::routePress(
        workspace, snapshot, screen, 100U, 170U, &catalog);
    TEST_ASSERT_TRUE(selected.navigated);
    TEST_ASSERT_FALSE(selected.action.has_value());
    TEST_ASSERT_EQUAL_STRING(entries[4].program.program.id.c_str(),
                             workspace.selectedProgramId()->c_str());

    // Scrolled to the very end only row 0 is backed by an entry.
    fermentation::FermentationTouchWorkspace tail;
    tail.setPage(fermentation::FermentationUiPage::ProgramList);
    auto tailScreen = listScreen(snapshot, tail, catalog);
    for (std::size_t step = 0U; step + 1U < total; ++step) {
        TEST_ASSERT_TRUE(fermentation::main_ui::routePress(
                             tail, snapshot, tailScreen, 180U, 220U, &catalog)
                             .navigated);
        tailScreen = listScreen(snapshot, tail, catalog);
    }
    TEST_ASSERT_TRUE(
        fermentation::main_ui::targetAt(tailScreen, 100U, 70U).has_value());
    TEST_ASSERT_FALSE(
        fermentation::main_ui::targetAt(tailScreen, 100U, 110U).has_value());
}

void test_program_list_hit_rows_exist_only_on_the_program_list_page() {
    fermentation::FermentationUiSnapshot snapshot;
    snapshot.home.mode = fermentation::FermentationHomeMode::Standby;
    const auto catalog = makeScrollableCatalogForTest();
    fermentation::FermentationTouchWorkspace workspace;
    const auto home = listScreen(snapshot, workspace, catalog);
    TEST_ASSERT_FALSE(
        fermentation::main_ui::targetAt(home, 100U, 80U).has_value());
}

void test_held_program_row_renders_press_feedback_for_that_row_only() {
    fermentation::FermentationUiSnapshot snapshot;
    snapshot.home.mode = fermentation::FermentationHomeMode::Standby;
    const auto catalog = makeScrollableCatalogForTest();
    fermentation::FermentationTouchWorkspace workspace;
    workspace.setPage(fermentation::FermentationUiPage::ProgramList);
    const device_platform::DeviceUiTarget held{
        device_platform::DeviceUiTargetKind::ContentCell, 0U, 1U, 0U};
    const auto screen = listScreen(snapshot, workspace, catalog, held);

    std::size_t feedbackCount = 0U;
    for (const auto& command : screen.commands) {
        if (command.kind !=
            fermentation::main_ui::ScreenDrawKind::PressFeedback)
            continue;
        ++feedbackCount;
        TEST_ASSERT_EQUAL_UINT16(8U, command.rect.left);
        TEST_ASSERT_EQUAL_UINT16(104U, command.rect.top);
        TEST_ASSERT_EQUAL_UINT16(304U, command.rect.width);
        TEST_ASSERT_EQUAL_UINT16(40U, command.rect.height);
    }
    TEST_ASSERT_EQUAL_UINT32(1U, static_cast<std::uint32_t>(feedbackCount));

    // The render key distinguishes the held row so the feedback is redrawn.
    const auto otherRow = device_platform::DeviceUiTarget{
        device_platform::DeviceUiTargetKind::ContentCell, 0U, 0U, 0U};
    TEST_ASSERT_FALSE(keyFor(snapshot, workspace, "en", held) ==
                      keyFor(snapshot, workspace, "en", otherRow));
    TEST_ASSERT_FALSE(keyFor(snapshot, workspace, "en", held) ==
                      keyFor(snapshot, workspace, "en"));
}

void test_pager_counter_sits_in_the_title_row_clear_of_the_rows() {
    fermentation::FermentationUiSnapshot snapshot;
    snapshot.home.mode = fermentation::FermentationHomeMode::Standby;
    const auto catalog = makeScrollableCatalogForTest();
    fermentation::FermentationTouchWorkspace workspace;
    workspace.setPage(fermentation::FermentationUiPage::ProgramList);
    const auto screen = listScreen(snapshot, workspace, catalog);
    const auto counter = std::find_if(
        screen.commands.begin(), screen.commands.end(),
        [](const auto& command) { return command.text.rfind("1/", 0U) == 0U; });
    TEST_ASSERT_TRUE(counter != screen.commands.end());
    TEST_ASSERT_EQUAL_UINT16(40U, counter->rect.top);
    TEST_ASSERT_EQUAL_UINT16(248U, counter->rect.left);
    TEST_ASSERT_LESS_OR_EQUAL_UINT16(64U,
                                     counter->rect.top + counter->rect.height);
    assertWithinDisplay(counter->rect);
}

void test_not_startable_program_row_is_dimmed_and_stays_hittable() {
    fermentation::FermentationUiSnapshot snapshot;
    snapshot.home.mode = fermentation::FermentationHomeMode::Standby;
    auto catalog = makeScrollableCatalogForTest();
    // Factory programs are listed first, so the disabled one is row 0.
    catalog.programs.front().program.enabled = false;
    fermentation::FermentationTouchWorkspace workspace;
    workspace.setPage(fermentation::FermentationUiPage::ProgramList);
    const auto screen = listScreen(snapshot, workspace, catalog);
    const auto& first = screen.workspace.programList.front();
    TEST_ASSERT_FALSE(first.startable);
    const auto text =
        std::find_if(screen.commands.begin(), screen.commands.end(),
                     [&first](const auto& command) {
                         return command.text == first.program.program.name;
                     });
    TEST_ASSERT_TRUE(text != screen.commands.end());
    TEST_ASSERT_EQUAL(
        static_cast<int>(device_platform::ThemeToken::TextSecondary),
        static_cast<int>(text->token));
    // Administration must still reach it: the row is hittable.
    const auto target = fermentation::main_ui::targetAt(screen, 100U, 80U);
    TEST_ASSERT_TRUE(target.has_value());
    TEST_ASSERT_EQUAL(
        static_cast<int>(device_platform::DeviceUiTargetKind::ContentCell),
        static_cast<int>(target->kind));
}

void test_program_summary_names_the_reason_of_a_not_startable_program() {
    fermentation::FermentationUiSnapshot snapshot;
    snapshot.home.mode = fermentation::FermentationHomeMode::Standby;
    auto catalog = makeScrollableCatalogForTest();
    catalog.programs.back().program.enabled = false;
    fermentation::FermentationTouchWorkspace workspace;
    TEST_ASSERT_TRUE(
        workspace.selectProgram(catalog.programs.back().program.id, catalog));
    const auto screen = listScreen(snapshot, workspace, catalog);
    const auto packs = fermentation::makeFermentationUiTextPacks();
    const auto expected = device_platform::resolveText(
        packs, device_platform::LocaleId{"en"},
        fermentation::fermentationTextKey("program-disabled"));
    TEST_ASSERT_TRUE(hasText(screen, expected.value));
}

// S2: message rows and detail.
fermentation::FermentationUiSnapshot snapshotWithMessagesForRender(
    std::size_t count) {
    fermentation::FermentationUiSnapshot snapshot;
    snapshot.home.mode = fermentation::FermentationHomeMode::Standby;
    const std::array<fermentation::MessageCode, 5U> codes{
        fermentation::MessageCode::ProductInsertionRequested,
        fermentation::MessageCode::RunAborted,
        fermentation::MessageCode::SafetyFault,
        fermentation::MessageCode::RecoveryPending,
        fermentation::MessageCode::RunCompleted};
    for (std::size_t index = 0U; index < count; ++index) {
        fermentation::RuntimeMessage message;
        message.id = static_cast<std::uint32_t>(21U + index);
        message.code = codes[index % codes.size()];
        message.active = true;
        snapshot.messages.push_back({message});
    }
    return snapshot;
}

std::string textFor(const char* key, const char* locale) {
    return device_platform::resolveText(
               fermentation::makeFermentationUiTextPacks(),
               device_platform::LocaleId{locale},
               fermentation::fermentationTextKey(key))
        .value;
}

void test_message_list_rows_are_hittable_and_follow_the_pager_window() {
    auto snapshot = snapshotWithMessagesForRender(5U);
    snapshot.messages[0].message.acknowledged = true;
    snapshot.messages[1].message.acousticMuted = true;
    fermentation::FermentationTouchWorkspace workspace;
    workspace.setPage(fermentation::FermentationUiPage::Messages);
    auto screen = fermentation::main_ui::makeRepresentativeScreen(
        snapshot, workspace, fermentation::makeFermentationUiTextPacks(),
        device_platform::LocaleId{"en"});

    TEST_ASSERT_TRUE(hasText(screen, "Insert product"));
    TEST_ASSERT_TRUE(hasText(screen, "Run aborted"));
    TEST_ASSERT_TRUE(hasText(screen, "Safety fault"));
    TEST_ASSERT_FALSE(hasText(screen, "Recovery pending"));
    // Row state: acknowledged / muted entries show their state at the right.
    TEST_ASSERT_TRUE(hasText(screen, "Acknowledged"));
    TEST_ASSERT_TRUE(hasText(screen, "Muted"));
    const auto state = std::find_if(
        screen.commands.begin(), screen.commands.end(),
        [](const auto& command) { return command.text == "Acknowledged"; });
    TEST_ASSERT_EQUAL_UINT16(212U, state->rect.left);
    TEST_ASSERT_EQUAL_UINT16(75U, state->rect.top);

    const auto row2 = fermentation::main_ui::targetAt(screen, 100U, 150U);
    TEST_ASSERT_TRUE(row2.has_value());
    TEST_ASSERT_EQUAL(
        static_cast<int>(device_platform::DeviceUiTargetKind::ContentCell),
        static_cast<int>(row2->kind));
    TEST_ASSERT_EQUAL_UINT8(2U, row2->row);
    TEST_ASSERT_FALSE(
        fermentation::main_ui::targetAt(screen, 100U, 184U).has_value());

    // Scroll by two via the real bottom slot; the window becomes 2..4 and row
    // 2 selects the fifth message (canonical id 25).
    for (int step = 0; step < 2; ++step) {
        TEST_ASSERT_TRUE(fermentation::main_ui::routePress(workspace, snapshot,
                                                           screen, 180U, 220U)
                             .navigated);
        screen = fermentation::main_ui::makeRepresentativeScreen(
            snapshot, workspace, fermentation::makeFermentationUiTextPacks(),
            device_platform::LocaleId{"en"});
    }
    TEST_ASSERT_FALSE(hasText(screen, "Insert product"));
    TEST_ASSERT_TRUE(hasText(screen, "Run completed"));
    const auto selected = fermentation::main_ui::routePress(workspace, snapshot,
                                                            screen, 100U, 170U);
    TEST_ASSERT_TRUE(selected.navigated);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::FermentationUiPage::MessageDetail),
        static_cast<int>(workspace.page()));
    TEST_ASSERT_EQUAL_UINT32(25U, *workspace.view(snapshot).selectedMessageId);

    // The held row is drawn as press feedback for that row only.
    fermentation::FermentationTouchWorkspace other;
    other.setPage(fermentation::FermentationUiPage::Messages);
    const device_platform::DeviceUiTarget held{
        device_platform::DeviceUiTargetKind::ContentCell, 0U, 1U, 0U};
    const auto feedbackScreen = fermentation::main_ui::makeRepresentativeScreen(
        snapshot, other, fermentation::makeFermentationUiTextPacks(),
        device_platform::LocaleId{"en"}, held);
    const auto feedback = std::find_if(
        feedbackScreen.commands.begin(), feedbackScreen.commands.end(),
        [](const auto& command) {
            return command.kind ==
                   fermentation::main_ui::ScreenDrawKind::PressFeedback;
        });
    TEST_ASSERT_TRUE(feedback != feedbackScreen.commands.end());
    TEST_ASSERT_EQUAL_UINT16(104U, feedback->rect.top);
}

void test_empty_message_list_has_no_hittable_rows() {
    const auto snapshot = snapshotWithMessagesForRender(0U);
    fermentation::FermentationTouchWorkspace workspace;
    workspace.setPage(fermentation::FermentationUiPage::Messages);
    const auto screen = fermentation::main_ui::makeRepresentativeScreen(
        snapshot, workspace, fermentation::makeFermentationUiTextPacks(),
        device_platform::LocaleId{"en"});
    TEST_ASSERT_FALSE(
        fermentation::main_ui::targetAt(screen, 100U, 80U).has_value());
}

void test_message_detail_shows_code_class_and_state_in_all_locales() {
    auto snapshot = snapshotWithMessagesForRender(1U);
    snapshot.messages[0].message.code =
        fermentation::MessageCode::TargetReachTimeExceeded;
    snapshot.messages[0].message.messageClass =
        fermentation::MessageClass::ProcessWarning;
    snapshot.messages[0].message.acknowledged = true;
    snapshot.messages[0].message.acousticMuted = true;
    for (const char* locale : {"en", "de", "es"}) {
        fermentation::FermentationTouchWorkspace workspace;
        workspace.setPage(fermentation::FermentationUiPage::Messages);
        TEST_ASSERT_TRUE(
            workspace
                .press(snapshot,
                       {device_platform::DeviceUiTargetKind::ContentCell, 0U,
                        0U, 0U})
                .navigated);
        const auto screen = fermentation::main_ui::makeRepresentativeScreen(
            snapshot, workspace, fermentation::makeFermentationUiTextPacks(),
            device_platform::LocaleId{locale});
        for (const char* key : {"message-target-reach-time-exceeded",
                                "message-class-process-warning",
                                "message-acknowledged", "message-muted"}) {
            const auto text = textFor(key, locale);
            TEST_ASSERT_TRUE(!text.empty());
            TEST_ASSERT_TRUE(text.find("fermentation") == std::string::npos);
            TEST_ASSERT_TRUE(hasText(screen, text));
        }
    }

    // A selection that is not in the snapshot draws no detail text.
    fermentation::FermentationTouchWorkspace stale;
    stale.setPage(fermentation::FermentationUiPage::MessageDetail);
    stale.setSelectedMessage(999U);
    const auto screen = fermentation::main_ui::makeRepresentativeScreen(
        snapshot, stale, fermentation::makeFermentationUiTextPacks(),
        device_platform::LocaleId{"en"});
    TEST_ASSERT_FALSE(hasText(screen, textFor("message-acknowledged", "en")));
}

void test_every_message_code_and_class_has_localized_text() {
    using fermentation::MessageClass;
    using fermentation::MessageCode;
    const auto packs = fermentation::makeFermentationUiTextPacks();
    for (const char* locale : {"en", "de", "es"}) {
        for (const auto code :
             {MessageCode::ProductInsertionRequested,
              MessageCode::TargetReachTimeExceeded,
              MessageCode::UserDecisionRequired, MessageCode::RunCompleted,
              MessageCode::RunAborted, MessageCode::RecoveryPending,
              MessageCode::SafetyFault}) {
            const auto key = fermentation::messageCodeTextKey(code);
            const auto result = device_platform::resolveText(
                packs, device_platform::LocaleId{locale}, key);
            TEST_ASSERT_TRUE(result.value != key.visibleTechnicalKey());
        }
        for (const auto messageClass :
             {MessageClass::Information, MessageClass::ProcessWarning,
              MessageClass::Recovery, MessageClass::DecisionRequired,
              MessageClass::SafetyFault}) {
            const auto key = fermentation::messageClassTextKey(messageClass);
            const auto result = device_platform::resolveText(
                packs, device_platform::LocaleId{locale}, key);
            TEST_ASSERT_TRUE(result.value != key.visibleTechnicalKey());
        }
    }
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
    const auto unavailable = keyFor(snapshot, workspace);
    const auto connected =
        keyFor(snapshot, workspace, "en", std::nullopt,
               device_platform::DeviceUiNetworkStatus::Connected);
    const auto clocked =
        keyFor(snapshot, workspace, "en", std::nullopt,
               device_platform::DeviceUiNetworkStatus::Unavailable, 3661);

    TEST_ASSERT_FALSE(unavailable == connected);
    TEST_ASSERT_FALSE(unavailable == clocked);
    // The visible clock is HH:MM, so seconds within one minute must not
    // redraw, while the next minute must.
    TEST_ASSERT_TRUE(clocked ==
                     keyFor(snapshot, workspace, "en", std::nullopt,
                            device_platform::DeviceUiNetworkStatus::Unavailable,
                            3661 + 30));
    TEST_ASSERT_FALSE(
        clocked == keyFor(snapshot, workspace, "en", std::nullopt,
                          device_platform::DeviceUiNetworkStatus::Unavailable,
                          3661 + 60));
}

void test_render_key_unchanged_workspace_remains_equal() {
    fermentation::FermentationUiSnapshot snapshot;
    snapshot.home.mode = fermentation::FermentationHomeMode::Standby;
    fermentation::FermentationTouchWorkspace workspace;

    TEST_ASSERT_TRUE(keyFor(snapshot, workspace) ==
                     keyFor(snapshot, workspace));
}

void test_render_key_changes_when_manual_holding_values_enable_confirm() {
    fermentation::FermentationUiSnapshot snapshot;
    fermentation::FermentationTouchWorkspace workspace;
    workspace.setPage(fermentation::FermentationUiPage::ManualHolding);
    const auto packs = fermentation::makeFermentationUiTextPacks();

    const auto beforeValues = fermentation::main_ui::makeRepresentativeScreen(
        snapshot, workspace, packs, device_platform::LocaleId{"en"});
    TEST_ASSERT_FALSE(beforeValues.workspace.bottomSlots[2].enabled);
    const auto keyBefore = keyFor(snapshot, workspace);

    workspace.setManualHoldingValues(
        fermentation::FermentationUiManualRunPlanValues{});
    const auto afterValues = fermentation::main_ui::makeRepresentativeScreen(
        snapshot, workspace, packs, device_platform::LocaleId{"en"});
    TEST_ASSERT_TRUE(afterValues.workspace.bottomSlots[2].enabled);

    TEST_ASSERT_FALSE(keyBefore == keyFor(snapshot, workspace));
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
    const auto keyBefore = keyFor(snapshot, workspace);

    workspace.setProgramEditCandidate(fermentation::ProgramDocument{});
    const auto afterCandidate = fermentation::main_ui::makeRepresentativeScreen(
        snapshot, workspace, packs, device_platform::LocaleId{"en"});
    TEST_ASSERT_TRUE(afterCandidate.workspace.bottomSlots[3].enabled);

    TEST_ASSERT_FALSE(keyBefore == keyFor(snapshot, workspace));
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
        "Fermentationsschrank", "ACDEFHJKMNPQRTU3", 0x0104A8C0U};
    const auto screen = fermentation::main_ui::makeRepresentativeScreen(
        snapshot, workspace, packs, device_platform::LocaleId{"en"},
        std::nullopt, nullptr,
        device_platform::DeviceUiNetworkStatus::Unavailable, {}, accessPoint);

    TEST_ASSERT_TRUE(hasText(screen, "Home WiFi"));
    TEST_ASSERT_TRUE(hasText(screen, "SSID: Fermentationsschrank"));
    TEST_ASSERT_TRUE(hasText(screen, "Password: ACDEFHJKMNPQRTU3"));
    TEST_ASSERT_TRUE(hasText(screen, "IP: 192.168.4.1"));
    const auto qr =
        std::find_if(screen.commands.begin(), screen.commands.end(),
                     [](const auto& command) {
                         return command.kind ==
                                fermentation::main_ui::ScreenDrawKind::QrCode;
                     });
    TEST_ASSERT_TRUE(qr != screen.commands.end());
    TEST_ASSERT_EQUAL_STRING(
        "WIFI:T:WPA;S:Fermentationsschrank;P:ACDEFHJKMNPQRTU3;;",
        qr->text.c_str());
    TEST_ASSERT_EQUAL_UINT16(156U, qr->rect.left);
    TEST_ASSERT_EQUAL_UINT16(34U, qr->rect.top);
    TEST_ASSERT_EQUAL_UINT16(164U, qr->rect.width);
    TEST_ASSERT_EQUAL_UINT16(164U, qr->rect.height);
    TEST_ASSERT_TRUE(qr->text.find("http") == std::string::npos);
    TEST_ASSERT_TRUE(qr->text.find("192.168.4.1") == std::string::npos);
    TEST_ASSERT_NOT_EQUAL(0U, screen.localNetworkInfoFingerprint);
    const auto ssid =
        std::find_if(screen.commands.begin(), screen.commands.end(),
                     [](const auto& command) {
                         return command.text == "SSID: Fermentationsschrank";
                     });
    const auto password =
        std::find_if(screen.commands.begin(), screen.commands.end(),
                     [](const auto& command) {
                         return command.text == "Password: ACDEFHJKMNPQRTU3";
                     });
    TEST_ASSERT_TRUE(ssid != screen.commands.end());
    TEST_ASSERT_TRUE(password != screen.commands.end());
    TEST_ASSERT_TRUE(ssid->wrapText);
    TEST_ASSERT_TRUE(password->wrapText);
    TEST_ASSERT_EQUAL_UINT16(8U, ssid->rect.left);
    TEST_ASSERT_EQUAL_UINT16(72U, ssid->rect.top);
    TEST_ASSERT_EQUAL_UINT16(140U, ssid->rect.width);
    TEST_ASSERT_EQUAL_UINT16(36U, ssid->rect.height);
    TEST_ASSERT_EQUAL_UINT16(8U, password->rect.left);
    TEST_ASSERT_EQUAL_UINT16(110U, password->rect.top);
    TEST_ASSERT_EQUAL_UINT16(140U, password->rect.width);
    TEST_ASSERT_EQUAL_UINT16(54U, password->rect.height);

    const auto ip = std::find_if(
        screen.commands.begin(), screen.commands.end(),
        [](const auto& command) { return command.text == "IP: 192.168.4.1"; });
    TEST_ASSERT_TRUE(ip != screen.commands.end());
    TEST_ASSERT_EQUAL_UINT16(8U, ip->rect.left);
    TEST_ASSERT_EQUAL_UINT16(166U, ip->rect.top);
    TEST_ASSERT_EQUAL_UINT16(140U, ip->rect.width);
    TEST_ASSERT_EQUAL_UINT16(18U, ip->rect.height);

    const auto title = std::find_if(
        screen.commands.begin(), screen.commands.end(),
        [](const auto& command) { return command.text == "WLAN"; });
    const auto mode = std::find_if(
        screen.commands.begin(), screen.commands.end(),
        [](const auto& command) { return command.text == "Home WiFi"; });
    const auto header = std::find_if(
        screen.commands.begin(), screen.commands.end(),
        [](const auto& command) {
            return command.kind ==
                   fermentation::main_ui::ScreenDrawKind::NetworkStatusIcon;
        });
    TEST_ASSERT_TRUE(title != screen.commands.end());
    TEST_ASSERT_TRUE(mode != screen.commands.end());
    TEST_ASSERT_TRUE(header != screen.commands.end());
    TEST_ASSERT_EQUAL_UINT16(8U, title->rect.left);
    TEST_ASSERT_EQUAL_UINT16(34U, title->rect.top);
    TEST_ASSERT_EQUAL_UINT16(140U, title->rect.width);
    TEST_ASSERT_EQUAL_UINT16(18U, title->rect.height);
    TEST_ASSERT_EQUAL_UINT16(8U, mode->rect.left);
    TEST_ASSERT_EQUAL_UINT16(52U, mode->rect.top);
    TEST_ASSERT_EQUAL_UINT16(140U, mode->rect.width);
    TEST_ASSERT_EQUAL_UINT16(18U, mode->rect.height);

    const std::array<device_platform::DisplayRect, 5U> manualRects{
        title->rect, mode->rect, ssid->rect, password->rect, ip->rect};
    for (const auto& rect : manualRects) {
        assertWithinDisplay(rect);
        TEST_ASSERT_FALSE(overlaps(qr->rect, rect));
    }
    for (std::size_t index = 0U; index < manualRects.size(); ++index) {
        for (std::size_t other = index + 1U; other < manualRects.size();
             ++other) {
            TEST_ASSERT_FALSE(overlaps(manualRects[index], manualRects[other]));
        }
    }
    assertWithinDisplay(qr->rect);
    assertWithinDisplay(header->rect);
    TEST_ASSERT_FALSE(overlaps(qr->rect, header->rect));
    for (const auto& command : screen.commands) {
        assertWithinDisplay(command.rect);
        if (command.rect.top >= 200U &&
            command.kind != fermentation::main_ui::ScreenDrawKind::Fill) {
            TEST_ASSERT_FALSE(overlaps(qr->rect, command.rect));
        }
    }

    auto changedAccessPoint = accessPoint;
    changedAccessPoint.password += "-rotated";
    const auto changedScreen = fermentation::main_ui::makeRepresentativeScreen(
        snapshot, workspace, packs, device_platform::LocaleId{"en"},
        std::nullopt, nullptr,
        device_platform::DeviceUiNetworkStatus::Unavailable, {},
        changedAccessPoint);
    {
        // The key carries the lifecycle's access-point change revision, never
        // the credentials themselves.
        fermentation::FermentationUiSnapshot keySnapshot;
        fermentation::FermentationTouchWorkspace keyWorkspace;
        keyWorkspace.setPage(fermentation::FermentationUiPage::HeaderNetwork);
        const auto network =
            device_platform::DeviceUiNetworkStatus::Unavailable;
        TEST_ASSERT_FALSE(keyFor(keySnapshot, keyWorkspace, "en", std::nullopt,
                                 network, std::nullopt, 1U) ==
                          keyFor(keySnapshot, keyWorkspace, "en", std::nullopt,
                                 network, std::nullopt, 2U));
    }
    const auto changedQr =
        std::find_if(changedScreen.commands.begin(),
                     changedScreen.commands.end(), [](const auto& command) {
                         return command.kind ==
                                fermentation::main_ui::ScreenDrawKind::QrCode;
                     });
    TEST_ASSERT_TRUE(changedQr != changedScreen.commands.end());
    TEST_ASSERT_TRUE(changedQr->text != qr->text);

    const device_platform::DeviceUiTarget heldBottomSlot{
        device_platform::DeviceUiTargetKind::BottomSlot, 2U};
    const auto pressedScreen = fermentation::main_ui::makeRepresentativeScreen(
        snapshot, workspace, packs, device_platform::LocaleId{"en"},
        heldBottomSlot, nullptr,
        device_platform::DeviceUiNetworkStatus::Unavailable, {}, accessPoint);
    TEST_ASSERT_EQUAL_UINT(21U, pressedScreen.commands.size());
    TEST_ASSERT_EQUAL_UINT(21U, pressedScreen.commands.capacity());

    workspace.setPage(fermentation::FermentationUiPage::Home);
    const auto ordinaryScreen = fermentation::main_ui::makeRepresentativeScreen(
        snapshot, workspace, packs, device_platform::LocaleId{"en"},
        std::nullopt, nullptr,
        device_platform::DeviceUiNetworkStatus::Unavailable, {}, accessPoint);
    TEST_ASSERT_FALSE(hasText(ordinaryScreen, "Fermentation"));
    TEST_ASSERT_FALSE(hasText(ordinaryScreen, "ACDEFHJKMNPQRTU3"));
    TEST_ASSERT_EQUAL_UINT64(0U, ordinaryScreen.localNetworkInfoFingerprint);

    for (const auto& command : screen.commands) {
        TEST_ASSERT_LESS_OR_EQUAL_UINT16(
            320U, command.rect.left + command.rect.width);
        TEST_ASSERT_LESS_OR_EQUAL_UINT16(
            240U, command.rect.top + command.rect.height);
    }
}

void test_unselected_network_mode_renders_localized_selection_prompt() {
    fermentation::FermentationUiSnapshot snapshot;
    snapshot.network.currentMode = device_platform::NetworkMode::UNSELECTED;
    fermentation::FermentationTouchWorkspace workspace;
    workspace.setPage(fermentation::FermentationUiPage::HeaderNetwork);
    const auto packs = fermentation::makeFermentationUiTextPacks();

    const auto de = fermentation::main_ui::makeRepresentativeScreen(
        snapshot, workspace, packs, device_platform::LocaleId{"de"});
    const auto en = fermentation::main_ui::makeRepresentativeScreen(
        snapshot, workspace, packs, device_platform::LocaleId{"en"});
    const auto es = fermentation::main_ui::makeRepresentativeScreen(
        snapshot, workspace, packs, device_platform::LocaleId{"es"});

    TEST_ASSERT_TRUE(hasText(de, "Modus waehlen"));
    TEST_ASSERT_TRUE(hasText(en, "Select mode"));
    TEST_ASSERT_TRUE(hasText(es, "Elegir modo"));
    TEST_ASSERT_FALSE(hasText(de, "Nicht ausgewaehlt"));
    TEST_ASSERT_FALSE(hasText(en, "Not selected"));
    TEST_ASSERT_FALSE(hasText(es, "Sin seleccionar"));
    TEST_ASSERT_FALSE(hasText(de, "network-unselected"));
    TEST_ASSERT_FALSE(hasText(en, "network-unselected"));
    TEST_ASSERT_FALSE(hasText(es, "network-unselected"));

    const auto prompt = std::find_if(
        de.commands.begin(), de.commands.end(),
        [](const auto& command) { return command.text == "Modus waehlen"; });
    TEST_ASSERT_TRUE(prompt != de.commands.end());
    TEST_ASSERT_EQUAL_UINT16(8U, prompt->rect.left);
    TEST_ASSERT_EQUAL_UINT16(52U, prompt->rect.top);
    TEST_ASSERT_EQUAL_UINT16(140U, prompt->rect.width);
    TEST_ASSERT_EQUAL_UINT16(
        fermentation::main_ui::RepresentativeScreen::kTextLineHeight,
        prompt->rect.height);
}

void assertNetworkBottomLabelsFit(
    const fermentation::main_ui::RepresentativeScreen& screen) {
    std::size_t labelCount = 0U;
    for (const auto& command : screen.commands) {
        if (command.kind != fermentation::main_ui::ScreenDrawKind::Text ||
            command.rect.top < 200U || command.rect.width != 76U) {
            continue;
        }
        TEST_ASSERT_LESS_OR_EQUAL_UINT16(
            320U, command.rect.left + command.rect.width);
        TEST_ASSERT_LESS_OR_EQUAL_UINT16(
            240U, command.rect.top + command.rect.height);
        ++labelCount;
    }
    TEST_ASSERT_EQUAL_UINT(4U, labelCount);

    constexpr std::array<std::uint16_t, 4U> slotCenters{40U, 120U, 200U, 280U};
    for (const auto x : slotCenters) {
        const auto target = fermentation::main_ui::targetAt(
            screen, static_cast<std::uint16_t>(x), 220U);
        TEST_ASSERT_TRUE(target.has_value());
        TEST_ASSERT_EQUAL(
            static_cast<int>(device_platform::DeviceUiTargetKind::BottomSlot),
            static_cast<int>(target->kind));
        TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(x / 80U),
                                target->slotIndex);
    }
}

void test_network_action_labels_fit_without_changing_bottom_hit_targets() {
    fermentation::FermentationUiSnapshot snapshot;
    fermentation::FermentationTouchWorkspace workspace;
    workspace.setPage(fermentation::FermentationUiPage::HeaderNetwork);
    const auto packs = fermentation::makeFermentationUiTextPacks();
    const auto de = fermentation::main_ui::makeRepresentativeScreen(
        snapshot, workspace, packs, device_platform::LocaleId{"de"});
    const auto en = fermentation::main_ui::makeRepresentativeScreen(
        snapshot, workspace, packs, device_platform::LocaleId{"en"});
    const auto es = fermentation::main_ui::makeRepresentativeScreen(
        snapshot, workspace, packs, device_platform::LocaleId{"es"});

    TEST_ASSERT_TRUE(hasText(de, "Nur AP"));
    TEST_ASSERT_TRUE(hasText(de, "Heimnetz"));
    TEST_ASSERT_TRUE(hasText(de, "Setup"));
    TEST_ASSERT_TRUE(hasText(en, "AP only"));
    TEST_ASSERT_TRUE(hasText(en, "Home WiFi"));
    TEST_ASSERT_TRUE(hasText(en, "WiFi setup"));
    TEST_ASSERT_TRUE(hasText(es, "Solo AP"));
    TEST_ASSERT_TRUE(hasText(es, "WiFi casa"));
    TEST_ASSERT_TRUE(hasText(es, "Ajustes"));

    assertNetworkBottomLabelsFit(de);
    assertNetworkBottomLabelsFit(en);
    assertNetworkBottomLabelsFit(es);
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

void test_wifi_qr_max_payload_keeps_pinned_lvgl_geometry_contract() {
    constexpr std::uint16_t kQrCanvas = 164U;
    constexpr std::uint16_t kQrEffectiveVersion = 5U;
    constexpr std::uint16_t kQrModuleCount = 37U;
    constexpr std::uint16_t kQrModuleScale = 4U;
    constexpr std::uint16_t kQrMarginPerSide = 8U;
    TEST_ASSERT_EQUAL_UINT16(
        kQrCanvas, kQrModuleCount * kQrModuleScale + 2U * kQrMarginPerSide);
    TEST_ASSERT_EQUAL_UINT16(148U, kQrModuleCount * kQrModuleScale);
    TEST_ASSERT_EQUAL_UINT16(8U, kQrCanvas - 148U - kQrMarginPerSide);
    TEST_ASSERT_EQUAL_UINT16(5U, kQrEffectiveVersion);

    fermentation::FermentationUiSnapshot snapshot;
    snapshot.network.currentMode = device_platform::NetworkMode::AP_ONLY;
    fermentation::FermentationTouchWorkspace workspace;
    workspace.setPage(fermentation::FermentationUiPage::HeaderNetwork);
    const auto accessPoint = device_platform::NetworkAccessPointInfo{
        std::string(14U, '\\'), std::string(16U, 'A'), 0x0104A8C0U};
    const auto payload =
        fermentation::main_ui::makeSoftApWifiQrPayload(accessPoint);
    TEST_ASSERT_TRUE(payload.has_value());
    TEST_ASSERT_EQUAL_UINT(62U, payload->size());

    const auto screen = fermentation::main_ui::makeRepresentativeScreen(
        snapshot, workspace, fermentation::makeFermentationUiTextPacks(),
        device_platform::LocaleId{"en"}, std::nullopt, nullptr,
        device_platform::DeviceUiNetworkStatus::Unavailable, {}, accessPoint);
    const auto qr =
        std::find_if(screen.commands.begin(), screen.commands.end(),
                     [](const auto& command) {
                         return command.kind ==
                                fermentation::main_ui::ScreenDrawKind::QrCode;
                     });
    TEST_ASSERT_TRUE(qr != screen.commands.end());
    TEST_ASSERT_EQUAL_STRING(payload->c_str(), qr->text.c_str());
    TEST_ASSERT_EQUAL_UINT16(156U, qr->rect.left);
    TEST_ASSERT_EQUAL_UINT16(34U, qr->rect.top);
    TEST_ASSERT_EQUAL_UINT16(164U, qr->rect.width);
    TEST_ASSERT_EQUAL_UINT16(164U, qr->rect.height);
    assertWithinDisplay(qr->rect);
}

void test_web_access_page_shows_the_application_state_in_all_locales() {
    const auto packs = fermentation::makeFermentationUiTextPacks();
    struct Expectation {
        const char* locale;
        fermentation::FermentationWebAccessState state;
        const char* title;
        const char* status;
    };
    const Expectation expectations[] = {
        {"en", fermentation::FermentationWebAccessState::Closed, "Web access",
         "Web setup not allowed yet"},
        {"en", fermentation::FermentationWebAccessState::WindowOpen,
         "Web access", "Web setup allowed (10 min)"},
        {"en", fermentation::FermentationWebAccessState::NotApplicable,
         "Web access", "Web setup not available"},
        {"de", fermentation::FermentationWebAccessState::NotApplicable,
         "Webzugang", "Web-Setup nicht verfuegbar"},
        {"es", fermentation::FermentationWebAccessState::NotApplicable,
         "Acceso web", "Config. web no disponible"},
        {"de", fermentation::FermentationWebAccessState::Closed, "Webzugang",
         "Web-Setup nicht freigegeben"},
        {"de", fermentation::FermentationWebAccessState::WindowOpen,
         "Webzugang", "Web-Setup frei (10 Min)"},
        {"es", fermentation::FermentationWebAccessState::Closed, "Acceso web",
         "Config. web no permitida"},
        {"es", fermentation::FermentationWebAccessState::WindowOpen,
         "Acceso web", "Config. web permitida (10 min)"},
    };
    for (const auto& expected : expectations) {
        fermentation::FermentationUiSnapshot snapshot;
        snapshot.webAccess = expected.state;
        fermentation::FermentationTouchWorkspace workspace;
        workspace.setPage(fermentation::FermentationUiPage::HeaderWebAccess);
        const auto screen = fermentation::main_ui::makeRepresentativeScreen(
            snapshot, workspace, packs,
            device_platform::LocaleId{expected.locale}, std::nullopt, nullptr,
            device_platform::DeviceUiNetworkStatus::Connected, {},
            std::nullopt);
        TEST_ASSERT_TRUE(hasText(screen, expected.title));
        TEST_ASSERT_TRUE(hasText(screen, expected.status));
        // No stale network page content is drawn on this page.
        TEST_ASSERT_FALSE(hasText(screen, "SSID: "));
        // No state may claim a successful setup.
        TEST_ASSERT_FALSE(hasText(screen, "Web access is set up"));
        TEST_ASSERT_FALSE(hasText(screen, "Webzugang ist eingerichtet"));
        TEST_ASSERT_FALSE(hasText(screen, "Acceso web configurado"));
        for (const auto& command : screen.commands) {
            TEST_ASSERT_TRUE(command.rect.left + command.rect.width <=
                             screen.kWidth);
            TEST_ASSERT_TRUE(command.rect.top + command.rect.height <=
                             screen.kHeight);
        }
    }
}

void test_language_page_offers_the_web_access_entry_with_a_localized_label() {
    const auto packs = fermentation::makeFermentationUiTextPacks();
    fermentation::FermentationUiSnapshot snapshot;
    fermentation::FermentationTouchWorkspace workspace;
    workspace.setPage(fermentation::FermentationUiPage::HeaderLanguage);
    const auto view = workspace.view(snapshot);
    TEST_ASSERT_TRUE(view.bottomSlots[3].enabled);
    TEST_ASSERT_TRUE(view.bottomSlots[3].label ==
                     fermentation::fermentationTextKey("web-access"));
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
    RUN_TEST(test_program_list_rows_are_40px_touch_rows_with_exact_hit_zones);
    RUN_TEST(
        test_program_list_window_follows_the_pager_and_hits_follow_the_window);
    RUN_TEST(test_program_list_hit_rows_exist_only_on_the_program_list_page);
    RUN_TEST(test_held_program_row_renders_press_feedback_for_that_row_only);
    RUN_TEST(test_pager_counter_sits_in_the_title_row_clear_of_the_rows);
    RUN_TEST(test_not_startable_program_row_is_dimmed_and_stays_hittable);
    RUN_TEST(test_program_summary_names_the_reason_of_a_not_startable_program);
    RUN_TEST(test_message_list_rows_are_hittable_and_follow_the_pager_window);
    RUN_TEST(test_empty_message_list_has_no_hittable_rows);
    RUN_TEST(test_message_detail_shows_code_class_and_state_in_all_locales);
    RUN_TEST(test_every_message_code_and_class_has_localized_text);
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
    RUN_TEST(test_unselected_network_mode_renders_localized_selection_prompt);
    RUN_TEST(
        test_network_action_labels_fit_without_changing_bottom_hit_targets);
    RUN_TEST(test_softap_wifi_qr_escapes_reserved_characters_deterministically);
    RUN_TEST(test_wifi_qr_max_payload_keeps_pinned_lvgl_geometry_contract);
    RUN_TEST(test_web_access_page_shows_the_application_state_in_all_locales);
    RUN_TEST(
        test_language_page_offers_the_web_access_entry_with_a_localized_label);
    RUN_TEST(test_network_page_missing_softap_info_is_explicit);
    return UNITY_END();
}
