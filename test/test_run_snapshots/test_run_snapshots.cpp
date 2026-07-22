#include <unity.h>

#include <cstddef>
#include <limits>

#include "run_snapshot.hpp"
#include "standard_program_catalog.hpp"

namespace {

using fermentation::ActiveRun;
using fermentation::FactoryProgramCatalog;
using fermentation::ProgramDocument;
using fermentation::ProgramSourceKind;
using fermentation::RunAdjustmentContext;
using fermentation::RunAdjustmentRequest;
using fermentation::RunAdjustmentStatus;
using fermentation::RunChangeReason;
using fermentation::RunChangeSource;

ProgramDocument makeCommissionedFactoryProgram(const char* id) {
    auto document = FactoryProgramCatalog::find(id);
    TEST_ASSERT_TRUE(document.has_value());
    auto& program = document->program;
    program.productSensorFailure.fallbackDelaySeconds = 30U;
    program.fermentationStages.front().targetTemperatureCelsius = 38.0;
    program.fermentationStages.front().durationMinutes = 120U;
    program.targetQualification.bandCelsius = 0.5;
    program.targetQualification.durationMinutes = 10U;
    program.maximumTargetReachMinutes = 180U;
    if (program.completion.mode !=
        fermentation::CompletionMode::FinishWithoutCooling) {
        program.completion.coolingTargetCelsius = 8.0;
    }
    TEST_ASSERT_TRUE(validateProgram(*document).valid());
    return *document;
}

ProgramDocument makeCommissionedUserProgram() {
    auto document = FactoryProgramCatalog::makeUserCopy(
        "yogurt-mild", "user-yogurt", "Benutzerjoghurt");
    TEST_ASSERT_TRUE(document.has_value());
    auto& program = document->program;
    program.productSensorFailure.fallbackDelaySeconds = 30U;
    program.fermentationStages.front().targetTemperatureCelsius = 39.0;
    program.fermentationStages.front().durationMinutes = 180U;
    program.targetQualification.bandCelsius = 0.5;
    program.targetQualification.durationMinutes = 10U;
    program.maximumTargetReachMinutes = 180U;
    program.completion.coolingTargetCelsius = 8.0;
    TEST_ASSERT_TRUE(validateProgram(*document).valid());
    return *document;
}

RunAdjustmentContext adjustableContext() { return {true, true, 0U, 0U}; }

RunAdjustmentRequest targetAdjustment(double target,
                                      std::uint64_t monotonicMillis) {
    RunAdjustmentRequest request;
    request.targetTemperatureCelsius = target;
    request.confirmed = true;
    request.source = RunChangeSource::WebInterface;
    request.reason = RunChangeReason::UserAdjustment;
    request.timestamp.monotonicMillis = monotonicMillis;
    request.timestamp.unixTimeSeconds = 1784736000;
    return request;
}

void test_start_accepts_standard_and_user_programs() {
    const auto standard = makeCommissionedFactoryProgram("yogurt-mild");
    const auto user = makeCommissionedUserProgram();

    const auto standardRun =
        ActiveRun::start(standard, ProgramSourceKind::FactoryCatalog, 7U);
    const auto userRun =
        ActiveRun::start(user, ProgramSourceKind::UserProgram, 3U);

    TEST_ASSERT_TRUE(standardRun.has_value());
    TEST_ASSERT_TRUE(userRun.has_value());
    TEST_ASSERT_EQUAL_UINT32(7U, standardRun->snapshot().sourceProgramRevision);
    TEST_ASSERT_EQUAL_STRING(
        "Joghurt mild",
        standardRun->snapshot().sourceProgram.program.name.c_str());
    TEST_ASSERT_EQUAL_DOUBLE(
        39.0, userRun->effectiveValues().targetTemperatureCelsius);
    TEST_ASSERT_EQUAL_UINT32(
        180U, userRun->effectiveValues().remainingDurationMinutes);
}

void test_start_rejects_uncommissioned_or_mismatched_sources() {
    const auto uncommissioned = FactoryProgramCatalog::programs().front();
    const auto user = makeCommissionedUserProgram();

    TEST_ASSERT_FALSE(
        ActiveRun::start(uncommissioned, ProgramSourceKind::FactoryCatalog, 1U)
            .has_value());
    TEST_ASSERT_FALSE(
        ActiveRun::start(user, ProgramSourceKind::FactoryCatalog, 1U)
            .has_value());
    TEST_ASSERT_FALSE(
        ActiveRun::start(user, ProgramSourceKind::UserProgram, 0U).has_value());
}

void test_source_program_changes_do_not_change_run_snapshot() {
    auto source = makeCommissionedUserProgram();
    auto run = ActiveRun::start(source, ProgramSourceKind::UserProgram, 4U);
    TEST_ASSERT_TRUE(run.has_value());

    source.program.name = "Spaeter geaendert";
    source.program.fermentationStages.front().targetTemperatureCelsius = 25.0;
    source.program.fermentationStages.front().durationMinutes = 10U;

    TEST_ASSERT_EQUAL_STRING(
        "Benutzerjoghurt", run->snapshot().sourceProgram.program.name.c_str());
    TEST_ASSERT_EQUAL_DOUBLE(
        39.0, *run->snapshot()
                   .sourceProgram.program.fermentationStages.front()
                   .targetTemperatureCelsius);
    TEST_ASSERT_EQUAL_UINT32(
        180U, *run->snapshot()
                   .sourceProgram.program.fermentationStages.front()
                   .durationMinutes);
}

void test_target_adjustment_records_complete_revision() {
    const auto source = makeCommissionedUserProgram();
    auto run = ActiveRun::start(source, ProgramSourceKind::UserProgram, 4U);
    TEST_ASSERT_TRUE(run.has_value());

    const auto result = run->applyAdjustment(targetAdjustment(40.0, 1000U),
                                             adjustableContext());

    TEST_ASSERT_TRUE(result.applied());
    TEST_ASSERT_TRUE(result.requiresTargetRequalification);
    TEST_ASSERT_EQUAL_DOUBLE(40.0,
                             run->effectiveValues().targetTemperatureCelsius);
    TEST_ASSERT_EQUAL_UINT32(1U, run->revisionCount());
    const auto* revision = run->revisionAt(0U);
    TEST_ASSERT_NOT_NULL(revision);
    TEST_ASSERT_EQUAL_UINT32(1U, revision->sequence);
    TEST_ASSERT_EQUAL_UINT32(0U, revision->monotonicEpoch);
    TEST_ASSERT_EQUAL_DOUBLE(39.0, revision->before.targetTemperatureCelsius);
    TEST_ASSERT_EQUAL_DOUBLE(40.0, revision->after.targetTemperatureCelsius);
    TEST_ASSERT_TRUE(revision->targetTemperatureChanged);
    TEST_ASSERT_FALSE(revision->remainingDurationChanged);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RunChangeSource::WebInterface),
                          static_cast<int>(revision->source));
    TEST_ASSERT_EQUAL_UINT64(1000U, revision->timestamp.monotonicMillis);
    TEST_ASSERT_TRUE(revision->timestamp.unixTimeSeconds.has_value());
    TEST_ASSERT_EQUAL_INT64(1784736000, *revision->timestamp.unixTimeSeconds);
    TEST_ASSERT_EQUAL_DOUBLE(
        39.0, *run->snapshot()
                   .sourceProgram.program.fermentationStages.front()
                   .targetTemperatureCelsius);
}

