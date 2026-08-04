#include <unity.h>

#include "run_checkpoint_schedule.hpp"

namespace {

using namespace fermentation;

void test_schedule_is_unarmed_until_confirmed_then_uses_explicit_time() {
    RunCheckpointSchedule schedule{5U};
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RunCheckpointScheduleStatus::NotDue),
                          static_cast<int>(schedule.due(1U)));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunCheckpointScheduleStatus::Success),
        static_cast<int>(schedule.validate(100U)));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunCheckpointScheduleStatus::Success),
        static_cast<int>(schedule.confirm(100U)));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RunCheckpointScheduleStatus::NotDue),
                          static_cast<int>(schedule.due(299999U)));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunCheckpointScheduleStatus::Success),
        static_cast<int>(schedule.due(300100U)));
}

void test_schedule_rejects_backwards_time_without_mutating_confirmation() {
    RunCheckpointSchedule schedule{5U};
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunCheckpointScheduleStatus::Success),
        static_cast<int>(schedule.confirm(500U)));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunCheckpointScheduleStatus::TimeWentBackwards),
        static_cast<int>(schedule.validate(499U)));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunCheckpointScheduleStatus::TimeWentBackwards),
        static_cast<int>(schedule.due(499U)));
}

}  // namespace

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_schedule_is_unarmed_until_confirmed_then_uses_explicit_time);
    RUN_TEST(
        test_schedule_rejects_backwards_time_without_mutating_confirmation);
    return UNITY_END();
}
