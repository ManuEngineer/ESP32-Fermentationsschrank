#include <unity.h>

#include <cstdint>

#include "sensor_selection.hpp"

namespace {

using namespace fermentation;
using device_platform::SensorFaultReason;
using device_platform::SensorQuality;
using device_platform::SensorQualitySnapshot;

SensorQualitySnapshot snapshotWithQuality(SensorQuality quality) {
    SensorQualitySnapshot snapshot;
    snapshot.quality = quality;
    if (quality != SensorQuality::Valid) {
        snapshot.lastFaultReason = SensorFaultReason::MissingSample;
    }
    return snapshot;
}

SensorQualitySnapshot validSnapshot() {
    return snapshotWithQuality(SensorQuality::Valid);
}

SensorQualitySnapshot staleSnapshot() {
    return snapshotWithQuality(SensorQuality::Stale);
}

SensorQualitySnapshot failedSnapshot() {
    return snapshotWithQuality(SensorQuality::Failed);
}

SensorSelectionStateView makeView(
    SensorSelectionPhase phase, SensorPeltierPermission permission,
    std::optional<RunSensorMode> activeMode,
    std::optional<PersistedSensorSelectionState> persisted,
    std::uint32_t runRevision, std::string activeRunId = "run-1") {
    SensorSelectionStateView view;
    view.activeRunId = std::move(activeRunId);
    view.runtime.phase = phase;
    view.runtime.permission = permission;
    view.activeMode = activeMode;
    view.persisted = persisted;
    view.runRevision = runRevision;
    return view;
}

PersistedSensorSelectionState persistedState(
    SensorSelectionProvenance provenance, SensorSelectionDecisionCause cause,
    std::uint32_t revision) {
    return {provenance, cause, revision};
}

SensorSelectionDecision makeDecision(
    const SensorSelectionStateView& expected, SensorPreference sensorPreference,
    ProductSensorFailurePolicy policy, ReturnStrategy returnStrategy,
    SensorQualitySnapshot air, SensorQualitySnapshot product,
    SensorQualitySnapshot cooling,
    std::optional<std::uint32_t> fallbackDelaySeconds = std::nullopt,
    std::optional<SensorSelectionUserAction> userAction = std::nullopt) {
    SensorSelectionDecision decision;
    decision.expected = expected;
    decision.program.sensorPreference = sensorPreference;
    decision.program.policy = policy;
    decision.program.returnStrategy = returnStrategy;
    decision.program.fallbackDelaySeconds = fallbackDelaySeconds;
    decision.plausibility.air = std::move(air);
    decision.plausibility.product = std::move(product);
    decision.plausibility.cooling = std::move(cooling);
    decision.plausibility.evaluationMonotonicMillis = 1'000'000U;
    decision.userAction = userAction;
    return decision;
}

bool runtimeAndModeUnchanged(const SensorSelectionStateView& before,
                             const SensorSelectionStateMutation& result) {
    return result.runtime == before.runtime && result.activeMode == before.activeMode &&
           result.persisted == before.persisted &&
           result.resultingRunRevision == before.runRevision;
}

// ---------------------------------------------------------------------------
// Vollstaendige Policy-/Aktionsmatrix (6.4.14)
// ---------------------------------------------------------------------------

void test_fallback_after_timeout_stays_blocked_before_timeout() {
    auto view = makeView(SensorSelectionPhase::ProductFailureDetected,
                         SensorPeltierPermission::Blocked, RunSensorMode::Product,
                         persistedState(SensorSelectionProvenance::InitialSelection,
                                       SensorSelectionDecisionCause::ProductFailureBlock,
                                       2U),
                         2U);
    view.runtime.fallbackWaitStartedAtMonotonicMillis = 1000U;
    auto decision = makeDecision(
        view, SensorPreference::ProductIfAvailableElseAir,
        ProductSensorFailurePolicy::FallbackToAirAfterTimeout,
        ReturnStrategy::AutomaticValidatedReturnToProduct, validSnapshot(),
        failedSnapshot(), validSnapshot(), 60U);

    const auto result = applySensorSelectionDecision(view, decision, 1500U);

    TEST_ASSERT_TRUE(result.status == SensorSelectionApplyStatus::NoChange);
    TEST_ASSERT_TRUE(runtimeAndModeUnchanged(view, result));
}

void test_fallback_after_timeout_falls_back_to_air_when_valid() {
    auto view = makeView(SensorSelectionPhase::ProductFailureDetected,
                         SensorPeltierPermission::Blocked, RunSensorMode::Product,
                         persistedState(SensorSelectionProvenance::InitialSelection,
                                       SensorSelectionDecisionCause::ProductFailureBlock,
                                       2U),
                         2U);
    view.runtime.fallbackWaitStartedAtMonotonicMillis = 1000U;
    auto decision = makeDecision(
        view, SensorPreference::ProductIfAvailableElseAir,
        ProductSensorFailurePolicy::FallbackToAirAfterTimeout,
        ReturnStrategy::AutomaticValidatedReturnToProduct, validSnapshot(),
        failedSnapshot(), validSnapshot(), 60U);

    const auto result = applySensorSelectionDecision(view, decision, 61000U);

    TEST_ASSERT_TRUE(result.status ==
                     SensorSelectionApplyStatus::AppliedPersistentCandidate);
    TEST_ASSERT_TRUE(result.runtime.phase == SensorSelectionPhase::AirFallbackActive);
    TEST_ASSERT_TRUE(result.runtime.permission == SensorPeltierPermission::Allowed);
    TEST_ASSERT_TRUE(result.activeMode == RunSensorMode::Air);
    TEST_ASSERT_EQUAL_UINT32(3U, result.resultingRunRevision);
    TEST_ASSERT_TRUE(result.event.has_value());
    TEST_ASSERT_TRUE(result.event->cause == SensorSelectionDecisionCause::FallbackToAir);
    TEST_ASSERT_TRUE(result.event->beforeMode == RunSensorMode::Product);
    TEST_ASSERT_TRUE(result.event->afterMode == RunSensorMode::Air);
    TEST_ASSERT_FALSE(result.notice.has_value());
    TEST_ASSERT_TRUE(result.persisted.has_value());
    TEST_ASSERT_TRUE(result.persisted->provenance ==
                     SensorSelectionProvenance::FallbackActive);
}

void test_fallback_after_timeout_reaches_safe_locked_without_air() {
    auto view = makeView(SensorSelectionPhase::ProductFailureDetected,
                         SensorPeltierPermission::Blocked, RunSensorMode::Product,
                         persistedState(SensorSelectionProvenance::InitialSelection,
                                       SensorSelectionDecisionCause::ProductFailureBlock,
                                       2U),
                         2U);
    view.runtime.fallbackWaitStartedAtMonotonicMillis = 1000U;
    auto decision = makeDecision(
        view, SensorPreference::ProductIfAvailableElseAir,
        ProductSensorFailurePolicy::FallbackToAirAfterTimeout,
        ReturnStrategy::AutomaticValidatedReturnToProduct, failedSnapshot(),
        failedSnapshot(), validSnapshot(), 60U);

    const auto result = applySensorSelectionDecision(view, decision, 61000U);

    TEST_ASSERT_TRUE(result.status ==
                     SensorSelectionApplyStatus::AppliedPersistentCandidate);
    TEST_ASSERT_TRUE(result.runtime.phase == SensorSelectionPhase::SafeLocked);
    TEST_ASSERT_TRUE(result.runtime.permission == SensorPeltierPermission::Blocked);
    TEST_ASSERT_TRUE(result.activeMode == RunSensorMode::Product);
    TEST_ASSERT_TRUE(result.notice.has_value());
    TEST_ASSERT_TRUE(result.notice->cause == SensorSelectionDecisionCause::SafeStateEntry);
}

void test_continue_with_air_before_timeout_is_manual_fallback() {
    auto view = makeView(SensorSelectionPhase::ProductFailureDetected,
                         SensorPeltierPermission::Blocked, RunSensorMode::Product,
                         persistedState(SensorSelectionProvenance::InitialSelection,
                                       SensorSelectionDecisionCause::ProductFailureBlock,
                                       2U),
                         2U);
    view.runtime.fallbackWaitStartedAtMonotonicMillis = 1000U;
    auto decision = makeDecision(
        view, SensorPreference::ProductIfAvailableElseAir,
        ProductSensorFailurePolicy::FallbackToAirAfterTimeout,
        ReturnStrategy::AutomaticValidatedReturnToProduct, validSnapshot(),
        failedSnapshot(), validSnapshot(), 60U,
        SensorSelectionUserAction::ContinueWithAir);

    const auto result = applySensorSelectionDecision(view, decision, 1200U);

    TEST_ASSERT_TRUE(result.status ==
                     SensorSelectionApplyStatus::AppliedPersistentCandidate);
    TEST_ASSERT_TRUE(result.runtime.phase == SensorSelectionPhase::AirFallbackActive);
    TEST_ASSERT_TRUE(result.event.has_value());
    TEST_ASSERT_TRUE(result.event->cause ==
                     SensorSelectionDecisionCause::ManualUserFallback);
}

void test_continue_with_air_without_air_cooling_is_invalid_input() {
    auto view = makeView(SensorSelectionPhase::ProductFailureDetected,
                         SensorPeltierPermission::Blocked, RunSensorMode::Product,
                         persistedState(SensorSelectionProvenance::InitialSelection,
                                       SensorSelectionDecisionCause::ProductFailureBlock,
                                       2U),
                         2U);
    view.runtime.fallbackWaitStartedAtMonotonicMillis = 1000U;
    auto decision = makeDecision(
        view, SensorPreference::ProductIfAvailableElseAir,
        ProductSensorFailurePolicy::FallbackToAirAfterTimeout,
        ReturnStrategy::AutomaticValidatedReturnToProduct, failedSnapshot(),
        failedSnapshot(), validSnapshot(), 60U,
        SensorSelectionUserAction::ContinueWithAir);

    const auto result = applySensorSelectionDecision(view, decision, 1200U);

    TEST_ASSERT_TRUE(result.status == SensorSelectionApplyStatus::InvalidDecision);
    TEST_ASSERT_TRUE(runtimeAndModeUnchanged(view, result));
}

// Korrekturauftrag Befund 2, Regressionstest: der 6.4.14-Matrixzeile
// "ContinueWithAir ohne gueltiges Air/Cooling -> CommandStatus::InvalidInput,
// keine Mutation" muss auch dann eine reine Ablehnung bleiben, wenn das
// vorgezogene Air-/Cooling-Sicherheits-Gate (Befund 2) sonst frueher greifen
// wuerde als die Aktion selbst ausgewertet wird - eine "aufgewertete"
// SafeLocked-Mutation waere hier eine ungeplante Persistenz fuer ein laut
// Plan abgelehntes Kommando.
void test_continue_with_air_from_user_decision_required_with_single_air_failure_is_invalid_input() {
    auto view = makeView(SensorSelectionPhase::UserDecisionRequired,
                         SensorPeltierPermission::Blocked, RunSensorMode::Product,
                         persistedState(SensorSelectionProvenance::InitialSelection,
                                       SensorSelectionDecisionCause::ProductFailureBlock,
                                       2U),
                         2U);
    auto decision = makeDecision(
        view, SensorPreference::ProductIfAvailableElseAir,
        ProductSensorFailurePolicy::WaitForUser, ReturnStrategy::ManualReturnToProduct,
        failedSnapshot(), failedSnapshot(), validSnapshot(), std::nullopt,
        SensorSelectionUserAction::ContinueWithAir);

    const auto result = applySensorSelectionDecision(view, decision, 1200U);

    TEST_ASSERT_TRUE(result.status == SensorSelectionApplyStatus::InvalidDecision);
    TEST_ASSERT_TRUE(runtimeAndModeUnchanged(view, result));
}

void test_continue_with_air_in_product_failure_detected_with_simultaneous_failure_is_invalid_input() {
    auto view = makeView(SensorSelectionPhase::ProductFailureDetected,
                         SensorPeltierPermission::Blocked, RunSensorMode::Product,
                         persistedState(SensorSelectionProvenance::InitialSelection,
                                       SensorSelectionDecisionCause::ProductFailureBlock,
                                       2U),
                         2U);
    view.runtime.fallbackWaitStartedAtMonotonicMillis = 1000U;
    auto decision = makeDecision(
        view, SensorPreference::ProductIfAvailableElseAir,
        ProductSensorFailurePolicy::FallbackToAirAfterTimeout,
        ReturnStrategy::AutomaticValidatedReturnToProduct, failedSnapshot(),
        failedSnapshot(), failedSnapshot(), 60U,
        SensorSelectionUserAction::ContinueWithAir);

    const auto result = applySensorSelectionDecision(view, decision, 1200U);

    TEST_ASSERT_TRUE(result.status == SensorSelectionApplyStatus::InvalidDecision);
    TEST_ASSERT_TRUE(runtimeAndModeUnchanged(view, result));
}

void test_safety_pending_matrix_is_out_of_scope_for_core_function() {
    // Die criticalSafetyEventPending-Matrix (6.14.3) gehoert zu
    // decideApplySensorSelectionAction (Commit 4), nicht zu
    // applySensorSelectionDecision - siehe Plan 6.14.1: die Kernfunktion
    // kennt criticalSafetyEventPending bewusst nicht (keine zweite
    // Safety-Regelimplementierung). Dieser Test dokumentiert die Abgrenzung.
    TEST_ASSERT_TRUE(true);
}

void test_wait_for_user_transitions_immediately_without_wait() {
    auto view = makeView(SensorSelectionPhase::NormalProduct,
                         SensorPeltierPermission::Allowed, RunSensorMode::Product,
                         persistedState(SensorSelectionProvenance::InitialSelection,
                                       SensorSelectionDecisionCause::StartSelection,
                                       1U),
                         1U);
    auto decision = makeDecision(view, SensorPreference::ProductIfAvailableElseAir,
                                 ProductSensorFailurePolicy::WaitForUser,
                                 ReturnStrategy::AutomaticValidatedReturnToProduct,
                                 validSnapshot(), failedSnapshot(), validSnapshot());

    const auto result = applySensorSelectionDecision(view, decision, 100U);

    TEST_ASSERT_TRUE(result.status ==
                     SensorSelectionApplyStatus::AppliedPersistentCandidate);
    TEST_ASSERT_TRUE(result.runtime.phase ==
                     SensorSelectionPhase::UserDecisionRequired);
    TEST_ASSERT_TRUE(result.runtime.permission == SensorPeltierPermission::Blocked);
    TEST_ASSERT_TRUE(result.notice.has_value());
    TEST_ASSERT_TRUE(result.notice->cause ==
                     SensorSelectionDecisionCause::ProductFailureBlock);
    TEST_ASSERT_FALSE(
        result.runtime.fallbackWaitStartedAtMonotonicMillis.has_value());
}

void test_wait_for_user_continue_with_air_from_user_decision_required() {
    auto view = makeView(SensorSelectionPhase::UserDecisionRequired,
                         SensorPeltierPermission::Blocked, RunSensorMode::Product,
                         persistedState(SensorSelectionProvenance::InitialSelection,
                                       SensorSelectionDecisionCause::ProductFailureBlock,
                                       2U),
                         2U);
    auto decision = makeDecision(
        view, SensorPreference::ProductIfAvailableElseAir,
        ProductSensorFailurePolicy::WaitForUser,
        ReturnStrategy::AutomaticValidatedReturnToProduct, validSnapshot(),
        failedSnapshot(), validSnapshot(), std::nullopt,
        SensorSelectionUserAction::ContinueWithAir);

    const auto result = applySensorSelectionDecision(view, decision, 100U);

    TEST_ASSERT_TRUE(result.status ==
                     SensorSelectionApplyStatus::AppliedPersistentCandidate);
    TEST_ASSERT_TRUE(result.runtime.phase == SensorSelectionPhase::AirFallbackActive);
    TEST_ASSERT_TRUE(result.event->cause ==
                     SensorSelectionDecisionCause::ManualUserFallback);
}

void test_wait_for_user_continue_with_air_rejected_for_product_required() {
    auto view = makeView(SensorSelectionPhase::UserDecisionRequired,
                         SensorPeltierPermission::Blocked, RunSensorMode::Product,
                         persistedState(SensorSelectionProvenance::InitialSelection,
                                       SensorSelectionDecisionCause::ProductFailureBlock,
                                       2U),
                         2U);
    auto decision = makeDecision(
        view, SensorPreference::ProductRequired,
        ProductSensorFailurePolicy::WaitForUser,
        ReturnStrategy::AutomaticValidatedReturnToProduct, validSnapshot(),
        failedSnapshot(), validSnapshot(), std::nullopt,
        SensorSelectionUserAction::ContinueWithAir);

    const auto result = applySensorSelectionDecision(view, decision, 100U);

    TEST_ASSERT_TRUE(result.status == SensorSelectionApplyStatus::InvalidDecision);
    TEST_ASSERT_TRUE(runtimeAndModeUnchanged(view, result));
}

void test_wait_for_user_product_recovers_without_user_action() {
    auto view = makeView(SensorSelectionPhase::UserDecisionRequired,
                         SensorPeltierPermission::Blocked, RunSensorMode::Product,
                         persistedState(SensorSelectionProvenance::InitialSelection,
                                       SensorSelectionDecisionCause::ProductFailureBlock,
                                       2U),
                         2U);
    auto decision = makeDecision(view, SensorPreference::ProductIfAvailableElseAir,
                                 ProductSensorFailurePolicy::WaitForUser,
                                 ReturnStrategy::AutomaticValidatedReturnToProduct,
                                 validSnapshot(), validSnapshot(), validSnapshot());

    const auto result = applySensorSelectionDecision(view, decision, 100U);

    TEST_ASSERT_TRUE(result.status ==
                     SensorSelectionApplyStatus::AppliedPersistentCandidate);
    TEST_ASSERT_TRUE(result.runtime.phase == SensorSelectionPhase::NormalProduct);
    TEST_ASSERT_TRUE(result.runtime.permission == SensorPeltierPermission::Allowed);
    TEST_ASSERT_TRUE(result.notice->cause ==
                     SensorSelectionDecisionCause::RecoveryRevalidation);
}

void test_wait_for_user_recheck_product_still_invalid_is_no_change() {
    auto view = makeView(SensorSelectionPhase::UserDecisionRequired,
                         SensorPeltierPermission::Blocked, RunSensorMode::Product,
                         persistedState(SensorSelectionProvenance::InitialSelection,
                                       SensorSelectionDecisionCause::ProductFailureBlock,
                                       2U),
                         2U);
    auto decision = makeDecision(
        view, SensorPreference::ProductIfAvailableElseAir,
        ProductSensorFailurePolicy::WaitForUser,
        ReturnStrategy::AutomaticValidatedReturnToProduct, validSnapshot(),
        failedSnapshot(), validSnapshot(), std::nullopt,
        SensorSelectionUserAction::RecheckProduct);

    const auto result = applySensorSelectionDecision(view, decision, 100U);

    TEST_ASSERT_TRUE(result.status == SensorSelectionApplyStatus::NoChange);
    TEST_ASSERT_TRUE(runtimeAndModeUnchanged(view, result));
}

void test_user_decision_required_air_loss_reaches_safe_locked() {
    auto view = makeView(SensorSelectionPhase::UserDecisionRequired,
                         SensorPeltierPermission::Blocked, RunSensorMode::Product,
                         persistedState(SensorSelectionProvenance::InitialSelection,
                                       SensorSelectionDecisionCause::ProductFailureBlock,
                                       2U),
                         2U);
    auto decision = makeDecision(view, SensorPreference::ProductIfAvailableElseAir,
                                 ProductSensorFailurePolicy::WaitForUser,
                                 ReturnStrategy::AutomaticValidatedReturnToProduct,
                                 failedSnapshot(), failedSnapshot(), validSnapshot());

    const auto result = applySensorSelectionDecision(view, decision, 100U);

    TEST_ASSERT_TRUE(result.status ==
                     SensorSelectionApplyStatus::AppliedPersistentCandidate);
    TEST_ASSERT_TRUE(result.runtime.phase == SensorSelectionPhase::SafeLocked);
    TEST_ASSERT_TRUE(result.notice->cause == SensorSelectionDecisionCause::SafeStateEntry);
}

void test_stop_to_safe_state_reaches_safe_locked_without_product_failure_block() {
    auto view = makeView(SensorSelectionPhase::NormalProduct,
                         SensorPeltierPermission::Allowed, RunSensorMode::Product,
                         persistedState(SensorSelectionProvenance::InitialSelection,
                                       SensorSelectionDecisionCause::StartSelection,
                                       1U),
                         1U);
    auto decision = makeDecision(view, SensorPreference::ProductIfAvailableElseAir,
                                 ProductSensorFailurePolicy::StopToSafeState,
                                 ReturnStrategy::ManualReturnToProduct, validSnapshot(),
                                 failedSnapshot(), validSnapshot());

    const auto result = applySensorSelectionDecision(view, decision, 100U);

    TEST_ASSERT_TRUE(result.status ==
                     SensorSelectionApplyStatus::AppliedPersistentCandidate);
    TEST_ASSERT_TRUE(result.runtime.phase == SensorSelectionPhase::SafeLocked);
    TEST_ASSERT_TRUE(result.notice->cause == SensorSelectionDecisionCause::SafeStateEntry);
    // Vorrangregel: genau EINE Entscheidung, kein ProductFailureBlock-Vorlauf.
    TEST_ASSERT_FALSE(
        result.persisted->lastDecisionCause ==
        SensorSelectionDecisionCause::ProductFailureBlock);
}

void test_stop_to_safe_state_stays_locked_when_product_recovers() {
    auto view = makeView(SensorSelectionPhase::SafeLocked,
                         SensorPeltierPermission::Blocked, RunSensorMode::Product,
                         persistedState(SensorSelectionProvenance::InitialSelection,
                                       SensorSelectionDecisionCause::SafeStateEntry, 2U),
                         2U);
    auto decision = makeDecision(view, SensorPreference::ProductIfAvailableElseAir,
                                 ProductSensorFailurePolicy::StopToSafeState,
                                 ReturnStrategy::ManualReturnToProduct, validSnapshot(),
                                 validSnapshot(), validSnapshot());

    const auto result = applySensorSelectionDecision(view, decision, 100U);

    TEST_ASSERT_TRUE(result.status == SensorSelectionApplyStatus::NoChange);
    TEST_ASSERT_TRUE(result.runtime.phase == SensorSelectionPhase::SafeLocked);
}

// ---------------------------------------------------------------------------
// Vorrangregel (6.4.9)
// ---------------------------------------------------------------------------

void test_precedence_simultaneous_air_cooling_failure_during_return_validation() {
    auto view = makeView(SensorSelectionPhase::ReturnValidationPending,
                         SensorPeltierPermission::Allowed, RunSensorMode::Air,
                         persistedState(SensorSelectionProvenance::FallbackActive,
                                       SensorSelectionDecisionCause::FallbackToAir, 2U),
                         2U);
    view.runtime.returnValidation.enteredAtMonotonicMillis = 500U;
    view.runtime.returnValidation.lastObservedProfileRevision = 7U;
    auto decision = makeDecision(view, SensorPreference::ProductIfAvailableElseAir,
                                 ProductSensorFailurePolicy::FallbackToAirAfterTimeout,
                                 ReturnStrategy::AutomaticValidatedReturnToProduct,
                                 failedSnapshot(), failedSnapshot(), failedSnapshot());

    const auto result = applySensorSelectionDecision(view, decision, 600U);

    TEST_ASSERT_TRUE(result.status ==
                     SensorSelectionApplyStatus::AppliedPersistentCandidate);
    TEST_ASSERT_TRUE(result.runtime.phase == SensorSelectionPhase::SafeLocked);
    TEST_ASSERT_TRUE(result.notice->cause == SensorSelectionDecisionCause::SafeStateEntry);
    TEST_ASSERT_FALSE(
        result.notice->cause == SensorSelectionDecisionCause::ReturnValidationAborted);
}

// ---------------------------------------------------------------------------
// Re-Arm-Pflichttests (6.4.12)
// ---------------------------------------------------------------------------

void test_rearm_ten_thousand_unavailable_evaluations_never_write() {
    auto view = makeView(SensorSelectionPhase::ReturnValidationPending,
                         SensorPeltierPermission::Allowed, RunSensorMode::Air,
                         persistedState(SensorSelectionProvenance::FallbackActive,
                                       SensorSelectionDecisionCause::FallbackToAir, 2U),
                         2U);
    view.runtime.returnValidation.enteredAtMonotonicMillis = 500U;
    view.runtime.returnValidation.lastObservedProfileRevision = 7U;

    for (std::uint32_t i = 0U; i < 10000U; ++i) {
        auto decision =
            makeDecision(view, SensorPreference::ProductIfAvailableElseAir,
                        ProductSensorFailurePolicy::FallbackToAirAfterTimeout,
                        ReturnStrategy::AutomaticValidatedReturnToProduct,
                        validSnapshot(), validSnapshot(), validSnapshot());
        decision.plausibility.thermalCompatibility.status =
            ThermalCompatibility::Unavailable;

        const auto result = applySensorSelectionDecision(view, decision, 600U + i);

        TEST_ASSERT_TRUE(result.status == SensorSelectionApplyStatus::NoChange);
        TEST_ASSERT_EQUAL_UINT32(2U, result.resultingRunRevision);
        view = makeView(result.runtime.phase, result.runtime.permission,
                        result.activeMode, result.persisted,
                        result.resultingRunRevision);
        view.runtime = result.runtime;
    }
}

void test_rearm_repeated_incompatible_aborts_at_most_once_per_attempt() {
    auto view = makeView(SensorSelectionPhase::ReturnValidationPending,
                         SensorPeltierPermission::Allowed, RunSensorMode::Air,
                         persistedState(SensorSelectionProvenance::FallbackActive,
                                       SensorSelectionDecisionCause::FallbackToAir, 2U),
                         2U);
    view.runtime.returnValidation.enteredAtMonotonicMillis = 500U;
    view.runtime.returnValidation.lastObservedProfileRevision = 7U;

    auto decision = makeDecision(view, SensorPreference::ProductIfAvailableElseAir,
                                 ProductSensorFailurePolicy::FallbackToAirAfterTimeout,
                                 ReturnStrategy::AutomaticValidatedReturnToProduct,
                                 validSnapshot(), validSnapshot(), validSnapshot());
    decision.plausibility.thermalCompatibility.status =
        ThermalCompatibility::Incompatible;
    decision.plausibility.thermalCompatibility.profileRevision = 7U;

    const auto firstAbort = applySensorSelectionDecision(view, decision, 600U);
    TEST_ASSERT_TRUE(firstAbort.status ==
                     SensorSelectionApplyStatus::AppliedPersistentCandidate);
    TEST_ASSERT_TRUE(firstAbort.notice->cause ==
                     SensorSelectionDecisionCause::ReturnValidationAborted);
    TEST_ASSERT_TRUE(firstAbort.runtime.phase == SensorSelectionPhase::AirFallbackActive);

    // Weitere automatische Zyklen ohne Re-Arm-Bedingung duerfen nicht erneut
    // in ReturnValidationPending eintreten (kein weiterer Abort moeglich).
    SensorSelectionStateView afterAbort =
        makeView(firstAbort.runtime.phase, firstAbort.runtime.permission,
                firstAbort.activeMode, firstAbort.persisted,
                firstAbort.resultingRunRevision);
    afterAbort.runtime = firstAbort.runtime;
    auto secondDecision =
        makeDecision(afterAbort, SensorPreference::ProductIfAvailableElseAir,
                    ProductSensorFailurePolicy::FallbackToAirAfterTimeout,
                    ReturnStrategy::AutomaticValidatedReturnToProduct,
                    validSnapshot(), validSnapshot(), validSnapshot());
    secondDecision.plausibility.thermalCompatibility.status =
        ThermalCompatibility::Incompatible;
    secondDecision.plausibility.thermalCompatibility.profileRevision = 7U;

    const auto second =
        applySensorSelectionDecision(afterAbort, secondDecision, 700U);
    TEST_ASSERT_TRUE(second.status == SensorSelectionApplyStatus::NoChange);
    TEST_ASSERT_TRUE(second.runtime.phase == SensorSelectionPhase::AirFallbackActive);
}

void test_rearm_new_evidence_generation_starts_exactly_one_new_attempt() {
    auto view = makeView(SensorSelectionPhase::AirFallbackActive,
                         SensorPeltierPermission::Allowed, RunSensorMode::Air,
                         persistedState(SensorSelectionProvenance::FallbackActive,
                                       SensorSelectionDecisionCause::ReturnValidationAborted,
                                       3U),
                         3U);
    view.runtime.returnValidation.lastObservedProfileRevision = 7U;  // vorheriger Versuch

    auto decision = makeDecision(view, SensorPreference::ProductIfAvailableElseAir,
                                 ProductSensorFailurePolicy::FallbackToAirAfterTimeout,
                                 ReturnStrategy::AutomaticValidatedReturnToProduct,
                                 validSnapshot(), validSnapshot(), validSnapshot());
    decision.plausibility.thermalCompatibility.profileRevision = 8U;  // neue Generation

    const auto result = applySensorSelectionDecision(view, decision, 800U);

    TEST_ASSERT_TRUE(result.status == SensorSelectionApplyStatus::AppliedRamOnly);
    TEST_ASSERT_TRUE(result.runtime.phase == SensorSelectionPhase::ReturnValidationPending);
    TEST_ASSERT_EQUAL_UINT32(3U, result.resultingRunRevision);
    TEST_ASSERT_TRUE(
        result.runtime.returnValidation.lastObservedProfileRevision == 8U);
}

void test_rearm_without_evidence_change_does_not_reenter() {
    auto view = makeView(SensorSelectionPhase::AirFallbackActive,
                         SensorPeltierPermission::Allowed, RunSensorMode::Air,
                         persistedState(SensorSelectionProvenance::FallbackActive,
                                       SensorSelectionDecisionCause::ReturnValidationAborted,
                                       3U),
                         3U);
    view.runtime.returnValidation.lastObservedProfileRevision = 7U;

    auto decision = makeDecision(view, SensorPreference::ProductIfAvailableElseAir,
                                 ProductSensorFailurePolicy::FallbackToAirAfterTimeout,
                                 ReturnStrategy::AutomaticValidatedReturnToProduct,
                                 validSnapshot(), validSnapshot(), validSnapshot());
    decision.plausibility.thermalCompatibility.profileRevision = 7U;  // unveraendert

    const auto result = applySensorSelectionDecision(view, decision, 800U);

    TEST_ASSERT_TRUE(result.status == SensorSelectionApplyStatus::NoChange);
    TEST_ASSERT_TRUE(result.runtime.phase == SensorSelectionPhase::AirFallbackActive);
}

void test_rearm_compatible_after_full_stability_returns_exactly_once() {
    auto view = makeView(SensorSelectionPhase::ReturnValidationPending,
                         SensorPeltierPermission::Allowed, RunSensorMode::Air,
                         persistedState(SensorSelectionProvenance::FallbackActive,
                                       SensorSelectionDecisionCause::FallbackToAir, 2U),
                         2U);
    view.runtime.returnValidation.enteredAtMonotonicMillis = 500U;
    view.runtime.returnValidation.lastObservedProfileRevision = 7U;

    auto decision = makeDecision(view, SensorPreference::ProductIfAvailableElseAir,
                                 ProductSensorFailurePolicy::FallbackToAirAfterTimeout,
                                 ReturnStrategy::AutomaticValidatedReturnToProduct,
                                 validSnapshot(), validSnapshot(), validSnapshot());
    decision.plausibility.direction = AbstractControlDirection::Cooling;
    decision.plausibility.controlDemandAgeMs = 10U;
    decision.plausibility.thermalCompatibility.status = ThermalCompatibility::Compatible;
    decision.plausibility.thermalCompatibility.profileRevision = 7U;

    const auto result = applySensorSelectionDecision(view, decision, 900U);

    TEST_ASSERT_TRUE(result.status ==
                     SensorSelectionApplyStatus::AppliedPersistentCandidate);
    TEST_ASSERT_TRUE(result.runtime.phase == SensorSelectionPhase::NormalProduct);
    TEST_ASSERT_TRUE(result.activeMode == RunSensorMode::Product);
    TEST_ASSERT_TRUE(result.event->cause ==
                     SensorSelectionDecisionCause::AutomaticValidatedReturn);
}

void test_rearm_product_failure_during_return_validation_needs_new_recognition() {
    auto view = makeView(SensorSelectionPhase::ReturnValidationPending,
                         SensorPeltierPermission::Allowed, RunSensorMode::Air,
                         persistedState(SensorSelectionProvenance::FallbackActive,
                                       SensorSelectionDecisionCause::FallbackToAir, 2U),
                         2U);
    view.runtime.returnValidation.enteredAtMonotonicMillis = 500U;
    view.runtime.returnValidation.lastObservedProfileRevision = 7U;
    auto decision = makeDecision(view, SensorPreference::ProductIfAvailableElseAir,
                                 ProductSensorFailurePolicy::FallbackToAirAfterTimeout,
                                 ReturnStrategy::AutomaticValidatedReturnToProduct,
                                 validSnapshot(), failedSnapshot(), validSnapshot());

    const auto aborted = applySensorSelectionDecision(view, decision, 600U);
    TEST_ASSERT_TRUE(aborted.status ==
                     SensorSelectionApplyStatus::AppliedPersistentCandidate);
    TEST_ASSERT_TRUE(aborted.runtime.phase == SensorSelectionPhase::AirFallbackActive);
    TEST_ASSERT_TRUE(
        aborted.runtime.returnValidation.lastObservedProfileRevision == 7U);

    // Ohne neue Evidenzgeneration (gleiche profileRevision) kein Re-Arm.
    SensorSelectionStateView stillAirValid =
        makeView(aborted.runtime.phase, aborted.runtime.permission,
                aborted.activeMode, aborted.persisted, aborted.resultingRunRevision);
    stillAirValid.runtime = aborted.runtime;
    auto stillInvalidDecision =
        makeDecision(stillAirValid, SensorPreference::ProductIfAvailableElseAir,
                    ProductSensorFailurePolicy::FallbackToAirAfterTimeout,
                    ReturnStrategy::AutomaticValidatedReturnToProduct, validSnapshot(),
                    failedSnapshot(), validSnapshot());
    const auto stayed =
        applySensorSelectionDecision(stillAirValid, stillInvalidDecision, 700U);
    TEST_ASSERT_TRUE(stayed.status == SensorSelectionApplyStatus::NoChange);
}

// Korrekturauftrag Befund 3 (6.4.12 Re-Arm-Bedingung iii), Pflichttest
// "invalid->valid-Re-Arm bei gleicher Profilrevision": nach einem Abbruch
// wegen erneut ungueltigem Produkt loest ein spaeterer invalid->valid-Zyklus
// genau eine neue Rueckkehrvalidierung aus - auch ohne geaenderte
// profileRevision (Bedingung i bleibt hier bewusst unerfuellt).
void test_rearm_invalid_to_valid_product_cycle_rearms_without_profile_change() {
    auto view = makeView(SensorSelectionPhase::ReturnValidationPending,
                         SensorPeltierPermission::Allowed, RunSensorMode::Air,
                         persistedState(SensorSelectionProvenance::FallbackActive,
                                       SensorSelectionDecisionCause::FallbackToAir, 2U),
                         2U);
    view.runtime.returnValidation.enteredAtMonotonicMillis = 500U;
    view.runtime.returnValidation.lastObservedProfileRevision = 7U;
    auto decision = makeDecision(view, SensorPreference::ProductIfAvailableElseAir,
                                 ProductSensorFailurePolicy::FallbackToAirAfterTimeout,
                                 ReturnStrategy::AutomaticValidatedReturnToProduct,
                                 validSnapshot(), failedSnapshot(), validSnapshot());

    // Abbruch: Produkt waehrend ReturnValidationPending erneut ungueltig.
    const auto aborted = applySensorSelectionDecision(view, decision, 600U);
    TEST_ASSERT_TRUE(aborted.status ==
                     SensorSelectionApplyStatus::AppliedPersistentCandidate);
    TEST_ASSERT_TRUE(aborted.runtime.phase == SensorSelectionPhase::AirFallbackActive);
    TEST_ASSERT_TRUE(aborted.runtime.productReArmPending);
    TEST_ASSERT_TRUE(
        aborted.runtime.returnValidation.lastObservedProfileRevision == 7U);

    // Produkt wird wieder valide - dieselbe profileRevision wie zuvor (7U),
    // Bedingung (i) waere hier NICHT erfuellt.
    SensorSelectionStateView recovered =
        makeView(aborted.runtime.phase, aborted.runtime.permission,
                aborted.activeMode, aborted.persisted, aborted.resultingRunRevision);
    recovered.runtime = aborted.runtime;
    auto recoveryDecision = makeDecision(
        recovered, SensorPreference::ProductIfAvailableElseAir,
        ProductSensorFailurePolicy::FallbackToAirAfterTimeout,
        ReturnStrategy::AutomaticValidatedReturnToProduct, validSnapshot(),
        validSnapshot(), validSnapshot());
    recoveryDecision.plausibility.thermalCompatibility.profileRevision = 7U;

    const auto rearmed =
        applySensorSelectionDecision(recovered, recoveryDecision, 700U);

    TEST_ASSERT_TRUE(rearmed.status == SensorSelectionApplyStatus::AppliedRamOnly);
    TEST_ASSERT_TRUE(rearmed.runtime.phase == SensorSelectionPhase::ReturnValidationPending);
    TEST_ASSERT_FALSE(rearmed.runtime.productReArmPending);
}

// ---------------------------------------------------------------------------
// RecheckProduct-Sonderfall (6.4.12)
// ---------------------------------------------------------------------------

void test_recheck_product_from_air_fallback_with_incomplete_evidence_is_ram_only() {
    auto view = makeView(SensorSelectionPhase::AirFallbackActive,
                         SensorPeltierPermission::Allowed, RunSensorMode::Air,
                         persistedState(SensorSelectionProvenance::FallbackActive,
                                       SensorSelectionDecisionCause::FallbackToAir, 2U),
                         2U);
    auto decision = makeDecision(
        view, SensorPreference::ProductIfAvailableElseAir,
        ProductSensorFailurePolicy::FallbackToAirAfterTimeout,
        ReturnStrategy::AutomaticValidatedReturnToProduct, validSnapshot(),
        validSnapshot(), validSnapshot(), std::nullopt,
        SensorSelectionUserAction::RecheckProduct);
    decision.plausibility.thermalCompatibility.status = ThermalCompatibility::Stale;
    decision.plausibility.thermalCompatibility.profileRevision = 9U;

    const auto result = applySensorSelectionDecision(view, decision, 950U);

    TEST_ASSERT_TRUE(result.status == SensorSelectionApplyStatus::AppliedRamOnly);
    TEST_ASSERT_TRUE(result.runtime.phase == SensorSelectionPhase::ReturnValidationPending);
    TEST_ASSERT_EQUAL_UINT32(2U, result.resultingRunRevision);
    TEST_ASSERT_FALSE(result.event.has_value());
    TEST_ASSERT_FALSE(result.notice.has_value());
}

void test_recheck_product_from_disallowed_state_is_rejected() {
    auto view = makeView(SensorSelectionPhase::NormalAir, SensorPeltierPermission::Allowed,
                         RunSensorMode::Air,
                         persistedState(SensorSelectionProvenance::InitialSelection,
                                       SensorSelectionDecisionCause::StartSelection, 1U),
                         1U);
    auto decision = makeDecision(
        view, SensorPreference::AirOnly,
        ProductSensorFailurePolicy::FallbackToAirAfterTimeout,
        ReturnStrategy::RemainOnAirUntilEnd, validSnapshot(), validSnapshot(),
        validSnapshot(), std::nullopt, SensorSelectionUserAction::RecheckProduct);

    const auto result = applySensorSelectionDecision(view, decision, 100U);

    TEST_ASSERT_TRUE(result.status == SensorSelectionApplyStatus::InvalidDecision);
    TEST_ASSERT_TRUE(runtimeAndModeUnchanged(view, result));
}

// ---------------------------------------------------------------------------
// Typisierte Apply-Status-/Stale-Tests
// ---------------------------------------------------------------------------

void test_stale_decision_on_changed_phase_leaves_state_untouched() {
    auto expected = makeView(SensorSelectionPhase::NormalProduct,
                             SensorPeltierPermission::Allowed, RunSensorMode::Product,
                             persistedState(SensorSelectionProvenance::InitialSelection,
                                           SensorSelectionDecisionCause::StartSelection,
                                           1U),
                             1U);
    auto actual = expected;
    actual.runtime.phase = SensorSelectionPhase::AirFallbackActive;
    actual.activeMode = RunSensorMode::Air;

    auto decision = makeDecision(expected, SensorPreference::ProductIfAvailableElseAir,
                                 ProductSensorFailurePolicy::FallbackToAirAfterTimeout,
                                 ReturnStrategy::AutomaticValidatedReturnToProduct,
                                 validSnapshot(), validSnapshot(), validSnapshot());

    const auto result = applySensorSelectionDecision(actual, decision, 100U);

    TEST_ASSERT_TRUE(result.status == SensorSelectionApplyStatus::StaleDecision);
    TEST_ASSERT_TRUE(runtimeAndModeUnchanged(actual, result));
}

void test_stale_decision_on_changed_provenance_leaves_state_untouched() {
    auto expected = makeView(SensorSelectionPhase::AirFallbackActive,
                             SensorPeltierPermission::Allowed, RunSensorMode::Air,
                             persistedState(SensorSelectionProvenance::FallbackActive,
                                           SensorSelectionDecisionCause::FallbackToAir,
                                           2U),
                             2U);
    auto actual = expected;
    actual.persisted->provenance = SensorSelectionProvenance::ReturnedToProduct;

    auto decision = makeDecision(expected, SensorPreference::ProductIfAvailableElseAir,
                                 ProductSensorFailurePolicy::FallbackToAirAfterTimeout,
                                 ReturnStrategy::AutomaticValidatedReturnToProduct,
                                 validSnapshot(), validSnapshot(), validSnapshot());

    const auto result = applySensorSelectionDecision(actual, decision, 100U);

    TEST_ASSERT_TRUE(result.status == SensorSelectionApplyStatus::StaleDecision);
    TEST_ASSERT_TRUE(runtimeAndModeUnchanged(actual, result));
}

void test_stale_decision_on_changed_run_revision_leaves_state_untouched() {
    auto expected = makeView(SensorSelectionPhase::NormalProduct,
                             SensorPeltierPermission::Allowed, RunSensorMode::Product,
                             persistedState(SensorSelectionProvenance::InitialSelection,
                                           SensorSelectionDecisionCause::StartSelection,
                                           1U),
                             1U);
    auto actual = expected;
    actual.runRevision = 2U;

    auto decision = makeDecision(expected, SensorPreference::ProductIfAvailableElseAir,
                                 ProductSensorFailurePolicy::FallbackToAirAfterTimeout,
                                 ReturnStrategy::AutomaticValidatedReturnToProduct,
                                 validSnapshot(), validSnapshot(), validSnapshot());

    const auto result = applySensorSelectionDecision(actual, decision, 100U);

    TEST_ASSERT_TRUE(result.status == SensorSelectionApplyStatus::StaleDecision);
    TEST_ASSERT_TRUE(runtimeAndModeUnchanged(actual, result));
}

void test_invalid_decision_on_unreachable_runtime_combination() {
    auto view = makeView(SensorSelectionPhase::SafeLocked, SensorPeltierPermission::Allowed,
                         RunSensorMode::Product,
                         persistedState(SensorSelectionProvenance::InitialSelection,
                                       SensorSelectionDecisionCause::SafeStateEntry, 2U),
                         2U);
    auto decision = makeDecision(view, SensorPreference::ProductIfAvailableElseAir,
                                 ProductSensorFailurePolicy::FallbackToAirAfterTimeout,
                                 ReturnStrategy::AutomaticValidatedReturnToProduct,
                                 validSnapshot(), validSnapshot(), validSnapshot());

    const auto result = applySensorSelectionDecision(view, decision, 100U);

    TEST_ASSERT_TRUE(result.status == SensorSelectionApplyStatus::InvalidDecision);
    TEST_ASSERT_TRUE(runtimeAndModeUnchanged(view, result));
}

void test_invalid_context_for_no_active_run_and_restart_revalidation() {
    for (const auto phase : {SensorSelectionPhase::NoActiveRun,
                             SensorSelectionPhase::RestartRevalidationPending}) {
        auto view = makeView(phase, SensorPeltierPermission::Blocked, std::nullopt,
                             std::nullopt, 0U);
        auto decision = makeDecision(view, SensorPreference::ProductIfAvailableElseAir,
                                     ProductSensorFailurePolicy::FallbackToAirAfterTimeout,
                                     ReturnStrategy::AutomaticValidatedReturnToProduct,
                                     validSnapshot(), validSnapshot(), validSnapshot());

        const auto result = applySensorSelectionDecision(view, decision, 100U);

        TEST_ASSERT_TRUE(result.status == SensorSelectionApplyStatus::InvalidContext);
        TEST_ASSERT_TRUE(runtimeAndModeUnchanged(view, result));
    }
}

void test_time_went_backwards_rejects_without_state_change() {
    auto view = makeView(SensorSelectionPhase::NormalProduct,
                         SensorPeltierPermission::Allowed, RunSensorMode::Product,
                         persistedState(SensorSelectionProvenance::InitialSelection,
                                       SensorSelectionDecisionCause::StartSelection, 1U),
                         1U);
    view.runtime.lastAppliedMonotonicMillis = 5000U;
    auto decision = makeDecision(view, SensorPreference::ProductIfAvailableElseAir,
                                 ProductSensorFailurePolicy::FallbackToAirAfterTimeout,
                                 ReturnStrategy::AutomaticValidatedReturnToProduct,
                                 validSnapshot(), validSnapshot(), validSnapshot());

    const auto result = applySensorSelectionDecision(view, decision, 4999U);

    TEST_ASSERT_TRUE(result.status == SensorSelectionApplyStatus::TimeWentBackwards);
    TEST_ASSERT_TRUE(runtimeAndModeUnchanged(view, result));
}

void test_capacity_reached_rejects_without_state_change() {
    auto view = makeView(SensorSelectionPhase::NormalProduct,
                         SensorPeltierPermission::Allowed, RunSensorMode::Product,
                         persistedState(SensorSelectionProvenance::InitialSelection,
                                       SensorSelectionDecisionCause::StartSelection,
                                       std::numeric_limits<std::uint32_t>::max()),
                         std::numeric_limits<std::uint32_t>::max());
    auto decision = makeDecision(view, SensorPreference::ProductIfAvailableElseAir,
                                 ProductSensorFailurePolicy::StopToSafeState,
                                 ReturnStrategy::ManualReturnToProduct, validSnapshot(),
                                 failedSnapshot(), validSnapshot());

    const auto result = applySensorSelectionDecision(view, decision, 100U);

    TEST_ASSERT_TRUE(result.status == SensorSelectionApplyStatus::CapacityReached);
    TEST_ASSERT_TRUE(runtimeAndModeUnchanged(view, result));
}

void test_checked_fallback_delay_overflow_is_invalid_context() {
    // uint32_t-Sekunden * 1000 passen immer in uint64_t (checkedMillisFrom-
    // Seconds kann fuer keinen gueltigen uint32_t-Wert ueberlaufen); der
    // erreichbare Ueberlauf liegt in der checkedAdd-Summe aus einem nahe
    // UINT64_MAX liegenden fallbackWaitStartedAtMonotonicMillis.
    auto view = makeView(SensorSelectionPhase::ProductFailureDetected,
                         SensorPeltierPermission::Blocked, RunSensorMode::Product,
                         persistedState(SensorSelectionProvenance::InitialSelection,
                                       SensorSelectionDecisionCause::ProductFailureBlock,
                                       2U),
                         2U);
    view.runtime.fallbackWaitStartedAtMonotonicMillis =
        std::numeric_limits<std::uint64_t>::max() - 100U;
    auto decision = makeDecision(
        view, SensorPreference::ProductIfAvailableElseAir,
        ProductSensorFailurePolicy::FallbackToAirAfterTimeout,
        ReturnStrategy::AutomaticValidatedReturnToProduct, validSnapshot(),
        failedSnapshot(), validSnapshot(), 1U);

    const auto result = applySensorSelectionDecision(
        view, decision, std::numeric_limits<std::uint64_t>::max());

    TEST_ASSERT_TRUE(result.status == SensorSelectionApplyStatus::InvalidContext);
    TEST_ASSERT_TRUE(runtimeAndModeUnchanged(view, result));
}

void test_stale_quality_is_unusable_like_failed() {
    auto view = makeView(SensorSelectionPhase::NormalProduct,
                         SensorPeltierPermission::Allowed, RunSensorMode::Product,
                         persistedState(SensorSelectionProvenance::InitialSelection,
                                       SensorSelectionDecisionCause::StartSelection, 1U),
                         1U);
    auto decision = makeDecision(view, SensorPreference::ProductIfAvailableElseAir,
                                 ProductSensorFailurePolicy::FallbackToAirAfterTimeout,
                                 ReturnStrategy::AutomaticValidatedReturnToProduct,
                                 validSnapshot(), staleSnapshot(), validSnapshot());

    const auto result = applySensorSelectionDecision(view, decision, 100U);

    TEST_ASSERT_TRUE(result.status ==
                     SensorSelectionApplyStatus::AppliedPersistentCandidate);
    TEST_ASSERT_TRUE(result.runtime.phase ==
                     SensorSelectionPhase::ProductFailureDetected);
    TEST_ASSERT_TRUE(result.runtime.permission == SensorPeltierPermission::Blocked);
}

void test_no_change_produces_no_mutation() {
    auto view = makeView(SensorSelectionPhase::NormalProduct,
                         SensorPeltierPermission::Allowed, RunSensorMode::Product,
                         persistedState(SensorSelectionProvenance::InitialSelection,
                                       SensorSelectionDecisionCause::StartSelection, 1U),
                         1U);
    auto decision = makeDecision(view, SensorPreference::ProductIfAvailableElseAir,
                                 ProductSensorFailurePolicy::FallbackToAirAfterTimeout,
                                 ReturnStrategy::AutomaticValidatedReturnToProduct,
                                 validSnapshot(), validSnapshot(), validSnapshot());

    const auto result = applySensorSelectionDecision(view, decision, 100U);

    TEST_ASSERT_TRUE(result.status == SensorSelectionApplyStatus::NoChange);
    TEST_ASSERT_TRUE(runtimeAndModeUnchanged(view, result));
    TEST_ASSERT_FALSE(result.event.has_value());
    TEST_ASSERT_FALSE(result.notice.has_value());
}

// ---------------------------------------------------------------------------
// ThermalCompatibilityEvidence-Invarianten (6.10)
// ---------------------------------------------------------------------------

void test_thermal_evidence_zero_profile_revision_with_compatible_is_invalid_context() {
    // Korrekturauftrag Befund 6: strukturell ungueltige Fremdevidenz
    // (Compatible mit profileRevision == 0) liefert an diesem
    // evidenzverbrauchenden Rueckkehrpfad InvalidContext statt der zuvor
    // abweichenden NoChange-Auslegung - der Zustand bleibt dabei unveraendert
    // (ReturnValidationPending, keine Mutation), nur der Status macht das
    // Evidenzproblem sichtbar statt es stillschweigend zu absorbieren.
    auto view = makeView(SensorSelectionPhase::ReturnValidationPending,
                         SensorPeltierPermission::Allowed, RunSensorMode::Air,
                         persistedState(SensorSelectionProvenance::FallbackActive,
                                       SensorSelectionDecisionCause::FallbackToAir, 2U),
                         2U);
    view.runtime.returnValidation.enteredAtMonotonicMillis = 500U;
    auto decision = makeDecision(view, SensorPreference::ProductIfAvailableElseAir,
                                 ProductSensorFailurePolicy::FallbackToAirAfterTimeout,
                                 ReturnStrategy::AutomaticValidatedReturnToProduct,
                                 validSnapshot(), validSnapshot(), validSnapshot());
    decision.plausibility.thermalCompatibility.status = ThermalCompatibility::Compatible;
    decision.plausibility.thermalCompatibility.profileRevision = 0U;

    const auto result = applySensorSelectionDecision(view, decision, 600U);

    TEST_ASSERT_TRUE(result.status == SensorSelectionApplyStatus::InvalidContext);
    TEST_ASSERT_TRUE(result.runtime.phase == SensorSelectionPhase::ReturnValidationPending);
    TEST_ASSERT_TRUE(result.runtime == view.runtime);
}

void test_thermal_evidence_future_timestamp_is_invalid_context() {
    // Wie oben: eine strukturell ungueltige Fremdevidenz (Zeitstempel in der
    // Zukunft) liefert InvalidContext, blockiert aber nur diesen
    // Rueckkehrversuch - der Automat selbst friert nicht ein.
    auto view = makeView(SensorSelectionPhase::ReturnValidationPending,
                         SensorPeltierPermission::Allowed, RunSensorMode::Air,
                         persistedState(SensorSelectionProvenance::FallbackActive,
                                       SensorSelectionDecisionCause::FallbackToAir, 2U),
                         2U);
    view.runtime.returnValidation.enteredAtMonotonicMillis = 500U;
    auto decision = makeDecision(view, SensorPreference::ProductIfAvailableElseAir,
                                 ProductSensorFailurePolicy::FallbackToAirAfterTimeout,
                                 ReturnStrategy::AutomaticValidatedReturnToProduct,
                                 validSnapshot(), validSnapshot(), validSnapshot());
    decision.plausibility.evaluationMonotonicMillis = 100U;
    decision.plausibility.thermalCompatibility.evaluatedAtMonotonicMillis = 200U;

    const auto result = applySensorSelectionDecision(view, decision, 600U);

    TEST_ASSERT_TRUE(result.status == SensorSelectionApplyStatus::InvalidContext);
    TEST_ASSERT_TRUE(result.runtime.phase == SensorSelectionPhase::ReturnValidationPending);
    TEST_ASSERT_TRUE(result.runtime == view.runtime);
}

void test_normal_product_ignores_structurally_invalid_thermal_evidence() {
    // Regressionstest fuer den Fail-Open-Fund: eine strukturell ungueltige
    // Fremdevidenz (hier zusaetzlich Stale mit profileRevision == 0) darf
    // einen echten Produktsensorausfall in NormalProduct nicht verdecken.
    // Vor der Korrektur haette das globale Gate hier InvalidContext geliefert
    // und ProductFailureBlock waere nie ausgeloest worden.
    auto view = makeView(SensorSelectionPhase::NormalProduct,
                         SensorPeltierPermission::Allowed, RunSensorMode::Product,
                         persistedState(SensorSelectionProvenance::InitialSelection,
                                       SensorSelectionDecisionCause::StartSelection, 1U),
                         1U);
    auto decision = makeDecision(view, SensorPreference::ProductIfAvailableElseAir,
                                 ProductSensorFailurePolicy::FallbackToAirAfterTimeout,
                                 ReturnStrategy::AutomaticValidatedReturnToProduct,
                                 validSnapshot(), failedSnapshot(), validSnapshot());
    decision.plausibility.thermalCompatibility.status = ThermalCompatibility::Stale;
    decision.plausibility.thermalCompatibility.profileRevision = 0U;

    const auto result = applySensorSelectionDecision(view, decision, 100U);

    TEST_ASSERT_TRUE(result.status ==
                     SensorSelectionApplyStatus::AppliedPersistentCandidate);
    TEST_ASSERT_TRUE(result.runtime.phase ==
                     SensorSelectionPhase::ProductFailureDetected);
    TEST_ASSERT_TRUE(result.runtime.permission == SensorPeltierPermission::Blocked);
}

void test_thermal_evidence_unavailable_never_grants_return() {
    auto view = makeView(SensorSelectionPhase::ReturnValidationPending,
                         SensorPeltierPermission::Allowed, RunSensorMode::Air,
                         persistedState(SensorSelectionProvenance::FallbackActive,
                                       SensorSelectionDecisionCause::FallbackToAir, 2U),
                         2U);
    view.runtime.returnValidation.enteredAtMonotonicMillis = 500U;
    auto decision = makeDecision(view, SensorPreference::ProductIfAvailableElseAir,
                                 ProductSensorFailurePolicy::FallbackToAirAfterTimeout,
                                 ReturnStrategy::AutomaticValidatedReturnToProduct,
                                 validSnapshot(), validSnapshot(), validSnapshot());
    decision.plausibility.direction = AbstractControlDirection::Cooling;
    decision.plausibility.controlDemandAgeMs = 10U;
    decision.plausibility.thermalCompatibility.status = ThermalCompatibility::Unavailable;

    const auto result = applySensorSelectionDecision(view, decision, 600U);

    TEST_ASSERT_TRUE(result.status == SensorSelectionApplyStatus::NoChange);
    TEST_ASSERT_TRUE(result.runtime.phase == SensorSelectionPhase::ReturnValidationPending);
}

void test_thermal_evidence_no_own_age_threshold_is_evaluated() {
    // Regressionstest: #21 wertet evaluatedAtMonotonicMillis nur gegen die
    // Zukunftspruefung aus, fuehrt aber keine eigene Alters-/Stale-Bewertung
    // durch. Ein sehr altes, aber nicht-zukuenftiges Compatible bleibt gueltig.
    auto view = makeView(SensorSelectionPhase::ReturnValidationPending,
                         SensorPeltierPermission::Allowed, RunSensorMode::Air,
                         persistedState(SensorSelectionProvenance::FallbackActive,
                                       SensorSelectionDecisionCause::FallbackToAir, 2U),
                         2U);
    view.runtime.returnValidation.enteredAtMonotonicMillis = 500U;
    view.runtime.returnValidation.lastObservedProfileRevision = 3U;
    auto decision = makeDecision(view, SensorPreference::ProductIfAvailableElseAir,
                                 ProductSensorFailurePolicy::FallbackToAirAfterTimeout,
                                 ReturnStrategy::AutomaticValidatedReturnToProduct,
                                 validSnapshot(), validSnapshot(), validSnapshot());
    decision.plausibility.direction = AbstractControlDirection::Cooling;
    decision.plausibility.controlDemandAgeMs = 10U;
    decision.plausibility.thermalCompatibility.status = ThermalCompatibility::Compatible;
    decision.plausibility.thermalCompatibility.profileRevision = 3U;
    decision.plausibility.thermalCompatibility.evaluatedAtMonotonicMillis = 1U;
    decision.plausibility.evaluationMonotonicMillis = 1'000'000U;

    const auto result = applySensorSelectionDecision(view, decision, 600U);

    TEST_ASSERT_TRUE(result.status ==
                     SensorSelectionApplyStatus::AppliedPersistentCandidate);
    TEST_ASSERT_TRUE(result.runtime.phase == SensorSelectionPhase::NormalProduct);
}

// ---------------------------------------------------------------------------
// computeRestartSensorSelection (6.12.3, reine Funktion)
// ---------------------------------------------------------------------------

void test_restart_recommendation_is_fail_closed_for_legacy_unknown() {
    const PersistedSensorSelectionState persisted{
        SensorSelectionProvenance::LegacyUnknown, SensorSelectionDecisionCause::None,
        0U};
    const SensorSelectionProgramContext program{
        SensorPreference::ProductIfAvailableElseAir,
        ProductSensorFailurePolicy::FallbackToAirAfterTimeout,
        ReturnStrategy::AutomaticValidatedReturnToProduct, 60U};

    const auto recommendation =
        computeRestartSensorSelection(persisted, RunSensorMode::Product, program);

    TEST_ASSERT_TRUE(recommendation.runtime.phase ==
                     SensorSelectionPhase::RestartRevalidationPending);
    TEST_ASSERT_TRUE(recommendation.runtime.permission ==
                     SensorPeltierPermission::Blocked);
    TEST_ASSERT_TRUE(recommendation.activeMode == RunSensorMode::Product);
    TEST_ASSERT_FALSE(
        recommendation.runtime.returnValidation.enteredAtMonotonicMillis
            .has_value());
}

void test_restart_recommendation_is_fail_closed_for_fallback_active() {
    const PersistedSensorSelectionState persisted{
        SensorSelectionProvenance::FallbackActive,
        SensorSelectionDecisionCause::FallbackToAir, 5U};
    const SensorSelectionProgramContext program{
        SensorPreference::ProductIfAvailableElseAir,
        ProductSensorFailurePolicy::FallbackToAirAfterTimeout,
        ReturnStrategy::AutomaticValidatedReturnToProduct, 60U};

    const auto recommendation =
        computeRestartSensorSelection(persisted, RunSensorMode::Air, program);

    TEST_ASSERT_TRUE(recommendation.runtime.phase ==
                     SensorSelectionPhase::RestartRevalidationPending);
    TEST_ASSERT_TRUE(recommendation.runtime.permission ==
                     SensorPeltierPermission::Blocked);
    TEST_ASSERT_TRUE(recommendation.activeMode == RunSensorMode::Air);
}

void test_restart_recommendation_is_fail_closed_for_returned_to_product() {
    const PersistedSensorSelectionState persisted{
        SensorSelectionProvenance::ReturnedToProduct,
        SensorSelectionDecisionCause::AutomaticValidatedReturn, 9U};
    const SensorSelectionProgramContext program{
        SensorPreference::ProductIfAvailableElseAir,
        ProductSensorFailurePolicy::FallbackToAirAfterTimeout,
        ReturnStrategy::AutomaticValidatedReturnToProduct, 60U};

    const auto recommendation =
        computeRestartSensorSelection(persisted, RunSensorMode::Product, program);

    TEST_ASSERT_TRUE(recommendation.runtime.phase ==
                     SensorSelectionPhase::RestartRevalidationPending);
    TEST_ASSERT_TRUE(recommendation.runtime.permission ==
                     SensorPeltierPermission::Blocked);
    TEST_ASSERT_TRUE(recommendation.activeMode == RunSensorMode::Product);
}

// ---------------------------------------------------------------------------
// Korrekturauftrag Befund 2: Air-/Cooling-Sicherheitsreaktion vor jeder
// Rueckkehraktion, auch bei nur einzeln ungueltigem Sensor.
// ---------------------------------------------------------------------------

void test_return_to_product_blocked_by_single_air_sensor_failure() {
    auto view = makeView(SensorSelectionPhase::AirFallbackActive,
                         SensorPeltierPermission::Allowed, RunSensorMode::Air,
                         persistedState(SensorSelectionProvenance::FallbackActive,
                                       SensorSelectionDecisionCause::FallbackToAir, 2U),
                         2U);
    auto decision = makeDecision(
        view, SensorPreference::ProductIfAvailableElseAir,
        ProductSensorFailurePolicy::FallbackToAirAfterTimeout,
        ReturnStrategy::ManualReturnToProduct, failedSnapshot(), validSnapshot(),
        validSnapshot(), std::nullopt, SensorSelectionUserAction::ReturnToProduct);

    const auto result = applySensorSelectionDecision(view, decision, 900U);

    TEST_ASSERT_TRUE(result.status ==
                     SensorSelectionApplyStatus::AppliedPersistentCandidate);
    TEST_ASSERT_TRUE(result.runtime.phase == SensorSelectionPhase::SafeLocked);
    TEST_ASSERT_TRUE(result.runtime.permission == SensorPeltierPermission::Blocked);
    TEST_ASSERT_TRUE(result.activeMode == RunSensorMode::Air);
}

void test_return_to_product_blocked_by_single_cooling_sensor_failure() {
    auto view = makeView(SensorSelectionPhase::AirFallbackActive,
                         SensorPeltierPermission::Allowed, RunSensorMode::Air,
                         persistedState(SensorSelectionProvenance::FallbackActive,
                                       SensorSelectionDecisionCause::FallbackToAir, 2U),
                         2U);
    auto decision = makeDecision(
        view, SensorPreference::ProductIfAvailableElseAir,
        ProductSensorFailurePolicy::FallbackToAirAfterTimeout,
        ReturnStrategy::ManualReturnToProduct, validSnapshot(), validSnapshot(),
        failedSnapshot(), std::nullopt, SensorSelectionUserAction::ReturnToProduct);

    const auto result = applySensorSelectionDecision(view, decision, 900U);

    TEST_ASSERT_TRUE(result.status ==
                     SensorSelectionApplyStatus::AppliedPersistentCandidate);
    TEST_ASSERT_TRUE(result.runtime.phase == SensorSelectionPhase::SafeLocked);
    TEST_ASSERT_TRUE(result.runtime.permission == SensorPeltierPermission::Blocked);
}

void test_recheck_product_in_product_failure_detected_blocked_by_single_air_failure() {
    // ProductFailureDetected haelt die 6.4.1-Asymmetrie (nur gleichzeitiger
    // Ausfall verlaesst die Phase) - ein einzelner Air-Ausfall blockiert
    // dennoch die Rueckkehr nach NormalProduct (productReturnEligible).
    auto view = makeView(SensorSelectionPhase::ProductFailureDetected,
                         SensorPeltierPermission::Blocked, RunSensorMode::Product,
                         persistedState(SensorSelectionProvenance::InitialSelection,
                                       SensorSelectionDecisionCause::ProductFailureBlock, 2U),
                         2U);
    view.runtime.fallbackWaitStartedAtMonotonicMillis = 1000U;
    auto decision = makeDecision(
        view, SensorPreference::ProductIfAvailableElseAir,
        ProductSensorFailurePolicy::FallbackToAirAfterTimeout,
        ReturnStrategy::AutomaticValidatedReturnToProduct, failedSnapshot(),
        validSnapshot(), validSnapshot(), 60U,
        SensorSelectionUserAction::RecheckProduct);

    const auto result = applySensorSelectionDecision(view, decision, 1100U);

    TEST_ASSERT_TRUE(result.status == SensorSelectionApplyStatus::NoChange);
    TEST_ASSERT_TRUE(result.runtime.phase == SensorSelectionPhase::ProductFailureDetected);
}

void test_recheck_product_in_user_decision_required_blocked_by_single_cooling_failure() {
    // UserDecisionRequired kennt keinen "gleichzeitig"-Qualifier (6.4.2) - ein
    // einzelner Cooling-Ausfall fuehrt hier direkt zu SafeLocked, noch vor der
    // Benutzeraktion.
    auto view = makeView(SensorSelectionPhase::UserDecisionRequired,
                         SensorPeltierPermission::Blocked, RunSensorMode::Product,
                         persistedState(SensorSelectionProvenance::InitialSelection,
                                       SensorSelectionDecisionCause::ProductFailureBlock, 2U),
                         2U);
    auto decision = makeDecision(
        view, SensorPreference::ProductIfAvailableElseAir,
        ProductSensorFailurePolicy::WaitForUser, ReturnStrategy::ManualReturnToProduct,
        validSnapshot(), validSnapshot(), failedSnapshot(), std::nullopt,
        SensorSelectionUserAction::RecheckProduct);

    const auto result = applySensorSelectionDecision(view, decision, 1100U);

    TEST_ASSERT_TRUE(result.status ==
                     SensorSelectionApplyStatus::AppliedPersistentCandidate);
    TEST_ASSERT_TRUE(result.runtime.phase == SensorSelectionPhase::SafeLocked);
    TEST_ASSERT_TRUE(result.runtime.permission == SensorPeltierPermission::Blocked);
}

// ---------------------------------------------------------------------------
// Korrekturauftrag Befund 5: fail-closed Kernvalidierung.
// ---------------------------------------------------------------------------

void test_malformed_state_missing_active_mode_on_mode_change_is_invalid_decision() {
    // Strukturell unmoegliches, aber von der Kernfunktion nicht blind
    // angenommenes malformed State: activeMode fehlt, obwohl die Phase einen
    // persistenzwuerdigen Moduswechsel anfordert. Keine Dereferenzierung,
    // stattdessen InvalidDecision, Zustand unveraendert.
    auto view = makeView(SensorSelectionPhase::ProductFailureDetected,
                         SensorPeltierPermission::Blocked, std::nullopt,
                         persistedState(SensorSelectionProvenance::InitialSelection,
                                       SensorSelectionDecisionCause::ProductFailureBlock, 2U),
                         2U);
    auto decision = makeDecision(
        view, SensorPreference::ProductIfAvailableElseAir,
        ProductSensorFailurePolicy::FallbackToAirAfterTimeout,
        ReturnStrategy::AutomaticValidatedReturnToProduct, validSnapshot(),
        failedSnapshot(), validSnapshot(), 60U,
        SensorSelectionUserAction::ContinueWithAir);

    const auto result = applySensorSelectionDecision(view, decision, 1100U);

    TEST_ASSERT_TRUE(result.status == SensorSelectionApplyStatus::InvalidDecision);
    TEST_ASSERT_TRUE(runtimeAndModeUnchanged(view, result));
}

void test_malformed_state_unknown_control_direction_never_grants_return() {
    // Whitelist statt Blacklist - ein unbekannter AbstractControlDirection-
    // Rohwert (keiner der drei bekannten Nicht-Unknown-Werte) darf trotz
    // sonst vollstaendig positiver Evidenz keine automatische Rueckkehr
    // freigeben.
    auto view = makeView(SensorSelectionPhase::ReturnValidationPending,
                         SensorPeltierPermission::Allowed, RunSensorMode::Air,
                         persistedState(SensorSelectionProvenance::FallbackActive,
                                       SensorSelectionDecisionCause::FallbackToAir, 2U),
                         2U);
    view.runtime.returnValidation.enteredAtMonotonicMillis = 500U;
    auto decision = makeDecision(view, SensorPreference::ProductIfAvailableElseAir,
                                 ProductSensorFailurePolicy::FallbackToAirAfterTimeout,
                                 ReturnStrategy::AutomaticValidatedReturnToProduct,
                                 validSnapshot(), validSnapshot(), validSnapshot());
    decision.plausibility.direction = static_cast<AbstractControlDirection>(99U);
    decision.plausibility.controlDemandAgeMs = 10U;
    decision.plausibility.thermalCompatibility.status = ThermalCompatibility::Compatible;
    decision.plausibility.thermalCompatibility.profileRevision = 7U;

    const auto result = applySensorSelectionDecision(view, decision, 900U);

    TEST_ASSERT_TRUE(result.status == SensorSelectionApplyStatus::NoChange);
    TEST_ASSERT_TRUE(result.runtime.phase == SensorSelectionPhase::ReturnValidationPending);
}

// ---------------------------------------------------------------------------
// Korrekturauftrag Befund 6: strukturell ungueltige Evidenz -> InvalidContext
// an den beiden zusaetzlichen evidenzverbrauchenden Stellen in
// AirFallbackActive (RecheckProduct und automatischer Re-Arm).
// ---------------------------------------------------------------------------

void test_air_fallback_recheck_product_with_malformed_evidence_is_invalid_context() {
    auto view = makeView(SensorSelectionPhase::AirFallbackActive,
                         SensorPeltierPermission::Allowed, RunSensorMode::Air,
                         persistedState(SensorSelectionProvenance::FallbackActive,
                                       SensorSelectionDecisionCause::FallbackToAir, 2U),
                         2U);
    auto decision = makeDecision(
        view, SensorPreference::ProductIfAvailableElseAir,
        ProductSensorFailurePolicy::FallbackToAirAfterTimeout,
        ReturnStrategy::AutomaticValidatedReturnToProduct, validSnapshot(),
        validSnapshot(), validSnapshot(), std::nullopt,
        SensorSelectionUserAction::RecheckProduct);
    decision.plausibility.thermalCompatibility.status = ThermalCompatibility::Compatible;
    decision.plausibility.thermalCompatibility.profileRevision = 0U;  // malformed

    const auto result = applySensorSelectionDecision(view, decision, 900U);

    TEST_ASSERT_TRUE(result.status == SensorSelectionApplyStatus::InvalidContext);
    TEST_ASSERT_TRUE(runtimeAndModeUnchanged(view, result));
}

void test_air_fallback_automatic_rearm_with_malformed_evidence_is_invalid_context() {
    auto view = makeView(SensorSelectionPhase::AirFallbackActive,
                         SensorPeltierPermission::Allowed, RunSensorMode::Air,
                         persistedState(SensorSelectionProvenance::FallbackActive,
                                       SensorSelectionDecisionCause::FallbackToAir, 2U),
                         2U);
    view.runtime.returnValidation.lastObservedProfileRevision = 5U;
    auto decision = makeDecision(view, SensorPreference::ProductIfAvailableElseAir,
                                 ProductSensorFailurePolicy::FallbackToAirAfterTimeout,
                                 ReturnStrategy::AutomaticValidatedReturnToProduct,
                                 validSnapshot(), validSnapshot(), validSnapshot());
    decision.plausibility.thermalCompatibility.status = ThermalCompatibility::Incompatible;
    // profileRevision 0 ist strukturell ungueltig, aber != 5 - Bedingung (i)
    // (profileChanged) waere sonst erfuellt und wuerde den Re-Arm-Versuch
    // auslösen.
    decision.plausibility.thermalCompatibility.profileRevision = 0U;

    const auto result = applySensorSelectionDecision(view, decision, 900U);

    TEST_ASSERT_TRUE(result.status == SensorSelectionApplyStatus::InvalidContext);
    TEST_ASSERT_TRUE(runtimeAndModeUnchanged(view, result));
}

}  // namespace

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_fallback_after_timeout_stays_blocked_before_timeout);
    RUN_TEST(test_fallback_after_timeout_falls_back_to_air_when_valid);
    RUN_TEST(test_fallback_after_timeout_reaches_safe_locked_without_air);
    RUN_TEST(test_continue_with_air_before_timeout_is_manual_fallback);
    RUN_TEST(test_continue_with_air_without_air_cooling_is_invalid_input);
    RUN_TEST(
        test_continue_with_air_from_user_decision_required_with_single_air_failure_is_invalid_input);
    RUN_TEST(
        test_continue_with_air_in_product_failure_detected_with_simultaneous_failure_is_invalid_input);
    RUN_TEST(test_safety_pending_matrix_is_out_of_scope_for_core_function);
    RUN_TEST(test_wait_for_user_transitions_immediately_without_wait);
    RUN_TEST(test_wait_for_user_continue_with_air_from_user_decision_required);
    RUN_TEST(test_wait_for_user_continue_with_air_rejected_for_product_required);
    RUN_TEST(test_wait_for_user_product_recovers_without_user_action);
    RUN_TEST(test_wait_for_user_recheck_product_still_invalid_is_no_change);
    RUN_TEST(test_user_decision_required_air_loss_reaches_safe_locked);
    RUN_TEST(test_stop_to_safe_state_reaches_safe_locked_without_product_failure_block);
    RUN_TEST(test_stop_to_safe_state_stays_locked_when_product_recovers);
    RUN_TEST(test_precedence_simultaneous_air_cooling_failure_during_return_validation);
    RUN_TEST(test_rearm_ten_thousand_unavailable_evaluations_never_write);
    RUN_TEST(test_rearm_repeated_incompatible_aborts_at_most_once_per_attempt);
    RUN_TEST(test_rearm_new_evidence_generation_starts_exactly_one_new_attempt);
    RUN_TEST(test_rearm_without_evidence_change_does_not_reenter);
    RUN_TEST(test_rearm_compatible_after_full_stability_returns_exactly_once);
    RUN_TEST(test_rearm_product_failure_during_return_validation_needs_new_recognition);
    RUN_TEST(test_rearm_invalid_to_valid_product_cycle_rearms_without_profile_change);
    RUN_TEST(test_recheck_product_from_air_fallback_with_incomplete_evidence_is_ram_only);
    RUN_TEST(test_recheck_product_from_disallowed_state_is_rejected);
    RUN_TEST(test_stale_decision_on_changed_phase_leaves_state_untouched);
    RUN_TEST(test_stale_decision_on_changed_provenance_leaves_state_untouched);
    RUN_TEST(test_stale_decision_on_changed_run_revision_leaves_state_untouched);
    RUN_TEST(test_invalid_decision_on_unreachable_runtime_combination);
    RUN_TEST(test_invalid_context_for_no_active_run_and_restart_revalidation);
    RUN_TEST(test_time_went_backwards_rejects_without_state_change);
    RUN_TEST(test_capacity_reached_rejects_without_state_change);
    RUN_TEST(test_checked_fallback_delay_overflow_is_invalid_context);
    RUN_TEST(test_stale_quality_is_unusable_like_failed);
    RUN_TEST(test_no_change_produces_no_mutation);
    RUN_TEST(test_thermal_evidence_zero_profile_revision_with_compatible_is_invalid_context);
    RUN_TEST(test_thermal_evidence_future_timestamp_is_invalid_context);
    RUN_TEST(test_thermal_evidence_unavailable_never_grants_return);
    RUN_TEST(test_thermal_evidence_no_own_age_threshold_is_evaluated);
    RUN_TEST(test_normal_product_ignores_structurally_invalid_thermal_evidence);
    RUN_TEST(test_restart_recommendation_is_fail_closed_for_legacy_unknown);
    RUN_TEST(test_restart_recommendation_is_fail_closed_for_fallback_active);
    RUN_TEST(test_restart_recommendation_is_fail_closed_for_returned_to_product);
    RUN_TEST(test_return_to_product_blocked_by_single_air_sensor_failure);
    RUN_TEST(test_return_to_product_blocked_by_single_cooling_sensor_failure);
    RUN_TEST(
        test_recheck_product_in_product_failure_detected_blocked_by_single_air_failure);
    RUN_TEST(
        test_recheck_product_in_user_decision_required_blocked_by_single_cooling_failure);
    RUN_TEST(test_malformed_state_missing_active_mode_on_mode_change_is_invalid_decision);
    RUN_TEST(test_malformed_state_unknown_control_direction_never_grants_return);
    RUN_TEST(
        test_air_fallback_recheck_product_with_malformed_evidence_is_invalid_context);
    RUN_TEST(
        test_air_fallback_automatic_rearm_with_malformed_evidence_is_invalid_context);
    return UNITY_END();
}
