#include <unity.h>

#include <algorithm>

#include "configuration_text.hpp"
#include "fermentation_touch_workspace.hpp"
#include "fermentation_ui_editing.hpp"
#include "standard_program_catalog.hpp"

namespace {

using namespace fermentation;

void test_numeric_edit_model_keeps_actions_transient() {
    NumericEditModel model;
    TEST_ASSERT_TRUE(model.apply({NumericEditAction::Digit, 2U}));
    TEST_ASSERT_TRUE(model.apply({NumericEditAction::Digit, 5U}));
    TEST_ASSERT_TRUE(model.apply({NumericEditAction::DecimalSeparator, 0U}));
    TEST_ASSERT_TRUE(model.apply({NumericEditAction::Digit, 5U}));
    TEST_ASSERT_EQUAL_STRING("25.5", model.candidate().c_str());
    TEST_ASSERT_TRUE(model.apply({NumericEditAction::Commit, 0U}));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(NumericEditState::Committed),
                          static_cast<int>(model.state()));
    TEST_ASSERT_DOUBLE_WITHIN(0.001, 25.5, model.committedValue().value());
    TEST_ASSERT_FALSE(model.apply({NumericEditAction::Digit, 9U}));
    model.reset();
    TEST_ASSERT_TRUE(model.apply({NumericEditAction::Minus, 0U}));
    TEST_ASSERT_TRUE(model.apply({NumericEditAction::Digit, 1U}));
    TEST_ASSERT_TRUE(model.apply({NumericEditAction::Backspace, 0U}));
    TEST_ASSERT_EQUAL_STRING("-", model.candidate().c_str());
    TEST_ASSERT_TRUE(model.apply({NumericEditAction::Cancel, 0U}));
    TEST_ASSERT_TRUE(model.candidate().empty());
}

// S8 keypad (D8) drives the unchanged model: editing an existing value, the
// decimal key on an empty candidate, the sign key, clear and commit.
void test_keypad_key_sequences_commit_the_edited_value() {
    NumericEditModel model{"25.0"};
    TEST_ASSERT_TRUE(model.apply({NumericEditAction::Backspace, 0U}));
    TEST_ASSERT_TRUE(model.apply({NumericEditAction::Backspace, 0U}));
    TEST_ASSERT_TRUE(model.apply({NumericEditAction::Digit, 7U}));
    TEST_ASSERT_EQUAL_STRING("257", model.candidate().c_str());
    TEST_ASSERT_TRUE(model.apply({NumericEditAction::Commit, 0U}));
    TEST_ASSERT_DOUBLE_WITHIN(0.001, 257.0, model.committedValue().value());

    model.reset();
    TEST_ASSERT_TRUE(model.apply({NumericEditAction::DecimalSeparator, 0U}));
    TEST_ASSERT_EQUAL_STRING("0.", model.candidate().c_str());
    // A second decimal separator is refused.
    TEST_ASSERT_FALSE(model.apply({NumericEditAction::DecimalSeparator, 0U}));
    TEST_ASSERT_TRUE(model.apply({NumericEditAction::Digit, 5U}));
    TEST_ASSERT_TRUE(model.apply({NumericEditAction::Minus, 0U}));
    TEST_ASSERT_EQUAL_STRING("-0.5", model.candidate().c_str());
    TEST_ASSERT_TRUE(model.apply({NumericEditAction::Minus, 0U}));
    TEST_ASSERT_EQUAL_STRING("0.5", model.candidate().c_str());

    model.reset("12");
    TEST_ASSERT_TRUE(model.apply({NumericEditAction::Clear, 0U}));
    TEST_ASSERT_TRUE(model.candidate().empty());
    // An empty candidate cannot be committed; the model stays editing.
    TEST_ASSERT_FALSE(model.apply({NumericEditAction::Commit, 0U}));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(NumericEditState::Editing),
                          static_cast<int>(model.state()));
}