void test_duration_and_combined_adjustments_are_atomic() {
    const auto source = makeCommissionedUserProgram();
    auto run = ActiveRun::start(source, ProgramSourceKind::UserProgram, 4U);
    TEST_ASSERT_TRUE(run.has_value());

    RunAdjustmentRequest durationRequest;
    durationRequest.remainingDurationMinutes = 0U;
    durationRequest.confirmed = true;
    durationRequest.timestamp.monotonicMillis = 100U;
    const auto durationResult =
        run->applyAdjustment(durationRequest, adjustableContext());
    TEST_ASSERT_TRUE(durationResult.applied());
    TEST_ASSERT_FALSE(durationResult.requiresTargetRequalification);
    TEST_ASSERT_EQUAL_UINT32(0U,
                             run->effectiveValues().remainingDurationMinutes);

    const auto valuesBeforeRejection = run->effectiveValues();
    const auto revisionsBeforeRejection = run->revisionCount();
    RunAdjustmentRequest invalidCombined;
    invalidCombined.targetTemperatureCelsius = 80.0;
    invalidCombined.remainingDurationMinutes = 60U;
    invalidCombined.confirmed = true;
    invalidCombined.timestamp.monotonicMillis = 200U;
    const auto rejected =
        run->applyAdjustment(invalidCombined, adjustableContext());

    TEST_ASSERT_EQUAL_INT(static_cast<int>(RunAdjustmentStatus::InvalidValue),
                          static_cast<int>(rejected.status));
    TEST_ASSERT_EQUAL_DOUBLE(valuesBeforeRejection.targetTemperatureCelsius,
                             run->effectiveValues().targetTemperatureCelsius);
    TEST_ASSERT_EQUAL_UINT32(valuesBeforeRejection.remainingDurationMinutes,
                             run->effectiveValues().remainingDurationMinutes);
    TEST_ASSERT_EQUAL_UINT32(revisionsBeforeRejection, run->revisionCount());
}

