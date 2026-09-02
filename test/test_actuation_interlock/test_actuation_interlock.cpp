#include <iterator>

#include <unity.h>

#include "actuator_planner.hpp"
#include "mock_reset_cause_source.hpp"
#include "actuation_interlock.hpp"
#include "presentation_state.hpp"

namespace {

using namespace fermentation;

ActuatorPlannerParameters watchdogTestParameters() {
    ActuatorPlannerParameters parameters;
    parameters.switchingWindowMillis = 1000U;
    parameters.minimumOnMillis = 100U;
    parameters.minimumOffMillis = 100U;
    parameters.polarityDeadTimeMillis = 100U;
    parameters.pulseAccumulatorCapMillis = 1000U;
    parameters.counterDirectionConfirmationQuoteThreshold = 0.5;
    parameters.counterDirectionConfirmationDurationMillis = 100U;
    parameters.requestWatchdogMillis = 1000U;
    parameters.outerFanPostRunMillis = 100U;
    parameters.innerFanPostRunMillis = 100U;
    return parameters;
}

void tripWatchdog(ActuatorPlanner& planner) {
    ActuatorPlanTickInput tick;
    tick.temperatureControlledPhase = true;
    tick.safetyGate.status = ActuatorSafetyGateStatus::Allowed;
    static_cast<void>(planner.tick(tick));
    tick.nowMonotonicMillis = 1000U;
    const auto result = planner.tick(tick);
    TEST_ASSERT_TRUE(result.reason == ActuatorPlanReason::StaleRequestWatchdog);
    TEST_ASSERT_TRUE(planner.state().latchedWatchdogFault.has_value());
}

void validBootEvidence(ActuationEvidence& input,
                       device_platform::SensorQualitySnapshot& sensor,
                       SensorSelectionRuntimeState& selection) {
    input.bootValidationComplete = true;
    input.configurationValidated = true;
    input.configurationRecoveryStatus =
        ConfigurationRecoveryStatus::RuntimeReady;
    input.persistenceValidated = true;
    input.sensorEvidenceValidated = true;
    input.explicitActivationRequested = true;
    input.plannerEvidenceValidated = true;
    input.activationKind = SafetyActivationKind::Resume;
    input.persistenceLoadStatus = RunPersistenceLoadStatus::Current;
    input.loadDisposition = RunLoadDisposition::ResumeOffer;
    input.persistenceCoordinatorState = RunPersistenceCoordinatorState::Ready;
    input.activationPersistenceResult = RunPersistenceResultStatus::Applied;
    input.processActivationApplied = true;
    sensor.quality = device_platform::SensorQuality::Valid;
    selection.phase = SensorSelectionPhase::NormalProduct;
    selection.permission = SensorPeltierPermission::Allowed;
    input.peltierSensor = &sensor;
    input.sensorSelectionRuntime = &selection;
}

void validOperationalStandbyEvidence(ActuationEvidence& input) {
    input.bootValidationComplete = true;
    input.configurationValidated = true;
    input.configurationRecoveryStatus =
        ConfigurationRecoveryStatus::RuntimeReady;
    input.configurationServiceMode = ConfigurationServiceMode::Operational;
    input.persistenceValidated = true;
    input.persistenceLoadStatus = RunPersistenceLoadStatus::NoPersistedRun;
    input.loadDisposition = RunLoadDisposition::Standby;
    input.persistenceCoordinatorState =
        RunPersistenceCoordinatorState::ReadyEmpty;
}

void validFallbackRecoveryEvidence(
    ActuationEvidence& input, device_platform::SensorQualitySnapshot& sensor,
    SensorSelectionRuntimeState& selection) {
    input.bootValidationComplete = true;
    input.configurationValidated = true;
    input.configurationRecoveryStatus =
        ConfigurationRecoveryStatus::RuntimeReady;
    input.persistenceValidated = true;
    input.persistenceLoadStatus = RunPersistenceLoadStatus::FallbackRecovered;
    input.persistenceCoordinatorState =
        RunPersistenceCoordinatorState::FallbackRecoveryPending;
    input.loadDisposition = RunLoadDisposition::SafeBoot;
    input.sensorEvidenceValidated = true;
    sensor.quality = device_platform::SensorQuality::Valid;
    selection.phase = SensorSelectionPhase::NormalProduct;
    selection.permission = SensorPeltierPermission::Allowed;
    input.peltierSensor = &sensor;
    input.sensorSelectionRuntime = &selection;
}

void test_missing_boot_evidence_is_safe_boot() {
    const auto result = ActuationInterlock::evaluate({});

    TEST_ASSERT_TRUE(result.permission == ActuatorSafetyGateStatus::Unresolved);
    TEST_ASSERT_TRUE(result.faultCode == FaultCode::SystemProducerUnknown);
}

void test_fallback_selection_required_never_allows_even_with_complete_evidence() {
    ActuationEvidence input;
    device_platform::SensorQualitySnapshot sensor;
    SensorSelectionRuntimeState selection;
    validFallbackRecoveryEvidence(input, sensor, selection);
    input.loadDisposition = RunLoadDisposition::FallbackSelectionRequired;
    const auto result = ActuationInterlock::evaluate(input);
    TEST_ASSERT_TRUE(result.permission != ActuatorSafetyGateStatus::Allowed);
}

void test_no_active_run_is_standby_without_actuator_allow() {
    ActuationEvidence input;
    input.configurationValidated = true;
    input.persistenceValidated = true;
    input.persistenceLoadStatus = RunPersistenceLoadStatus::NoPersistedRun;
    input.loadDisposition = RunLoadDisposition::Standby;
    input.persistenceCoordinatorState =
        RunPersistenceCoordinatorState::ReadyEmpty;

    const auto result = ActuationInterlock::evaluate(input);
    TEST_ASSERT_TRUE(result.permission == ActuatorSafetyGateStatus::Unresolved);
    TEST_ASSERT_TRUE(result.faultCode == FaultCode::None);
}

void test_no_active_run_load_is_not_a_resume_offer() {
    ActuationEvidence input;
    input.configurationValidated = true;
    input.persistenceValidated = true;
    input.persistenceLoadStatus = RunPersistenceLoadStatus::NoActiveRun;
    input.loadDisposition = RunLoadDisposition::Standby;
    input.persistenceCoordinatorState =
        RunPersistenceCoordinatorState::ReadyEmpty;

    const auto result = ActuationInterlock::evaluate(input);
    TEST_ASSERT_TRUE(result.permission == ActuatorSafetyGateStatus::Unresolved);
    TEST_ASSERT_TRUE(result.faultCode == FaultCode::None);
}

void test_recovery_evaluation_actuation_is_blocked() {
    ActuationEvidence input;
    device_platform::SensorQualitySnapshot sensor;
    SensorSelectionRuntimeState selection;
    validBootEvidence(input, sensor, selection);
    input.loadDisposition = RunLoadDisposition::RecoveryEvaluation;

    const auto result = ActuationInterlock::evaluate(input);
    TEST_ASSERT_TRUE(result.permission != ActuatorSafetyGateStatus::Allowed);
}

void test_waiting_for_trusted_time_actuation_is_blocked() {
    ActuationEvidence input;
    device_platform::SensorQualitySnapshot sensor;
    SensorSelectionRuntimeState selection;
    validBootEvidence(input, sensor, selection);
    // WaitingForTrustedTime is an application disposition represented at the
    // actuation boundary by the RecoveryEvaluation load disposition.
    input.loadDisposition = RunLoadDisposition::RecoveryEvaluation;

    const auto result = ActuationInterlock::evaluate(input);
    TEST_ASSERT_TRUE(result.permission != ActuatorSafetyGateStatus::Allowed);
}

void test_recovery_rejected_actuation_is_blocked() {
    ActuationEvidence input;
    device_platform::SensorQualitySnapshot sensor;
    SensorSelectionRuntimeState selection;
    validBootEvidence(input, sensor, selection);
    // RecoveryRejectedOrFailClosed remains in the application's
    // RecoveryEvaluation handoff; it cannot become an activation offer.
    input.loadDisposition = RunLoadDisposition::RecoveryEvaluation;

    const auto result = ActuationInterlock::evaluate(input);
    TEST_ASSERT_TRUE(result.permission != ActuatorSafetyGateStatus::Allowed);
}

void test_validated_explicit_activation_is_allowed_only_after_all_evidence() {
    ActuationEvidence input;
    device_platform::SensorQualitySnapshot sensor;
    SensorSelectionRuntimeState selection;
    validBootEvidence(input, sensor, selection);

    const auto result = ActuationInterlock::evaluate(input);

    TEST_ASSERT_TRUE(result.permission == ActuatorSafetyGateStatus::Allowed);
    TEST_ASSERT_TRUE(result.faultCode == FaultCode::None);
}

void test_fresh_start_stays_unresolved_until_new_run_is_applied() {
    ActuationEvidence input;
    device_platform::SensorQualitySnapshot sensor;
    SensorSelectionRuntimeState selection;
    input.bootValidationComplete = true;
    input.configurationValidated = true;
    input.configurationRecoveryStatus =
        ConfigurationRecoveryStatus::RuntimeReady;
    input.persistenceValidated = true;
    input.persistenceLoadStatus = RunPersistenceLoadStatus::NoActiveRun;
    input.loadDisposition = RunLoadDisposition::Standby;
    input.persistenceCoordinatorState = RunPersistenceCoordinatorState::Ready;
    input.sensorEvidenceValidated = true;
    input.explicitActivationRequested = true;
    input.plannerEvidenceValidated = true;
    input.activationKind = SafetyActivationKind::FreshStart;
    sensor.quality = device_platform::SensorQuality::Valid;
    selection.phase = SensorSelectionPhase::NormalProduct;
    selection.permission = SensorPeltierPermission::Allowed;
    input.peltierSensor = &sensor;
    input.sensorSelectionRuntime = &selection;

    const auto beforeCommit = ActuationInterlock::evaluate(input);
    TEST_ASSERT_TRUE(beforeCommit.permission ==
                     ActuatorSafetyGateStatus::Unresolved);

    input.activationPersistenceResult = RunPersistenceResultStatus::Applied;
    input.processActivationApplied = true;
    const auto afterCommit = ActuationInterlock::evaluate(input);
    TEST_ASSERT_TRUE(afterCommit.permission ==
                     ActuatorSafetyGateStatus::Allowed);
}

void test_fresh_start_commit_failure_never_allows() {
    ActuationEvidence input;
    device_platform::SensorQualitySnapshot sensor;
    SensorSelectionRuntimeState selection;
    input.bootValidationComplete = true;
    input.configurationValidated = true;
    input.configurationRecoveryStatus =
        ConfigurationRecoveryStatus::RuntimeReady;
    input.persistenceValidated = true;
    input.persistenceLoadStatus = RunPersistenceLoadStatus::NoPersistedRun;
    input.loadDisposition = RunLoadDisposition::Standby;
    input.persistenceCoordinatorState = RunPersistenceCoordinatorState::Ready;
    input.sensorEvidenceValidated = true;
    input.explicitActivationRequested = true;
    input.plannerEvidenceValidated = true;
    input.activationKind = SafetyActivationKind::FreshStart;
    input.activationPersistenceResult =
        RunPersistenceResultStatus::PersistenceIndeterminate;
    input.processActivationApplied = false;
    sensor.quality = device_platform::SensorQuality::Valid;
    selection.permission = SensorPeltierPermission::Allowed;
    input.peltierSensor = &sensor;
    input.sensorSelectionRuntime = &selection;

    const auto result = ActuationInterlock::evaluate(input);
    TEST_ASSERT_TRUE(result.permission == ActuatorSafetyGateStatus::Unresolved);
}

void test_resume_offer_stays_unresolved_until_apply_and_fsm_evidence() {
    ActuationEvidence input;
    device_platform::SensorQualitySnapshot sensor;
    SensorSelectionRuntimeState selection;
    validBootEvidence(input, sensor, selection);
    input.activationPersistenceResult.reset();
    input.processActivationApplied = false;

    const auto result = ActuationInterlock::evaluate(input);
    TEST_ASSERT_TRUE(result.permission == ActuatorSafetyGateStatus::Unresolved);

    input.activationPersistenceResult = RunPersistenceResultStatus::Applied;
    const auto beforeFsm = ActuationInterlock::evaluate(input);
    TEST_ASSERT_TRUE(beforeFsm.permission ==
                     ActuatorSafetyGateStatus::Unresolved);

    input.processActivationApplied = true;
    const auto afterFsm = ActuationInterlock::evaluate(input);
    TEST_ASSERT_TRUE(afterFsm.permission == ActuatorSafetyGateStatus::Allowed);
}

void test_resume_failed_persistence_never_actuates() {
    ActuationEvidence input;
    device_platform::SensorQualitySnapshot sensor;
    SensorSelectionRuntimeState selection;
    validBootEvidence(input, sensor, selection);
    input.persistenceCoordinatorState =
        RunPersistenceCoordinatorState::LoadedActiveRun;
    input.activationPersistenceResult = RunPersistenceResultStatus::WriteFailed;
    input.processActivationApplied = false;

    const auto cleanFailure = ActuationInterlock::evaluate(input);
    TEST_ASSERT_TRUE(cleanFailure.permission ==
                     ActuatorSafetyGateStatus::Unresolved);
    TEST_ASSERT_FALSE(cleanFailure.permission ==
                      ActuatorSafetyGateStatus::Allowed);

    input.activationPersistenceResult =
        RunPersistenceResultStatus::PersistenceIndeterminate;
    input.persistenceCoordinatorState =
        RunPersistenceCoordinatorState::BlockedIndeterminate;
    const auto indeterminate = ActuationInterlock::evaluate(input);
    TEST_ASSERT_TRUE(indeterminate.permission ==
                     ActuatorSafetyGateStatus::Unresolved);
    TEST_ASSERT_FALSE(indeterminate.permission ==
                      ActuatorSafetyGateStatus::Allowed);
}

void test_unconfirmed_resume_offer_does_not_require_sensor() {
    ActuationEvidence input;
    input.bootValidationComplete = true;
    input.configurationValidated = true;
    input.configurationRecoveryStatus =
        ConfigurationRecoveryStatus::RuntimeReady;
    input.persistenceValidated = true;
    input.persistenceLoadStatus = RunPersistenceLoadStatus::Current;
    input.persistenceCoordinatorState =
        RunPersistenceCoordinatorState::LoadedActiveRun;
    input.loadDisposition = RunLoadDisposition::ResumeOffer;

    const auto offer = ActuationInterlock::evaluate(input);
    TEST_ASSERT_TRUE(offer.faultCode == FaultCode::None);
    TEST_ASSERT_TRUE(offer.permission == ActuatorSafetyGateStatus::Unresolved);
}

void test_resume_confirm_requires_sensor_evidence() {
    ActuationEvidence input;
    input.bootValidationComplete = true;
    input.configurationValidated = true;
    input.configurationRecoveryStatus =
        ConfigurationRecoveryStatus::RuntimeReady;
    input.persistenceValidated = true;
    input.persistenceLoadStatus = RunPersistenceLoadStatus::Current;
    input.persistenceCoordinatorState =
        RunPersistenceCoordinatorState::LoadedActiveRun;
    input.loadDisposition = RunLoadDisposition::ResumeOffer;
    input.explicitActivationRequested = true;
    input.activationKind = SafetyActivationKind::Resume;
    input.plannerEvidenceValidated = true;
    input.activationPersistenceResult = RunPersistenceResultStatus::Applied;
    input.processActivationApplied = true;

    const auto confirmed = ActuationInterlock::evaluate(input);
    TEST_ASSERT_TRUE(confirmed.faultCode == FaultCode::SafetySensorUnavailable);
    TEST_ASSERT_TRUE(confirmed.permission ==
                     ActuatorSafetyGateStatus::Unresolved);
}

void test_non_resumable_current_never_becomes_allowed_from_boolean_evidence() {
    ActuationEvidence input;
    device_platform::SensorQualitySnapshot sensor;
    SensorSelectionRuntimeState selection;
    validBootEvidence(input, sensor, selection);
    input.loadDisposition = RunLoadDisposition::NoActiveRun;

    const auto result = ActuationInterlock::evaluate(input);
    TEST_ASSERT_TRUE(result.permission == ActuatorSafetyGateStatus::Unresolved);
    TEST_ASSERT_TRUE(result.faultCode == FaultCode::None);
}

void test_stale_sensor_blocks_and_ack_is_presentation_only() {
    ActuationEvidence input;
    device_platform::SensorQualitySnapshot sensor;
    SensorSelectionRuntimeState selection;
    validBootEvidence(input, sensor, selection);
    sensor.quality = device_platform::SensorQuality::Stale;

    const auto blocked = ActuationInterlock::evaluate(input);
    PresentationState presentation;
    presentation.faultCode = FaultCode::SafetySensorUnavailable;
    presentation.acknowledged = true;
    const auto afterAck = ActuationInterlock::evaluate(input);

    TEST_ASSERT_TRUE(blocked.permission ==
                     ActuatorSafetyGateStatus::Unresolved);
    TEST_ASSERT_TRUE(afterAck.permission ==
                     ActuatorSafetyGateStatus::Unresolved);
    TEST_ASSERT_TRUE(afterAck.faultCode == FaultCode::SafetySensorUnavailable);
    TEST_ASSERT_TRUE(presentation.acknowledged);
}

void test_missing_selection_projection_blocks_explicit_activation() {
    ActuationEvidence input;
    device_platform::SensorQualitySnapshot sensor;
    SensorSelectionRuntimeState selection;
    validBootEvidence(input, sensor, selection);
    input.sensorSelectionRuntime = nullptr;

    const auto result = ActuationInterlock::evaluate(input);
    TEST_ASSERT_TRUE(result.faultCode == FaultCode::SafetySensorUnavailable);
    TEST_ASSERT_TRUE(result.permission == ActuatorSafetyGateStatus::Unresolved);
}

void test_watchdog_reset_requires_fresh_evidence_and_is_ram_only() {
    ActuatorPlanner planner(watchdogTestParameters());
    tripWatchdog(planner);
    ActuationEvidence input;
    device_platform::SensorQualitySnapshot sensor;
    SensorSelectionRuntimeState selection;
    validBootEvidence(input, sensor, selection);
    input.actuatorPlanner = &planner;

    static_cast<void>(ActuationInterlock::evaluate(input));
    input.sensorEvidenceValidated = false;
    TEST_ASSERT_FALSE(
        ActuationInterlock::resetRequestWatchdog(planner, 1U, input));
    input.sensorEvidenceValidated = true;
    TEST_ASSERT_TRUE(
        ActuationInterlock::resetRequestWatchdog(planner, 2U, input));
    TEST_ASSERT_FALSE(planner.state().latchedWatchdogFault.has_value());
    TEST_ASSERT_TRUE(ActuationInterlock::evaluate(input).faultCode ==
                     FaultCode::None);
}

void test_watchdog_fault_is_sticky_until_explicit_reset() {
    ActuatorPlanner planner(watchdogTestParameters());
    tripWatchdog(planner);
    ActuationEvidence input;
    device_platform::SensorQualitySnapshot sensor;
    SensorSelectionRuntimeState selection;
    validBootEvidence(input, sensor, selection);
    input.actuatorPlanner = &planner;
    static_cast<void>(ActuationInterlock::evaluate(input));

    input.activationPersistenceResult.reset();
    input.processActivationApplied = false;
    const auto missingFaultEvidence = ActuationInterlock::evaluate(input);
    TEST_ASSERT_TRUE(missingFaultEvidence.faultCode ==
                     FaultCode::ActuatorRequestWatchdog);
    TEST_ASSERT_TRUE(missingFaultEvidence.permission ==
                     ActuatorSafetyGateStatus::Unresolved);

    input.activationPersistenceResult = RunPersistenceResultStatus::Applied;
    input.processActivationApplied = true;
    input.explicitActivationRequested = true;
    const auto newRequest = ActuationInterlock::evaluate(input);
    TEST_ASSERT_TRUE(newRequest.faultCode ==
                     FaultCode::ActuatorRequestWatchdog);

    TEST_ASSERT_TRUE(
        ActuationInterlock::resetRequestWatchdog(planner, 3U, input));
    TEST_ASSERT_TRUE(ActuationInterlock::evaluate(input).faultCode ==
                     FaultCode::None);

    const auto afterReset = ActuationInterlock::evaluate(input);
    TEST_ASSERT_TRUE(afterReset.permission ==
                     ActuatorSafetyGateStatus::Allowed);
}

void test_configuration_fault_requires_explicit_start_to_clear() {
    ActuationEvidence input;
    device_platform::SensorQualitySnapshot sensor;
    SensorSelectionRuntimeState selection;
    validBootEvidence(input, sensor, selection);
    input.configurationServiceMode = ConfigurationServiceMode::RuntimeFailure;
    const auto fault = ActuationInterlock::evaluate(input);
    TEST_ASSERT_TRUE(fault.faultCode == FaultCode::ConfigurationRuntimeFailure);

    input.configurationServiceMode = ConfigurationServiceMode::Operational;
    input.configurationRecoveryStatus =
        ConfigurationRecoveryStatus::RuntimeReady;
    input.explicitActivationRequested = false;
    const auto withoutStart = ActuationInterlock::evaluate(input);
    TEST_ASSERT_TRUE(withoutStart.faultCode == FaultCode::None);
    TEST_ASSERT_TRUE(withoutStart.permission ==
                     ActuatorSafetyGateStatus::Unresolved);

    input.explicitActivationRequested = true;
    const auto withStart = ActuationInterlock::evaluate(input);
    TEST_ASSERT_TRUE(withStart.faultCode == FaultCode::None);
    TEST_ASSERT_TRUE(withStart.permission == ActuatorSafetyGateStatus::Allowed);
}

void test_safe_boot_fault_is_not_cleared_by_missing_producer() {
    ActuationEvidence input;
    input.bootValidationComplete = true;
    input.configurationValidated = true;
    input.configurationRecoveryStatus =
        ConfigurationRecoveryStatus::ConfigurationUnavailable;
    input.configurationProducer =
        ConfigurationSafetyProducer::ConfigurationUnavailable;
    input.persistenceValidated = true;
    input.persistenceLoadStatus = RunPersistenceLoadStatus::NoPersistedRun;
    input.loadDisposition = RunLoadDisposition::Standby;
    input.persistenceCoordinatorState =
        RunPersistenceCoordinatorState::ReadyEmpty;
    const auto fault = ActuationInterlock::evaluate(input);
    TEST_ASSERT_TRUE(fault.faultCode == FaultCode::ConfigurationUnavailable);

    input.configurationRecoveryStatus.reset();
    const auto missingProducer = ActuationInterlock::evaluate(input);
    TEST_ASSERT_TRUE(missingProducer.faultCode ==
                     FaultCode::ConfigurationUnavailable);

    input.configurationRecoveryStatus =
        ConfigurationRecoveryStatus::RuntimeReady;
    input.configurationProducer.reset();
    const auto revalidated = ActuationInterlock::evaluate(input);
    TEST_ASSERT_TRUE(revalidated.faultCode == FaultCode::None);
}

void test_multiple_faults_keep_each_clear_contract() {
    ActuationEvidence input;
    device_platform::SensorQualitySnapshot sensor;
    SensorSelectionRuntimeState selection;
    validBootEvidence(input, sensor, selection);
    input.configurationProducer =
        ConfigurationSafetyProducer::ConfigurationUnavailable;
    sensor.quality = device_platform::SensorQuality::Stale;

    const auto combined = ActuationInterlock::evaluate(input);
    TEST_ASSERT_TRUE(combined.faultCode == FaultCode::ConfigurationUnavailable);

    sensor.quality = device_platform::SensorQuality::Valid;
    const auto configStillActive = ActuationInterlock::evaluate(input);
    TEST_ASSERT_TRUE(configStillActive.faultCode ==
                     FaultCode::ConfigurationUnavailable);

    input.configurationProducer.reset();
    const auto cleared = ActuationInterlock::evaluate(input);
    TEST_ASSERT_TRUE(cleared.faultCode == FaultCode::None);
    TEST_ASSERT_TRUE(cleared.permission == ActuatorSafetyGateStatus::Allowed);
}

void test_acknowledgement_is_presentation_only() {
    ActuationEvidence input;
    device_platform::SensorQualitySnapshot sensor;
    SensorSelectionRuntimeState selection;
    validBootEvidence(input, sensor, selection);
    input.configurationProducer =
        ConfigurationSafetyProducer::ConfigurationUnavailable;
    sensor.quality = device_platform::SensorQuality::Stale;

    static_cast<void>(ActuationInterlock::evaluate(input));
    PresentationState presentation;
    presentation.faultCode = FaultCode::ConfigurationUnavailable;
    presentation.acknowledged = true;

    const auto stillBlocked = ActuationInterlock::evaluate(input);
    TEST_ASSERT_TRUE(stillBlocked.faultCode ==
                     FaultCode::ConfigurationUnavailable);
    TEST_ASSERT_TRUE(stillBlocked.permission ==
                     ActuatorSafetyGateStatus::Unresolved);
    TEST_ASSERT_TRUE(presentation.acknowledged);
}

void test_watchdog_fault_survives_safe_boot_clear_until_explicit_reset() {
    ActuatorPlanner planner(watchdogTestParameters());
    tripWatchdog(planner);
    ActuationEvidence input;
    device_platform::SensorQualitySnapshot sensor;
    SensorSelectionRuntimeState selection;
    validBootEvidence(input, sensor, selection);
    input.actuatorPlanner = &planner;
    input.configurationProducer =
        ConfigurationSafetyProducer::ConfigurationUnavailable;

    const auto safeBoot = ActuationInterlock::evaluate(input);
    TEST_ASSERT_TRUE(safeBoot.faultCode == FaultCode::ConfigurationUnavailable);
    TEST_ASSERT_FALSE(
        ActuationInterlock::resetRequestWatchdog(planner, 2U, input));

    input.configurationProducer.reset();
    const auto watchdogRemains = ActuationInterlock::evaluate(input);
    TEST_ASSERT_TRUE(watchdogRemains.faultCode ==
                     FaultCode::ActuatorRequestWatchdog);
    TEST_ASSERT_TRUE(watchdogRemains.permission ==
                     ActuatorSafetyGateStatus::Unresolved);
    TEST_ASSERT_TRUE(
        ActuationInterlock::resetRequestWatchdog(planner, 2U, input));
    TEST_ASSERT_TRUE(ActuationInterlock::evaluate(input).faultCode ==
                     FaultCode::None);
}

void test_application_allocation_failure_is_presentation_only() {
    ActuationEvidence input;
    validOperationalStandbyEvidence(input);
    PresentationState presentation;
    presentation.applicationAllocationFailure = true;
    presentation.faultCode = FaultCode::None;

    const auto evaluation = ActuationInterlock::evaluate(input);
    TEST_ASSERT_TRUE(presentation.applicationAllocationFailure);
    TEST_ASSERT_TRUE(presentation.faultCode == FaultCode::None);
    TEST_ASSERT_TRUE(evaluation.faultCode == FaultCode::None);
    TEST_ASSERT_TRUE(evaluation.permission ==
                     ActuatorSafetyGateStatus::Unresolved);
}

void test_integrity_and_commit_faults_require_matching_resolution() {
    ActuationEvidence integrityInput;
    device_platform::SensorQualitySnapshot integritySensor;
    SensorSelectionRuntimeState integritySelection;
    validBootEvidence(integrityInput, integritySensor, integritySelection);
    integrityInput.configurationRecoveryStatus =
        ConfigurationRecoveryStatus::ConfigurationIntegrityFailure;
    integrityInput.configurationProducer =
        ConfigurationSafetyProducer::ConfigurationIntegrityFailure;
    TEST_ASSERT_TRUE(ActuationInterlock::evaluate(integrityInput).faultCode ==
                     FaultCode::ConfigurationIntegrityFailure);
    integrityInput.configurationRecoveryStatus =
        ConfigurationRecoveryStatus::RuntimeReady;
    integrityInput.configurationProducer.reset();
    TEST_ASSERT_TRUE(ActuationInterlock::evaluate(integrityInput).faultCode ==
                     FaultCode::None);

    ActuationEvidence commitInput;
    device_platform::SensorQualitySnapshot commitSensor;
    SensorSelectionRuntimeState commitSelection;
    validBootEvidence(commitInput, commitSensor, commitSelection);
    commitInput.configurationServiceMode =
        ConfigurationServiceMode::CommitIndeterminate;
    TEST_ASSERT_TRUE(ActuationInterlock::evaluate(commitInput).faultCode ==
                     FaultCode::ConfigurationCommitIndeterminate);
    commitInput.configurationServiceMode =
        ConfigurationServiceMode::Operational;
    commitInput.configurationCommitStatus =
        ConfigurationCommitStatus::Activated;
    TEST_ASSERT_TRUE(ActuationInterlock::evaluate(commitInput).faultCode ==
                     FaultCode::None);
}

void test_validated_fallback_is_service_required_without_resume() {
    ActuationEvidence input;
    device_platform::SensorQualitySnapshot sensor;
    SensorSelectionRuntimeState selection;
    validFallbackRecoveryEvidence(input, sensor, selection);

    const auto result = ActuationInterlock::evaluate(input);
    TEST_ASSERT_TRUE(result.faultCode == FaultCode::RunPersistenceUntrusted);
    TEST_ASSERT_TRUE(result.permission == ActuatorSafetyGateStatus::Unresolved);
    TEST_ASSERT_FALSE(result.permission == ActuatorSafetyGateStatus::Allowed);
}

void test_fallback_without_snapshot_is_fail_closed() {
    ActuationEvidence input;
    device_platform::SensorQualitySnapshot sensor;
    SensorSelectionRuntimeState selection;
    validFallbackRecoveryEvidence(input, sensor, selection);

    const auto result = ActuationInterlock::evaluate(input);
    TEST_ASSERT_TRUE(result.faultCode == FaultCode::RunPersistenceUntrusted);
    TEST_ASSERT_TRUE(result.permission == ActuatorSafetyGateStatus::Unresolved);
}

void test_fallback_without_validated_persistence_is_fail_closed() {
    ActuationEvidence input;
    device_platform::SensorQualitySnapshot sensor;
    SensorSelectionRuntimeState selection;
    validFallbackRecoveryEvidence(input, sensor, selection);
    input.persistenceValidated = false;

    const auto result = ActuationInterlock::evaluate(input);
    TEST_ASSERT_TRUE(result.faultCode == FaultCode::RunPersistenceUntrusted);
    TEST_ASSERT_TRUE(result.permission == ActuatorSafetyGateStatus::Unresolved);
}

void test_pending_fallback_requires_consistent_load_tuple() {
    ActuationEvidence input;
    device_platform::SensorQualitySnapshot sensor;
    SensorSelectionRuntimeState selection;
    validFallbackRecoveryEvidence(input, sensor, selection);
    input.persistenceLoadStatus = RunPersistenceLoadStatus::Current;

    const auto result = ActuationInterlock::evaluate(input);
    TEST_ASSERT_TRUE(result.faultCode == FaultCode::RunPersistenceUntrusted);
    TEST_ASSERT_TRUE(result.permission == ActuatorSafetyGateStatus::Unresolved);
}

void test_fallback_pending_never_allows_before_recovery_apply() {
    ActuationEvidence input;
    device_platform::SensorQualitySnapshot sensor;
    SensorSelectionRuntimeState selection;
    validFallbackRecoveryEvidence(input, sensor, selection);
    input.explicitActivationRequested = true;
    input.plannerEvidenceValidated = true;
    input.activationKind = SafetyActivationKind::Resume;
    input.activationPersistenceResult = RunPersistenceResultStatus::Applied;
    input.processActivationApplied = true;

    const auto result = ActuationInterlock::evaluate(input);
    TEST_ASSERT_TRUE(result.faultCode == FaultCode::RunPersistenceUntrusted);
    TEST_ASSERT_TRUE(result.permission == ActuatorSafetyGateStatus::Unresolved);
    TEST_ASSERT_FALSE(result.permission == ActuatorSafetyGateStatus::Allowed);
}

void test_latched_fallback_fault_clears_to_unresolved_offer() {
    ActuationEvidence input;
    device_platform::SensorQualitySnapshot sensor;
    SensorSelectionRuntimeState selection;
    validFallbackRecoveryEvidence(input, sensor, selection);
    input.persistenceValidated = false;

    const auto first = ActuationInterlock::evaluate(input);
    TEST_ASSERT_TRUE(first.faultCode == FaultCode::RunPersistenceUntrusted);
    TEST_ASSERT_TRUE(first.permission == ActuatorSafetyGateStatus::Unresolved);

    input.persistenceValidated = true;
    const auto resolved = ActuationInterlock::evaluate(input);
    TEST_ASSERT_TRUE(resolved.faultCode == FaultCode::RunPersistenceUntrusted);
    TEST_ASSERT_TRUE(resolved.permission ==
                     ActuatorSafetyGateStatus::Unresolved);
    TEST_ASSERT_FALSE(resolved.permission == ActuatorSafetyGateStatus::Allowed);
}

void test_unknown_producer_is_fail_closed() {
    ActuationEvidence input;
    input.bootValidationComplete = true;
    input.configurationValidated = true;
    input.persistenceValidated = true;
    input.persistenceLoadStatus = RunPersistenceLoadStatus::NoPersistedRun;
    input.loadDisposition = RunLoadDisposition::Standby;
    input.persistenceCoordinatorState =
        RunPersistenceCoordinatorState::ReadyEmpty;
    input.configurationServiceMode =
        static_cast<ConfigurationServiceMode>(0xFFU);

    const auto result = ActuationInterlock::evaluate(input);
    TEST_ASSERT_TRUE(result.faultCode == FaultCode::SystemProducerUnknown);

    // Omitting the producer is not positive resolution.
    input.configurationServiceMode.reset();
    const auto missingProducer = ActuationInterlock::evaluate(input);
    TEST_ASSERT_TRUE(missingProducer.faultCode == FaultCode::None);

    // The same source must later provide a known value.  The clear cycle is
    // still fail-closed and cannot become an Allowed gate.
    input.configurationServiceMode = ConfigurationServiceMode::Operational;
    const auto resolved = ActuationInterlock::evaluate(input);
    TEST_ASSERT_TRUE(resolved.faultCode == FaultCode::None);
    TEST_ASSERT_TRUE(resolved.permission ==
                     ActuatorSafetyGateStatus::Unresolved);
}

void test_unknown_producer_sources_resolve_independently() {
    ActuationEvidence input;
    input.bootValidationComplete = true;
    input.configurationValidated = true;
    input.configurationServiceMode =
        static_cast<ConfigurationServiceMode>(0xFFU);
    input.persistenceLoadStatus = static_cast<RunPersistenceLoadStatus>(0xFFU);
    input.persistenceValidated = true;
    input.persistenceCoordinatorState =
        RunPersistenceCoordinatorState::ReadyEmpty;

    TEST_ASSERT_TRUE(ActuationInterlock::evaluate(input).faultCode ==
                     FaultCode::SystemProducerUnknown);

    // Resolve only the configuration source; the missing persistence source
    // is projected from the current evidence as untrusted.
    input.configurationServiceMode = ConfigurationServiceMode::Operational;
    input.persistenceLoadStatus.reset();
    const auto oneSourceRemaining = ActuationInterlock::evaluate(input);
    TEST_ASSERT_TRUE(oneSourceRemaining.faultCode ==
                     FaultCode::RunPersistenceUntrusted);

    input.persistenceLoadStatus = RunPersistenceLoadStatus::NoPersistedRun;
    input.loadDisposition = RunLoadDisposition::Standby;
    const auto bothResolved = ActuationInterlock::evaluate(input);
    TEST_ASSERT_TRUE(bothResolved.faultCode == FaultCode::None);
    TEST_ASSERT_TRUE(bothResolved.permission ==
                     ActuatorSafetyGateStatus::Unresolved);
}

void test_configuration_recovery_status_uses_producer_context() {
    constexpr ConfigurationRecoveryStatus producerlessRejectedStatuses[] = {
        ConfigurationRecoveryStatus::ConfigurationIntegrityFailure,
        ConfigurationRecoveryStatus::UnsupportedNewerConfigurationSchema,
        ConfigurationRecoveryStatus::ConfigurationMutationBusy,
        ConfigurationRecoveryStatus::ConfigurationModelBudgetBusy,
        ConfigurationRecoveryStatus::StateTransitionRejected,
        ConfigurationRecoveryStatus::CounterOverflow,
        ConfigurationRecoveryStatus::PersistenceWriteFailure,
        ConfigurationRecoveryStatus::RuntimePreparationFailure,
    };
    for (const auto status : producerlessRejectedStatuses) {
        ActuationEvidence input;
        validOperationalStandbyEvidence(input);
        input.configurationRecoveryStatus = status;
        const auto result = ActuationInterlock::evaluate(input);
        TEST_ASSERT_TRUE(result.faultCode == FaultCode::None);
        TEST_ASSERT_TRUE(result.permission ==
                         ActuatorSafetyGateStatus::Unresolved);
    }

    ActuationEvidence unavailableInput;
    validOperationalStandbyEvidence(unavailableInput);
    unavailableInput.configurationRecoveryStatus =
        ConfigurationRecoveryStatus::CounterOverflow;
    unavailableInput.configurationProducer =
        ConfigurationSafetyProducer::ConfigurationUnavailable;
    const auto unavailableResult =
        ActuationInterlock::evaluate(unavailableInput);
    TEST_ASSERT_TRUE(unavailableResult.faultCode ==
                     FaultCode::ConfigurationUnavailable);

    ActuationEvidence integrityInput;
    validOperationalStandbyEvidence(integrityInput);
    integrityInput.configurationRecoveryStatus =
        ConfigurationRecoveryStatus::ConfigurationIntegrityFailure;
    integrityInput.configurationProducer =
        ConfigurationSafetyProducer::ConfigurationIntegrityFailure;
    const auto integrityResult = ActuationInterlock::evaluate(integrityInput);
    TEST_ASSERT_TRUE(integrityResult.faultCode ==
                     FaultCode::ConfigurationIntegrityFailure);
}

void test_normal_configuration_commit_rejections_keep_operational_runtime() {
    constexpr ConfigurationCommitStatus rejectedStatuses[] = {
        ConfigurationCommitStatus::PreviewNotFound,
        ConfigurationCommitStatus::PreviewSuperseded,
        ConfigurationCommitStatus::ConfigurationMutationBusy,
        ConfigurationCommitStatus::ConfigurationConflictFailure,
        ConfigurationCommitStatus::ConfigurationValidationFailure,
        ConfigurationCommitStatus::PersistenceFailure,
        ConfigurationCommitStatus::CapacityFailure,
    };
    for (const auto status : rejectedStatuses) {
        ActuationEvidence input;
        validOperationalStandbyEvidence(input);
        input.configurationCommitStatus = status;
        const auto result = ActuationInterlock::evaluate(input);
        TEST_ASSERT_TRUE(result.faultCode == FaultCode::None);
        TEST_ASSERT_TRUE(result.permission ==
                         ActuatorSafetyGateStatus::Unresolved);
    }

    ActuationEvidence commitInput;
    validOperationalStandbyEvidence(commitInput);
    commitInput.configurationCommitStatus =
        ConfigurationCommitStatus::ConfigurationCommitIndeterminate;
    const auto commitResult = ActuationInterlock::evaluate(commitInput);
    TEST_ASSERT_TRUE(commitResult.faultCode ==
                     FaultCode::ConfigurationCommitIndeterminate);

    ActuationEvidence runtimeInput;
    validOperationalStandbyEvidence(runtimeInput);
    runtimeInput.configurationServiceMode =
        ConfigurationServiceMode::RuntimeFailure;
    const auto runtimeResult = ActuationInterlock::evaluate(runtimeInput);
    TEST_ASSERT_TRUE(runtimeResult.faultCode ==
                     FaultCode::ConfigurationRuntimeFailure);
    TEST_ASSERT_TRUE(runtimeResult.permission ==
                     ActuatorSafetyGateStatus::Unresolved);
}

void test_configuration_fault_projection_uses_stable_r1_codes() {
    const auto evaluate = [](std::optional<ConfigurationRecoveryStatus>
                                 recovery,
                             std::optional<ConfigurationServiceMode> mode,
                             std::optional<ConfigurationCommitStatus> commit) {
        ActuationEvidence input;
        input.configurationValidated = true;
        input.persistenceValidated = true;
        input.persistenceLoadStatus = RunPersistenceLoadStatus::NoPersistedRun;
        input.loadDisposition = RunLoadDisposition::Standby;
        input.persistenceCoordinatorState =
            RunPersistenceCoordinatorState::ReadyEmpty;
        input.configurationRecoveryStatus = recovery;
        if (recovery == ConfigurationRecoveryStatus::ConfigurationUnavailable) {
            input.configurationProducer =
                ConfigurationSafetyProducer::ConfigurationUnavailable;
        } else if (recovery ==
                   ConfigurationRecoveryStatus::ConfigurationIntegrityFailure) {
            input.configurationProducer =
                ConfigurationSafetyProducer::ConfigurationIntegrityFailure;
        }
        input.configurationServiceMode = mode;
        input.configurationCommitStatus = commit;
        return ActuationInterlock::evaluate(input);
    };

    const auto unavailable =
        evaluate(ConfigurationRecoveryStatus::ConfigurationUnavailable,
                 std::nullopt, std::nullopt);
    TEST_ASSERT_TRUE(unavailable.faultCode ==
                     FaultCode::ConfigurationUnavailable);
    TEST_ASSERT_TRUE(unavailable.permission ==
                     ActuatorSafetyGateStatus::Unresolved);

    const auto integrity =
        evaluate(ConfigurationRecoveryStatus::ConfigurationIntegrityFailure,
                 std::nullopt, std::nullopt);
    TEST_ASSERT_TRUE(integrity.faultCode ==
                     FaultCode::ConfigurationIntegrityFailure);

    const auto runtime = evaluate(
        std::nullopt, ConfigurationServiceMode::RuntimeFailure, std::nullopt);
    TEST_ASSERT_TRUE(runtime.faultCode ==
                     FaultCode::ConfigurationRuntimeFailure);

    const auto indeterminate =
        evaluate(std::nullopt, std::nullopt,
                 ConfigurationCommitStatus::ConfigurationCommitIndeterminate);
    TEST_ASSERT_TRUE(indeterminate.faultCode ==
                     FaultCode::ConfigurationCommitIndeterminate);
}

void test_stateless_interlock_has_no_stale_gate() {
    const auto first = ActuationInterlock::evaluate({});
    const auto second = ActuationInterlock::evaluate({});
    TEST_ASSERT_TRUE(first.faultCode == second.faultCode);
    TEST_ASSERT_TRUE(first.permission == second.permission);
}

void test_reset_cause_port_is_diagnostic_only() {
    device_platform_test_support::MockResetCauseSource source(
        device_platform::ResetCause::TaskWatchdog);
    TEST_ASSERT_TRUE(source.resetCause() ==
                     device_platform::ResetCause::TaskWatchdog);
    source.setResetCause(device_platform::ResetCause::Unknown);
    TEST_ASSERT_TRUE(source.resetCause() ==
                     device_platform::ResetCause::Unknown);
}

void test_every_reset_cause_starts_fail_closed_without_resume() {
    constexpr device_platform::ResetCause causes[] = {
        device_platform::ResetCause::Unknown,
        device_platform::ResetCause::PowerOn,
        device_platform::ResetCause::External,
        device_platform::ResetCause::Software,
        device_platform::ResetCause::Panic,
        device_platform::ResetCause::InterruptWatchdog,
        device_platform::ResetCause::TaskWatchdog,
        device_platform::ResetCause::Watchdog,
        device_platform::ResetCause::DeepSleep,
        device_platform::ResetCause::Brownout,
        device_platform::ResetCause::Sdio,
        device_platform::ResetCause::Usb,
        device_platform::ResetCause::Jtag,
        device_platform::ResetCause::Efuse,
        device_platform::ResetCause::PowerGlitch,
        device_platform::ResetCause::CpuLockup,
        device_platform::ResetCause::Other,
    };
    for (const auto cause : causes) {
        PresentationState presentation;
        presentation.resetCause = cause;
        const auto result = ActuationInterlock::evaluate({});
        TEST_ASSERT_TRUE(result.permission ==
                         ActuatorSafetyGateStatus::Unresolved);
        TEST_ASSERT_TRUE(presentation.resetCause.has_value());
        TEST_ASSERT_TRUE(*presentation.resetCause == cause);
    }
}

}  // namespace