// S10 keyboard (O3): the model removes whole Unicode scalars, so a prefilled
// multi-byte name never ends up cut inside a character; the owning text rules
// decide validity.
void test_text_backspace_removes_whole_scalars() {
    // "K\xC3\xBCche \xE2\x82\xAC" = K ü c h e space euro (2- and 3-byte)
    TextEditModel model{
        "K\xC3\xBC"
        "che \xE2\x82\xAC"};
    TEST_ASSERT_TRUE(model.apply({TextEditAction::Backspace, ' '}));
    TEST_ASSERT_EQUAL_STRING(
        "K\xC3\xBC"
        "che ",
        model.candidate().c_str());
    for (int step = 0; step < 4; ++step)
        TEST_ASSERT_TRUE(model.apply({TextEditAction::Backspace, ' '}));
    TEST_ASSERT_EQUAL_STRING("K\xC3\xBC", model.candidate().c_str());
    TEST_ASSERT_TRUE(model.apply({TextEditAction::Backspace, ' '}));
    TEST_ASSERT_EQUAL_STRING("K", model.candidate().c_str());
    TEST_ASSERT_TRUE(model.apply({TextEditAction::Backspace, ' '}));
    TEST_ASSERT_TRUE(model.candidate().empty());
    TEST_ASSERT_FALSE(model.apply({TextEditAction::Backspace, ' '}));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(ConfigurationTextStatus::TooShort),
        static_cast<int>(validateVisibleName(model.candidate())));
}

void test_keyboard_text_sequences_meet_the_owning_name_rules() {
    TextEditModel model;
    for (const char c : std::string("Gaerschrank 1")) {
        TEST_ASSERT_TRUE(model.apply({TextEditAction::Character, c}));
    }
    TEST_ASSERT_EQUAL_STRING("Gaerschrank 1", model.candidate().c_str());
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(ConfigurationTextStatus::Success),
        static_cast<int>(validateVisibleName(model.candidate())));
    // A trailing space (the Space key) violates the name rule; the commit slot
    // follows the rule, not the model.
    TEST_ASSERT_TRUE(model.apply({TextEditAction::Character, ' '}));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(ConfigurationTextStatus::LeadingOrTrailingWhitespace),
        static_cast<int>(validateVisibleName(model.candidate())));
    TEST_ASSERT_TRUE(model.apply({TextEditAction::Clear, ' '}));
    TEST_ASSERT_TRUE(model.candidate().empty());
    // 48 characters are the upper bound; the 49th is refused by the rule.
    for (int index = 0; index < 48; ++index)
        TEST_ASSERT_TRUE(model.apply({TextEditAction::Character, 'a'}));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(ConfigurationTextStatus::Success),
        static_cast<int>(validateVisibleName(model.candidate())));
    TEST_ASSERT_TRUE(model.apply({TextEditAction::Character, 'a'}));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(ConfigurationTextStatus::TooManyScalars),
        static_cast<int>(validateVisibleName(model.candidate())));
    // Notes: empty is valid, the limit is the notes rule.
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ConfigurationTextStatus::Success),
                          static_cast<int>(validateProgramNotes("")));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(ConfigurationTextStatus::Success),
        static_cast<int>(validateProgramNotes(std::string(512U, 'n'))));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(ConfigurationTextStatus::TooManyScalars),
        static_cast<int>(validateProgramNotes(std::string(513U, 'n'))));
}

