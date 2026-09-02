#include <unity.h>

#include "fermentation_ui_projector.hpp"

namespace {

using namespace fermentation;

device_platform::SensorQualitySnapshot quality(double value) {
    device_platform::SensorQualitySnapshot snapshot;
    snapshot.filteredCelsius = value;
    snapshot.quality = device_platform::SensorQuality::Valid;
    return snapshot;
}

void test_projector_builds_shared_snapshot_without_surface_state() {
    RunCommandState state;
    state.processState.state = ProcessState::Fermenting;
    state.activeRunId = "shared-run";
    state.activeManualRun = ManualRunPlan{};

    FermentationUiProjectionInput input;
    input.runState = &state;
    input.revisions.expectedStateSequence = 7U;
    input.temperatures.push_back(
        {FermentationTemperatureRole::CabinetAir, 21.5, quality(21.5)});
    RuntimeMessage message;
    message.id = 9U;
    message.revision = 3U;
    state.messages[0] = message;
    state.messageCount = 1U;
    input.semanticActions.push_back(
        {device_platform::TextNamespace{"fermentation"}, "start"});
    input.primaryAction =
        device_platform::TextKey{device_platform::TextNamespace{"fermentation"},
                                 "start"};
    const auto snapshot = FermentationUiProjector::project(input);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(FermentationHomeMode::ActiveRun),
                          static_cast<int>(snapshot.home.mode));
    TEST_ASSERT_EQUAL_STRING("shared-run", snapshot.home.activeRunId.c_str());
    TEST_ASSERT_EQUAL_UINT32(1U, snapshot.temperatures.size());
    TEST_ASSERT_EQUAL_DOUBLE(21.5, snapshot.temperatures.front().valueCelsius.value());
    TEST_ASSERT_EQUAL_UINT32(1U, snapshot.messages.size());
    TEST_ASSERT_EQUAL_UINT32(7U, snapshot.revisions.expectedStateSequence);
    TEST_ASSERT_TRUE(snapshot.navigation.semanticActions.front().valid());
    TEST_ASSERT_TRUE(snapshot.home.primaryAction.valid());
}

void test_projector_marks_fallback_only_from_canonical_pending_state() {
    RunCommandState state;
    FermentationUiProjectionInput input;
    input.runState = &state;
    input.persistenceLoadStatus = RunPersistenceLoadStatus::FallbackRecovered;
    input.coordinatorState = RunPersistenceCoordinatorState::FallbackRecoveryPending;
    const auto snapshot = FermentationUiProjector::project(input);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RecoveryViewMode::FallbackSelectionRequired),
        static_cast<int>(snapshot.recovery.mode));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(FermentationHomeMode::Recovery),
                          static_cast<int>(snapshot.home.mode));
}

void test_projector_marks_recovery_home_from_canonical_disposition() {
    RunCommandState state;
    FermentationUiProjectionInput input;
    input.runState = &state;
    input.recoveryDisposition = RecoveryDisposition::WaitingForTrustedTime;
    const auto snapshot = FermentationUiProjector::project(input);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(FermentationHomeMode::Recovery),
                          static_cast<int>(snapshot.home.mode));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RecoveryViewMode::WaitingForTrustedTime),
        static_cast<int>(snapshot.recovery.mode));
}

void test_projector_maps_canonical_messages_temperatures_and_recovery_modes() {
    RunCommandState state;
    RuntimeMessage message;
    message.id = 42U;
    message.active = true;
    state.messages[0] = message;
    state.messageCount = 1U;
    FermentationUiProjectionInput input;
    input.runState = &state;
    input.temperatures.push_back(
        {FermentationTemperatureRole::Product, 18.0, quality(18.0)});

    auto snapshot = FermentationUiProjector::project(input);
    TEST_ASSERT_EQUAL_UINT32(1U, snapshot.messages.size());
    TEST_ASSERT_EQUAL_UINT32(42U, snapshot.messages.front().message.id);
    TEST_ASSERT_TRUE(snapshot.temperatures.front().quality.quality ==
                     device_platform::SensorQuality::Valid);

    state.processState.state = ProcessState::Cooling;
    snapshot = FermentationUiProjector::project(input);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RecoveryViewMode::Cooling),
                          static_cast<int>(snapshot.recovery.mode));
    state.processState.state = ProcessState::Completed;
    snapshot = FermentationUiProjector::project(input);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RecoveryViewMode::Completed),
                          static_cast<int>(snapshot.recovery.mode));
}

void test_refresh_revision_changes_only_on_new_publication() {
    RunCommandState state;
    FermentationUiRefreshRevisionTracker tracker;
    FermentationUiProjectionInput input;
    input.runState = &state;
    input.refreshTracker = &tracker;
    const auto first = FermentationUiProjector::project(input);
    const auto readAgain = FermentationUiProjector::project(input);
    TEST_ASSERT_EQUAL_UINT64(first.refreshRevision->value,
                             readAgain.refreshRevision->value);
    input.revisions.expectedStateSequence = 1U;
    const auto changed = FermentationUiProjector::project(input);
    TEST_ASSERT_TRUE(changed.refreshRevision->value > first.refreshRevision->value);
}

}  // namespace

void setUp() {}
void tearDown() {}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_projector_builds_shared_snapshot_without_surface_state);
    RUN_TEST(test_projector_marks_fallback_only_from_canonical_pending_state);
    RUN_TEST(test_projector_marks_recovery_home_from_canonical_disposition);
    RUN_TEST(test_projector_maps_canonical_messages_temperatures_and_recovery_modes);
    RUN_TEST(test_refresh_revision_changes_only_on_new_publication);
    return UNITY_END();
}
