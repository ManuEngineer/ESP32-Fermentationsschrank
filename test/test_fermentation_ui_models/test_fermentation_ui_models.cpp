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
    input.messages.push_back({message});
    input.semanticActions.push_back(
        {device_platform::TextNamespace{"fermentation"}, "start"});
    input.primaryAction =
        device_platform::TextKey{device_platform::TextNamespace{"fermentation"},
                                 "start"};
    input.semanticPublicationRevision = 11U;

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
}

void test_refresh_revision_changes_only_on_new_publication() {
    RunCommandState state;
    device_platform::UiRefreshRevisionTracker tracker;
    FermentationUiProjectionInput input;
    input.runState = &state;
    input.refreshTracker = &tracker;
    input.semanticPublicationRevision = 1U;
    const auto first = FermentationUiProjector::project(input);
    const auto readAgain = FermentationUiProjector::project(input);
    TEST_ASSERT_EQUAL_UINT64(first.refreshRevision->value,
                             readAgain.refreshRevision->value);
    input.semanticPublicationRevision = 2U;
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
    RUN_TEST(test_refresh_revision_changes_only_on_new_publication);
    return UNITY_END();
}