void test_keyboard_layout_is_one_definition_with_wide_bottom_keys() {
    using Kind = FermentationUiKeyboardKeyKind;
    // Row 3 is the same in every mode: Clear (0-1), Space (2-7), `-`, `.`.
    for (const auto mode : {TextEditMode::Lowercase, TextEditMode::Uppercase,
                            TextEditMode::Digits, TextEditMode::Symbols}) {
        TEST_ASSERT_TRUE(fermentationUiKeyboardKeyAt(mode, 3U, 0U).kind ==
                         Kind::Clear);
        TEST_ASSERT_TRUE(fermentationUiKeyboardKeyAt(mode, 3U, 1U).kind ==
                         Kind::Clear);
        for (std::uint8_t column = 2U; column < 8U; ++column) {
            const auto key = fermentationUiKeyboardKeyAt(mode, 3U, column);
            TEST_ASSERT_TRUE(key.kind == Kind::Character);
            TEST_ASSERT_EQUAL_CHAR(' ', key.character);
        }
        TEST_ASSERT_EQUAL_CHAR(
            '-', fermentationUiKeyboardKeyAt(mode, 3U, 8U).character);
        TEST_ASSERT_EQUAL_CHAR(
            '.', fermentationUiKeyboardKeyAt(mode, 3U, 9U).character);
        // Outside the grid there is no key.
        TEST_ASSERT_TRUE(fermentationUiKeyboardKeyAt(mode, 4U, 0U).kind ==
                         Kind::None);
        TEST_ASSERT_TRUE(fermentationUiKeyboardKeyAt(mode, 0U, 10U).kind ==
                         Kind::None);
    }
    // Every ASCII letter is reachable in the letter modes, every digit in the
    // digit mode (a name or note can be typed without leaving ASCII).
    std::string letters;
    std::string upper;
    std::string digits;
    for (std::uint8_t row = 0U; row < 3U; ++row) {
        for (std::uint8_t column = 0U; column < 10U; ++column) {
            const auto lower = fermentationUiKeyboardKeyAt(
                TextEditMode::Lowercase, row, column);
            if (lower.kind == Kind::Character) letters += lower.character;
            const auto big = fermentationUiKeyboardKeyAt(
                TextEditMode::Uppercase, row, column);
            if (big.kind == Kind::Character) upper += big.character;
            const auto number =
                fermentationUiKeyboardKeyAt(TextEditMode::Digits, row, column);
            if (number.kind == Kind::Character) digits += number.character;
        }
    }
    TEST_ASSERT_EQUAL_STRING("abcdefghijklmnopqrstuvwxyz", letters.c_str());
    TEST_ASSERT_EQUAL_STRING("ABCDEFGHIJKLMNOPQRSTUVWXYZ", upper.c_str());
    TEST_ASSERT_TRUE(digits.find("1234567890") == 0U);
}

void test_text_edit_model_has_mode_and_commit_without_validation() {
    TextEditModel model;
    TEST_ASSERT_TRUE(model.apply({TextEditAction::Character, 'A'}));
    TEST_ASSERT_TRUE(model.apply({TextEditAction::Mode, 0}));
    TEST_ASSERT_TRUE(model.apply({TextEditAction::Character, '!'}));
    TEST_ASSERT_EQUAL_STRING("A!", model.candidate().c_str());
    TEST_ASSERT_TRUE(model.apply({TextEditAction::Commit, 0}));
    TEST_ASSERT_TRUE(model.committedValue().has_value());
    TEST_ASSERT_EQUAL_STRING("A!", model.committedValue()->c_str());
}

void test_user_program_id_allocation_is_deterministic_and_non_overwriting() {
    auto catalog = makeFactoryProgramCatalog();
    const auto first = allocateNextUserProgramId(catalog);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(UserProgramIdAllocationStatus::Allocated),
        static_cast<int>(first.status));
    TEST_ASSERT_EQUAL_STRING("user-00", first.id->c_str());
    auto user = FactoryProgramCatalog::makeUserCopy(
        catalog.programs.front().program.id, "user-00", "Copy");
    TEST_ASSERT_TRUE(user.has_value());
    catalog.programs.push_back(*user);
    const auto second = allocateNextUserProgramId(catalog);
    TEST_ASSERT_EQUAL_STRING("user-01", second.id->c_str());
}