void test_unconfirmed_unsafe_and_completed_stage_changes_are_rejected() {
    const auto source = makeCommissionedUserProgram();
    auto run = ActiveRun::start(source, ProgramSourceKind::UserProgram, 4U);
    TEST_ASSERT_TRUE(run.has_value());
    const auto initialValues = run->effectiveValues();

    auto request = targetAdjustment(40.0, 100U);
    request.confirmed = false;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunAdjustmentStatus::NotConfirmed),
        static_cast<int>(
            run->applyAdjustment(request, adjustableContext()).status));

    request.confirmed = true;
    auto unsafe = adjustableContext();
    unsafe.safetyAllowsChange = false;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunAdjustmentStatus::SafetyRejected),
        static_cast<int>(run->applyAdjustment(request, unsafe).status));

    auto completed = adjustableContext();
    completed.completedStageCount = 1U;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunAdjustmentStatus::CompletedStage),
        static_cast<int>(run->applyAdjustment(request, completed).status));

    TEST_ASSERT_EQUAL_UINT32(0U, run->revisionCount());
    TEST_ASSERT_EQUAL_DOUBLE(initialValues.targetTemperatureCelsius,
                             run->effectiveValues().targetTemperatureCelsius);
}

void test_restore_replays_snapshot_and_revision_history() {
    const auto source = makeCommissionedUserProgram();
    auto run = ActiveRun::start(source, ProgramSourceKind::UserProgram, 9U);
    TEST_ASSERT_TRUE(run.has_value());
    TEST_ASSERT_TRUE(
        run->applyAdjustment(targetAdjustment(40.0, 100U), adjustableContext())
            .applied());

    RunAdjustmentRequest durationRequest;
    durationRequest.remainingDurationMinutes = 90U;
    durationRequest.confirmed = true;
    durationRequest.source = RunChangeSource::LocalDisplay;
    durationRequest.reason = RunChangeReason::UserAdjustment;
    durationRequest.timestamp.monotonicMillis = 200U;
    TEST_ASSERT_TRUE(
        run->applyAdjustment(durationRequest, adjustableContext()).applied());

    const auto persistedSnapshot = run->snapshot();
    const auto persistedRevisions = run->revisions();
    const auto persistedRevisionCount = run->revisionCount();
    const auto restored = ActiveRun::restore(
        persistedSnapshot, persistedRevisions, persistedRevisionCount);

    TEST_ASSERT_TRUE(restored.has_value());
    TEST_ASSERT_EQUAL_DOUBLE(
        40.0, restored->effectiveValues().targetTemperatureCelsius);
    TEST_ASSERT_EQUAL_UINT32(
        90U, restored->effectiveValues().remainingDurationMinutes);
    TEST_ASSERT_EQUAL_UINT32(2U, restored->revisionCount());
    TEST_ASSERT_EQUAL_STRING(
        "Benutzerjoghurt",
        restored->snapshot().sourceProgram.program.name.c_str());
}

