#include <unity.h>

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

}  // namespace

void setUp() {}
void tearDown() {}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_numeric_edit_model_keeps_actions_transient);
    RUN_TEST(test_text_edit_model_has_mode_and_commit_without_validation);
    RUN_TEST(
        test_user_program_id_allocation_is_deterministic_and_non_overwriting);
    return UNITY_END();
}
