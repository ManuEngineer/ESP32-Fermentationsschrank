#include <unity.h>

#include <algorithm>

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
    const auto copied = applyProgramEdit(
        catalog, {FermentationUiProgramEditOperation::Copy, sourceId,
                  std::nullopt, std::string{"Copy"}, true, false});
    TEST_ASSERT_TRUE(copied.status == FermentationUiProgramEditStatus::Applied);
    TEST_ASSERT_EQUAL_STRING("user-00", copied.affectedProgramId->c_str());
    TEST_ASSERT_EQUAL_UINT32(5U, catalog.programs.size());
    TEST_ASSERT_TRUE(catalog.programs.back().program.userDeletable);
    TEST_ASSERT_FALSE(catalog.programs.back().program.factoryCatalogEntry);

    auto newCandidate = catalog.programs.back();
    newCandidate.program.id = "temporary";
    newCandidate.program.name = "New catalog candidate";
    const auto created =
        applyProgramEdit(catalog, {FermentationUiProgramEditOperation::New, "",
                                   newCandidate, std::nullopt, true, false});
    TEST_ASSERT_TRUE(created.status ==
                     FermentationUiProgramEditStatus::Applied);
    TEST_ASSERT_EQUAL_STRING("user-01", created.affectedProgramId->c_str());

    catalog.programs[1].program.name = "changed";
    const auto reset =
        applyProgramEdit(catalog, {FermentationUiProgramEditOperation::Reset,
                                   catalog.programs[1].program.id, std::nullopt,
                                   std::nullopt, true, false});
    TEST_ASSERT_TRUE(reset.status == FermentationUiProgramEditStatus::Applied);
    TEST_ASSERT_EQUAL_STRING("Joghurt stichfest",
                             catalog.programs[1].program.name.c_str());

    const auto uninstall = applyProgramEdit(
        catalog, {FermentationUiProgramEditOperation::Uninstall,
                  catalog.programs[0].program.id, std::nullopt, std::nullopt,
                  true, false});
    TEST_ASSERT_TRUE(uninstall.status ==
                     FermentationUiProgramEditStatus::Applied);
    TEST_ASSERT_FALSE(catalog.programs[0].program.installed);

    const auto deletion = applyProgramEdit(
        catalog, {FermentationUiProgramEditOperation::Delete, "user-00",
                  std::nullopt, std::nullopt, false, false});
    TEST_ASSERT_TRUE(deletion.status ==
                     FermentationUiProgramEditStatus::ConfirmationRequired);
    const auto deleted = applyProgramEdit(
        catalog, {FermentationUiProgramEditOperation::Delete, "user-00",
                  std::nullopt, std::nullopt, true, false});
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
    RUN_TEST(test_text_edit_model_has_mode_and_commit_without_validation);
    RUN_TEST(
        test_user_program_id_allocation_is_deterministic_and_non_overwriting);
    RUN_TEST(test_program_list_and_mutations_use_catalog_ownership);
    return UNITY_END();
}