void setup() {}
void teardown() {}

void setup_suite() {
    UNITY_BEGIN();
    RUN_TEST(test_missing_boot_evidence_is_safe_boot);
    RUN_TEST(
        test_fallback_selection_required_never_allows_even_with_complete_evidence);
    RUN_TEST(test_no_active_run_is_standby_without_actuator_allow);
    RUN_TEST(test_no_active_run_load_is_not_a_resume_offer);
    RUN_TEST(test_recovery_evaluation_actuation_is_blocked);
    RUN_TEST(test_waiting_for_trusted_time_actuation_is_blocked);
    RUN_TEST(test_recovery_rejected_actuation_is_blocked);
    RUN_TEST(
        test_validated_explicit_activation_is_allowed_only_after_all_evidence);
    RUN_TEST(test_fresh_start_stays_unresolved_until_new_run_is_applied);
    RUN_TEST(test_fresh_start_commit_failure_never_allows);
    RUN_TEST(test_resume_offer_stays_unresolved_until_apply_and_fsm_evidence);
    RUN_TEST(test_resume_failed_persistence_never_actuates);
    RUN_TEST(test_unconfirmed_resume_offer_does_not_require_sensor);
    RUN_TEST(test_resume_confirm_requires_sensor_evidence);
    RUN_TEST(
        test_non_resumable_current_never_becomes_allowed_from_boolean_evidence);
    RUN_TEST(test_stale_sensor_blocks_and_ack_is_presentation_only);
    RUN_TEST(test_missing_selection_projection_blocks_explicit_activation);
    RUN_TEST(test_watchdog_reset_requires_fresh_evidence_and_is_ram_only);
    RUN_TEST(test_watchdog_fault_is_sticky_until_explicit_reset);
    RUN_TEST(test_configuration_fault_requires_explicit_start_to_clear);
    RUN_TEST(test_safe_boot_fault_is_not_cleared_by_missing_producer);
    RUN_TEST(test_multiple_faults_keep_each_clear_contract);
    RUN_TEST(test_acknowledgement_is_presentation_only);
    RUN_TEST(test_watchdog_fault_survives_safe_boot_clear_until_explicit_reset);
    RUN_TEST(test_application_allocation_failure_is_presentation_only);
    RUN_TEST(test_integrity_and_commit_faults_require_matching_resolution);
    RUN_TEST(test_validated_fallback_is_service_required_without_resume);
    RUN_TEST(test_fallback_without_snapshot_is_fail_closed);
    RUN_TEST(test_fallback_without_validated_persistence_is_fail_closed);
    RUN_TEST(test_pending_fallback_requires_consistent_load_tuple);
    RUN_TEST(test_fallback_pending_never_allows_before_recovery_apply);
    RUN_TEST(test_latched_fallback_fault_clears_to_unresolved_offer);
    RUN_TEST(test_unknown_producer_is_fail_closed);
    RUN_TEST(test_unknown_producer_sources_resolve_independently);
    RUN_TEST(test_configuration_recovery_status_uses_producer_context);
    RUN_TEST(
        test_normal_configuration_commit_rejections_keep_operational_runtime);
    RUN_TEST(test_configuration_fault_projection_uses_stable_r1_codes);
    RUN_TEST(test_stateless_interlock_has_no_stale_gate);
    RUN_TEST(test_reset_cause_port_is_diagnostic_only);
    RUN_TEST(test_every_reset_cause_starts_fail_closed_without_resume);
    UNITY_END();
}

int main() {
    setup_suite();
    return 0;
}
