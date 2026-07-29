#include <unity.h>

#include <utility>

#include "configuration_mutation_coordinator.hpp"

namespace {

void test_only_one_shared_lease_can_be_held() {
    fermentation::ConfigurationMutationCoordinator coordinator;
    auto first = coordinator.tryAcquire();
    TEST_ASSERT_TRUE(
        first.status ==
        fermentation::ConfigurationMutationAcquireStatus::Acquired);
    TEST_ASSERT_TRUE(first.lease.valid());

    auto second = coordinator.tryAcquire();
    TEST_ASSERT_TRUE(second.status ==
                     fermentation::ConfigurationMutationAcquireStatus::
                         ConfigurationMutationBusy);
    TEST_ASSERT_FALSE(second.lease.valid());
}

void test_move_transfers_and_releases_exactly_once() {
    fermentation::ConfigurationMutationCoordinator coordinator;
    {
        auto acquired = coordinator.tryAcquire();
        auto moved = std::move(acquired.lease);
        TEST_ASSERT_FALSE(acquired.lease.valid());
        TEST_ASSERT_TRUE(moved.valid());
        auto blocked = coordinator.tryAcquire();
        TEST_ASSERT_TRUE(blocked.status ==
                         fermentation::ConfigurationMutationAcquireStatus::
                             ConfigurationMutationBusy);
    }
    auto next = coordinator.tryAcquire();
    TEST_ASSERT_TRUE(
        next.status ==
        fermentation::ConfigurationMutationAcquireStatus::Acquired);
}

void test_all_consumers_share_the_same_gate() {
    fermentation::ConfigurationMutationCoordinator coordinator;
    auto configurationServiceLike = coordinator.tryAcquire();
    auto migrationLike = coordinator.tryAcquire();
    auto bootstrapResetLike = coordinator.tryAcquire();
    TEST_ASSERT_TRUE(
        configurationServiceLike.status ==
        fermentation::ConfigurationMutationAcquireStatus::Acquired);
    TEST_ASSERT_TRUE(migrationLike.status ==
                     fermentation::ConfigurationMutationAcquireStatus::
                         ConfigurationMutationBusy);
    TEST_ASSERT_TRUE(bootstrapResetLike.status ==
                     fermentation::ConfigurationMutationAcquireStatus::
                         ConfigurationMutationBusy);
}

}  // namespace

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_only_one_shared_lease_can_be_held);
    RUN_TEST(test_move_transfers_and_releases_exactly_once);
    RUN_TEST(test_all_consumers_share_the_same_gate);
    return UNITY_END();
}