void test_restore_rejects_corrupt_or_reordered_revision_history() {
    const auto source = makeCommissionedUserProgram();
    auto run = ActiveRun::start(source, ProgramSourceKind::UserProgram, 9U);
    TEST_ASSERT_TRUE(run.has_value());
    TEST_ASSERT_TRUE(
        run->applyAdjustment(targetAdjustment(40.0, 100U), adjustableContext())
            .applied());

    auto corrupt = run->revisions();
    corrupt[0].sequence = 2U;
    TEST_ASSERT_FALSE(
        ActiveRun::restore(run->snapshot(), corrupt, 1U).has_value());

    corrupt = run->revisions();
    corrupt[0].before.targetTemperatureCelsius = 20.0;
    TEST_ASSERT_FALSE(
        ActiveRun::restore(run->snapshot(), corrupt, 1U).has_value());

    corrupt = run->revisions();
    corrupt[1] = corrupt[0];
    corrupt[1].sequence = 2U;
    corrupt[1].before = corrupt[0].after;
    corrupt[1].after.targetTemperatureCelsius = 41.0;
    corrupt[1].timestamp.monotonicMillis = 99U;
    TEST_ASSERT_FALSE(
        ActiveRun::restore(run->snapshot(), corrupt, 2U).has_value());
}

void test_timestamp_and_revision_capacity_are_enforced() {
    const auto source = makeCommissionedUserProgram();
    auto run = ActiveRun::start(source, ProgramSourceKind::UserProgram, 9U);
    TEST_ASSERT_TRUE(run.has_value());

    for (std::size_t index = 0U; index < fermentation::kMaximumRunRevisions;
         ++index) {
        RunAdjustmentRequest request;
        request.remainingDurationMinutes =
            static_cast<std::uint32_t>(181U + index);
        request.confirmed = true;
        request.timestamp.monotonicMillis = index + 100U;
        TEST_ASSERT_TRUE(
            run->applyAdjustment(request, adjustableContext()).applied());
    }

    auto request = targetAdjustment(40.0, 1000U);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunAdjustmentStatus::RevisionCapacityReached),
        static_cast<int>(
            run->applyAdjustment(request, adjustableContext()).status));

    auto shortRun =
        ActiveRun::start(source, ProgramSourceKind::UserProgram, 9U);
    TEST_ASSERT_TRUE(shortRun.has_value());
    TEST_ASSERT_TRUE(
        shortRun
            ->applyAdjustment(targetAdjustment(40.0, 200U), adjustableContext())
            .applied());
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunAdjustmentStatus::TimestampWentBackwards),
        static_cast<int>(shortRun
                             ->applyAdjustment(targetAdjustment(41.0, 199U),
                                               adjustableContext())
                             .status));
    TEST_ASSERT_EQUAL_UINT32(1U, shortRun->revisionCount());
}