// SIM-26-06, SIM-26-41, SIM-26-42, SIM-26-43, SIM-26-52, SIM-26-53 and
// SIM-26-54: the list is a catalog projection and every mutation retains the
// canonical factory/user marker and ID rules.
void test_program_list_and_mutations_use_catalog_ownership() {
    const auto noUsage =
        makeFermentationUiProgramUsageEvidence(RunCommandState{});
    auto catalog = makeFactoryProgramCatalog();
    auto& factory = catalog.programs.back().program;
    factory.fermentationStages.front().targetTemperatureCelsius = 25.0;
    factory.fermentationStages.front().durationMinutes = 60U;
    factory.targetQualification.bandCelsius = 0.5;
    factory.targetQualification.durationMinutes = 10U;
    factory.maximumTargetReachMinutes = 180U;
    factory.productSensorFailure.fallbackDelaySeconds = 60U;

    auto list = makeFermentationUiProgramList(catalog);
    TEST_ASSERT_EQUAL_UINT32(4U, list.size());
    TEST_ASSERT_TRUE(list[0].program.program.factoryCatalogEntry);
    TEST_ASSERT_TRUE(list[3].startable);

    catalog.programs[0].program.installed = false;
    list = makeFermentationUiProgramList(catalog);
    TEST_ASSERT_EQUAL_UINT32(3U, list.size());
    TEST_ASSERT_TRUE(std::all_of(
        list.begin(), list.end(),
        [](const auto& entry) { return entry.program.program.installed; }));
    TEST_ASSERT_FALSE(catalog.programs[0].program.installed);

    catalog.programs[1].program.enabled = false;
    list = makeFermentationUiProgramList(catalog);
    const auto disabled =
        std::find_if(list.begin(), list.end(), [](const auto& entry) {
            return entry.program.program.id == "yogurt-firm";
        });
    TEST_ASSERT_TRUE(disabled != list.end());
    TEST_ASSERT_FALSE(disabled->startable);
    TEST_ASSERT_TRUE(disabled->blockedReason.has_value());

    const auto sourceId = factory.id;
    const auto copied =
        applyProgramEdit(catalog,
                         {FermentationUiProgramEditOperation::Copy, sourceId,
                          std::nullopt, std::string{"Copy"}, true},
                         noUsage);
    TEST_ASSERT_TRUE(copied.status == FermentationUiProgramEditStatus::Applied);
    TEST_ASSERT_EQUAL_STRING("user-00", copied.affectedProgramId->c_str());
    TEST_ASSERT_EQUAL_UINT32(5U, catalog.programs.size());
    TEST_ASSERT_TRUE(catalog.programs.back().program.userDeletable);
    TEST_ASSERT_FALSE(catalog.programs.back().program.factoryCatalogEntry);

    auto newCandidate = catalog.programs.back();
    newCandidate.program.id = "temporary";
    newCandidate.program.name = "New catalog candidate";
    const auto created =
        applyProgramEdit(catalog,
                         {FermentationUiProgramEditOperation::New, "",
                          newCandidate, std::nullopt, true},
                         noUsage);
    TEST_ASSERT_TRUE(created.status ==
                     FermentationUiProgramEditStatus::Applied);
    TEST_ASSERT_EQUAL_STRING("user-01", created.affectedProgramId->c_str());

    catalog.programs[1].program.name = "changed";
    const auto reset = applyProgramEdit(
        catalog,
        {FermentationUiProgramEditOperation::Reset,
         catalog.programs[1].program.id, std::nullopt, std::nullopt, true},
        noUsage);
    TEST_ASSERT_TRUE(reset.status == FermentationUiProgramEditStatus::Applied);
    TEST_ASSERT_EQUAL_STRING("Joghurt stichfest",
                             catalog.programs[1].program.name.c_str());

    const auto uninstall = applyProgramEdit(
        catalog,
        {FermentationUiProgramEditOperation::Uninstall,
         catalog.programs[0].program.id, std::nullopt, std::nullopt, true},
        noUsage);
    TEST_ASSERT_TRUE(uninstall.status ==
                     FermentationUiProgramEditStatus::Applied);
    TEST_ASSERT_FALSE(catalog.programs[0].program.installed);

    const auto deletion =
        applyProgramEdit(catalog,
                         {FermentationUiProgramEditOperation::Delete, "user-00",
                          std::nullopt, std::nullopt, false},
                         noUsage);
    TEST_ASSERT_TRUE(deletion.status ==
                     FermentationUiProgramEditStatus::ConfirmationRequired);
    const auto deleted =
        applyProgramEdit(catalog,
                         {FermentationUiProgramEditOperation::Delete, "user-00",
                          std::nullopt, std::nullopt, true},
                         noUsage);
    TEST_ASSERT_TRUE(deleted.status ==
                     FermentationUiProgramEditStatus::Applied);
    TEST_ASSERT_TRUE(std::none_of(
        catalog.programs.begin(), catalog.programs.end(),
        [](const auto& entry) { return entry.program.id == "user-00"; }));
}

}  // namespace

void setUp() {}
void tearDown() {}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_numeric_edit_model_keeps_actions_transient);
    RUN_TEST(test_keypad_key_sequences_commit_the_edited_value);
    RUN_TEST(test_text_edit_model_has_mode_and_commit_without_validation);
    RUN_TEST(test_text_backspace_removes_whole_scalars);
    RUN_TEST(test_keyboard_text_sequences_meet_the_owning_name_rules);
    RUN_TEST(test_keyboard_layout_is_one_definition_with_wide_bottom_keys);
    RUN_TEST(
        test_user_program_id_allocation_is_deterministic_and_non_overwriting);
    RUN_TEST(test_program_list_and_mutations_use_catalog_ownership);
    return UNITY_END();
}