void test_unix_timestamp_going_backwards_is_rejected() {
    const auto source = makeCommissionedUserProgram();
    auto run = ActiveRun::start(source, ProgramSourceKind::UserProgram, 9U);
    TEST_ASSERT_TRUE(run.has_value());

    auto first = targetAdjustment(40.0, 200U);
    first.timestamp.unixTimeSeconds = 1000;
    TEST_ASSERT_TRUE(
        run->applyAdjustment(first, adjustableContext()).applied());

    auto decreasedUtc = targetAdjustment(41.0, 300U);
    decreasedUtc.timestamp.unixTimeSeconds = 999;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunAdjustmentStatus::TimestampWentBackwards),
        static_cast<int>(
            run->applyAdjustment(decreasedUtc, adjustableContext()).status));
    TEST_ASSERT_EQUAL_UINT32(1U, run->revisionCount());

    auto noUtc = targetAdjustment(41.0, 300U);
    noUtc.timestamp.unixTimeSeconds = std::nullopt;
    TEST_ASSERT_TRUE(
        run->applyAdjustment(noUtc, adjustableContext()).applied());

    auto rollbackAfterMissingUtc = targetAdjustment(42.0, 400U);
    rollbackAfterMissingUtc.timestamp.unixTimeSeconds = 999;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunAdjustmentStatus::TimestampWentBackwards),
        static_cast<int>(
            run->applyAdjustment(rollbackAfterMissingUtc, adjustableContext())
                .status));
    TEST_ASSERT_EQUAL_UINT32(2U, run->revisionCount());
}

void test_restore_rejects_decreasing_unix_timestamp() {
    const auto source = makeCommissionedUserProgram();
    auto run = ActiveRun::start(source, ProgramSourceKind::UserProgram, 9U);
    TEST_ASSERT_TRUE(run.has_value());

    auto first = targetAdjustment(40.0, 100U);
    first.timestamp.unixTimeSeconds = 2000;
    TEST_ASSERT_TRUE(
        run->applyAdjustment(first, adjustableContext()).applied());

    RunAdjustmentRequest second;
    second.remainingDurationMinutes = 90U;
    second.confirmed = true;
    second.source = RunChangeSource::LocalDisplay;
    second.reason = RunChangeReason::UserAdjustment;
    second.timestamp.monotonicMillis = 200U;
    second.timestamp.unixTimeSeconds = std::nullopt;
    TEST_ASSERT_TRUE(
        run->applyAdjustment(second, adjustableContext()).applied());

    auto third = targetAdjustment(41.0, 300U);
    third.timestamp.unixTimeSeconds = 2001;
    TEST_ASSERT_TRUE(
        run->applyAdjustment(third, adjustableContext()).applied());

    auto revisions = run->revisions();
    revisions[2].timestamp.unixTimeSeconds = 1999;
    TEST_ASSERT_FALSE(
        ActiveRun::restore(run->snapshot(), revisions, 3U).has_value());
}

void test_restore_rejects_invalid_monotonic_epochs() {
    const auto source = makeCommissionedUserProgram();
    auto run = ActiveRun::start(source, ProgramSourceKind::UserProgram, 9U);
    TEST_ASSERT_TRUE(run.has_value());
    TEST_ASSERT_TRUE(
        run->applyAdjustment(targetAdjustment(40.0, 100U), adjustableContext())
            .applied());

    RunAdjustmentRequest durationRequest;
    durationRequest.remainingDurationMinutes = 90U;
    durationRequest.confirmed = true;
    durationRequest.timestamp.monotonicMillis = 200U;
    TEST_ASSERT_TRUE(
        run->applyAdjustment(durationRequest, adjustableContext()).applied());

    auto revisions = run->revisions();
    revisions[0].monotonicEpoch = 1U;
    TEST_ASSERT_FALSE(
        ActiveRun::restore(run->snapshot(), revisions, 2U).has_value());

    revisions = run->revisions();
    revisions[1].monotonicEpoch = 2U;
    TEST_ASSERT_FALSE(
        ActiveRun::restore(run->snapshot(), revisions, 2U).has_value());

    revisions = run->revisions();
    revisions[0].monotonicEpoch =
        std::numeric_limits<std::uint32_t>::max() - 1U;
    TEST_ASSERT_FALSE(
        ActiveRun::restore(run->snapshot(), revisions, 2U).has_value());
}

void test_adjustment_after_restart_uses_new_monotonic_epoch() {
    const auto source = makeCommissionedUserProgram();
    auto run = ActiveRun::start(source, ProgramSourceKind::UserProgram, 9U);
    TEST_ASSERT_TRUE(run.has_value());
    TEST_ASSERT_TRUE(run->applyAdjustment(targetAdjustment(40.0, 500000U),
                                          adjustableContext())
                         .applied());

    auto restored = ActiveRun::restore(run->snapshot(), run->revisions(),
                                       run->revisionCount());
    TEST_ASSERT_TRUE(restored.has_value());

    RunAdjustmentRequest afterRestart;
    afterRestart.remainingDurationMinutes = 90U;
    afterRestart.confirmed = true;
    afterRestart.timestamp.monotonicMillis = 5U;
    afterRestart.timestamp.unixTimeSeconds = 1784736001;
    TEST_ASSERT_TRUE(
        restored->applyAdjustment(afterRestart, adjustableContext()).applied());
    const auto* restartRevision = restored->revisionAt(1U);
    TEST_ASSERT_NOT_NULL(restartRevision);
    TEST_ASSERT_EQUAL_UINT32(1U, restartRevision->monotonicEpoch);
    TEST_ASSERT_EQUAL_UINT64(5U, restartRevision->timestamp.monotonicMillis);

    auto backwardsInSameBoot = targetAdjustment(41.0, 4U);
    backwardsInSameBoot.timestamp.unixTimeSeconds = 1784736002;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunAdjustmentStatus::TimestampWentBackwards),
        static_cast<int>(
            restored->applyAdjustment(backwardsInSameBoot, adjustableContext())
                .status));

    const auto restoredAgain = ActiveRun::restore(
        restored->snapshot(), restored->revisions(), restored->revisionCount());
    TEST_ASSERT_TRUE(restoredAgain.has_value());
    TEST_ASSERT_EQUAL_UINT32(
        90U, restoredAgain->effectiveValues().remainingDurationMinutes);
}

void test_noop_and_invalid_metadata_do_not_create_revisions() {
    const auto source = makeCommissionedUserProgram();
    auto run = ActiveRun::start(source, ProgramSourceKind::UserProgram, 9U);
    TEST_ASSERT_TRUE(run.has_value());

    RunAdjustmentRequest noChange;
    noChange.targetTemperatureCelsius = 39.0;
    noChange.confirmed = true;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunAdjustmentStatus::NoChange),
        static_cast<int>(
            run->applyAdjustment(noChange, adjustableContext()).status));

    auto invalidMetadata = targetAdjustment(40.0, 100U);
    invalidMetadata.source = static_cast<RunChangeSource>(255U);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunAdjustmentStatus::InvalidMetadata),
        static_cast<int>(
            run->applyAdjustment(invalidMetadata, adjustableContext()).status));
    TEST_ASSERT_EQUAL_UINT32(0U, run->revisionCount());
}

}  // namespace

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_start_accepts_standard_and_user_programs);
    RUN_TEST(test_start_rejects_uncommissioned_or_mismatched_sources);
    RUN_TEST(test_source_program_changes_do_not_change_run_snapshot);
    RUN_TEST(test_target_adjustment_records_complete_revision);
    RUN_TEST(test_duration_and_combined_adjustments_are_atomic);
    RUN_TEST(test_unconfirmed_unsafe_and_completed_stage_changes_are_rejected);
    RUN_TEST(test_restore_replays_snapshot_and_revision_history);
    RUN_TEST(test_restore_rejects_corrupt_or_reordered_revision_history);
    RUN_TEST(test_timestamp_and_revision_capacity_are_enforced);
    RUN_TEST(test_unix_timestamp_going_backwards_is_rejected);
    RUN_TEST(test_restore_rejects_decreasing_unix_timestamp);
    RUN_TEST(test_restore_rejects_invalid_monotonic_epochs);
    RUN_TEST(test_adjustment_after_restart_uses_new_monotonic_epoch);
    RUN_TEST(test_noop_and_invalid_metadata_do_not_create_revisions);
    return UNITY_END();
}
