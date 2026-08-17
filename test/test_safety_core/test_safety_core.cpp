#include <iterator>

#include <unity.h>

#include "actuator_planner.hpp"
#include "mock_reset_cause_source.hpp"
#include "safety_core.hpp"

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

void validBootEvidence(SafetyCoreInput& input,
                       device_platform::SensorQualitySnapshot& sensor,
                       SensorSelectionRuntimeState& selection,
                       RunPersistenceSnapshot& snapshot) {
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
    snapshot.variant = RunCheckpointVariant::ProgramRun;
    snapshot.processState.state = ProcessState::Preheating;
    input.persistenceSnapshot = &snapshot;
    input.persistenceCoordinatorState = RunPersistenceCoordinatorState::Ready;
    input.activationPersistenceResult = RunPersistenceResultStatus::Applied;
    input.processActivationApplied = true;
    sensor.quality = device_platform::SensorQuality::Valid;
    selection.phase = SensorSelectionPhase::NormalProduct;
    selection.permission = SensorPeltierPermission::Allowed;
    input.peltierSensor = &sensor;
    input.sensorSelectionRuntime = &selection;
}

void validOperationalStandbyEvidence(SafetyCoreInput& input) {
    input.bootValidationComplete = true;
    input.configurationValidated = true;
    input.configurationRecoveryStatus =
        ConfigurationRecoveryStatus::RuntimeReady;
    input.configurationServiceMode = ConfigurationServiceMode::Operational;
    input.persistenceValidated = true;
    input.persistenceLoadStatus = RunPersistenceLoadStatus::NoPersistedRun;
    input.persistenceCoordinatorState =
        RunPersistenceCoordinatorState::ReadyEmpty;
}

void test_missing_boot_evidence_is_safe_boot() {
    SafetyCore safety;
    safety.beginBoot(device_platform::ResetCause::PowerOn);

    const auto result = safety.evaluate({});

    TEST_ASSERT_TRUE(result.gate.status ==
                     ActuatorSafetyGateStatus::Unresolved);
    TEST_ASSERT_TRUE(result.disposition == SafetyDisposition::SafeBoot);
    TEST_ASSERT_TRUE(result.bootDisposition == SafetyBootDisposition::SafeBoot);
    TEST_ASSERT_TRUE(result.faultCode == FaultCode::SystemProducerUnknown);
}

void test_no_active_run_is_standby_without_actuator_allow() {
    SafetyCore safety;
    safety.beginBoot(device_platform::ResetCause::Brownout);
    SafetyCoreInput input;
    input.configurationValidated = true;
    input.persistenceValidated = true;
    input.persistenceLoadStatus = RunPersistenceLoadStatus::NoPersistedRun;
    input.persistenceCoordinatorState =
        RunPersistenceCoordinatorState::ReadyEmpty;

    const auto result = safety.evaluate(input);

    TEST_ASSERT_TRUE(result.bootDisposition == SafetyBootDisposition::Standby);
    TEST_ASSERT_TRUE(result.gate.status ==
                     ActuatorSafetyGateStatus::Unresolved);
    TEST_ASSERT_TRUE(result.faultCode == FaultCode::None);
}

void test_no_active_run_load_is_not_a_resume_offer() {
    SafetyCore safety;
    safety.beginBoot(device_platform::ResetCause::External);
    SafetyCoreInput input;
    input.configurationValidated = true;
    input.persistenceValidated = true;
    input.persistenceLoadStatus = RunPersistenceLoadStatus::NoActiveRun;
    input.persistenceCoordinatorState =
        RunPersistenceCoordinatorState::ReadyEmpty;

    const auto result = safety.evaluate(input);

    TEST_ASSERT_TRUE(result.bootDisposition == SafetyBootDisposition::Standby);
    TEST_ASSERT_TRUE(result.gate.status ==
                     ActuatorSafetyGateStatus::Unresolved);
    TEST_ASSERT_TRUE(result.faultCode == FaultCode::None);
}

void test_validated_explicit_activation_is_allowed_only_after_all_evidence() {
    SafetyCore safety;
    safety.beginBoot(device_platform::ResetCause::PowerOn);
    SafetyCoreInput input;
    device_platform::SensorQualitySnapshot sensor;
    SensorSelectionRuntimeState selection;
    RunPersistenceSnapshot snapshot;
    validBootEvidence(input, sensor, selection, snapshot);

    const auto result = safety.evaluate(input);

    TEST_ASSERT_TRUE(result.gate.status == ActuatorSafetyGateStatus::Allowed);
    TEST_ASSERT_TRUE(result.bootDisposition ==
                     SafetyBootDisposition::ResumeOffer);
    TEST_ASSERT_TRUE(result.faultCode == FaultCode::None);
}

void test_fresh_start_stays_unresolved_until_new_run_is_applied() {
    SafetyCore safety;
    safety.beginBoot(device_platform::ResetCause::PowerOn);
    SafetyCoreInput input;
    device_platform::SensorQualitySnapshot sensor;
    SensorSelectionRuntimeState selection;
    input.bootValidationComplete = true;
    input.configurationValidated = true;
    input.configurationRecoveryStatus =
        ConfigurationRecoveryStatus::RuntimeReady;
    input.persistenceValidated = true;
    input.persistenceLoadStatus = RunPersistenceLoadStatus::NoActiveRun;
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

    const auto beforeCommit = safety.evaluate(input);
    TEST_ASSERT_TRUE(beforeCommit.bootDisposition ==
                     SafetyBootDisposition::Standby);
    TEST_ASSERT_TRUE(beforeCommit.gate.status ==
                     ActuatorSafetyGateStatus::Unresolved);
    TEST_ASSERT_TRUE(beforeCommit.bootDisposition !=
                     SafetyBootDisposition::ResumeOffer);

    input.activationPersistenceResult = RunPersistenceResultStatus::Applied;
    input.processActivationApplied = true;
    const auto afterCommit = safety.evaluate(input);
    TEST_ASSERT_TRUE(afterCommit.bootDisposition ==
                     SafetyBootDisposition::Standby);
    TEST_ASSERT_TRUE(afterCommit.gate.status ==
                     ActuatorSafetyGateStatus::Allowed);
}

void test_fresh_start_commit_failure_never_allows() {
    SafetyCore safety;
    safety.beginBoot(device_platform::ResetCause::PowerOn);
    SafetyCoreInput input;
    device_platform::SensorQualitySnapshot sensor;
    SensorSelectionRuntimeState selection;
    input.bootValidationComplete = true;
    input.configurationValidated = true;
    input.configurationRecoveryStatus =
        ConfigurationRecoveryStatus::RuntimeReady;
    input.persistenceValidated = true;
    input.persistenceLoadStatus = RunPersistenceLoadStatus::NoPersistedRun;
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

    const auto result = safety.evaluate(input);
    TEST_ASSERT_TRUE(result.gate.status ==
                     ActuatorSafetyGateStatus::Unresolved);
    TEST_ASSERT_TRUE(result.bootDisposition !=
                     SafetyBootDisposition::ResumeOffer);
}

void test_resume_offer_stays_unresolved_until_apply_and_fsm_evidence() {
    SafetyCore safety;
    safety.beginBoot(device_platform::ResetCause::PowerOn);
    SafetyCoreInput input;
    device_platform::SensorQualitySnapshot sensor;
    SensorSelectionRuntimeState selection;
    RunPersistenceSnapshot snapshot;
    validBootEvidence(input, sensor, selection, snapshot);
    input.activationPersistenceResult.reset();
    input.processActivationApplied = false;

    const auto result = safety.evaluate(input);

    TEST_ASSERT_TRUE(result.bootDisposition ==
                     SafetyBootDisposition::ResumeOffer);
    TEST_ASSERT_TRUE(result.gate.status ==
                     ActuatorSafetyGateStatus::Unresolved);

    input.activationPersistenceResult = RunPersistenceResultStatus::Applied;
    const auto beforeFsm = safety.evaluate(input);
    TEST_ASSERT_TRUE(beforeFsm.gate.status ==
                     ActuatorSafetyGateStatus::Unresolved);

    input.processActivationApplied = true;
    const auto afterFsm = safety.evaluate(input);
    TEST_ASSERT_TRUE(afterFsm.gate.status == ActuatorSafetyGateStatus::Allowed);
}

void test_non_resumable_current_never_becomes_allowed_from_boolean_evidence() {
    SafetyCore safety;
    safety.beginBoot(device_platform::ResetCause::PowerOn);
    SafetyCoreInput input;
    device_platform::SensorQualitySnapshot sensor;
    SensorSelectionRuntimeState selection;
    RunPersistenceSnapshot snapshot;
    validBootEvidence(input, sensor, selection, snapshot);
    snapshot.processState.state = ProcessState::Fermenting;

    const auto result = safety.evaluate(input);

    TEST_ASSERT_TRUE(result.bootDisposition ==
                     SafetyBootDisposition::NoActiveRun);
    TEST_ASSERT_TRUE(result.gate.status ==
                     ActuatorSafetyGateStatus::Unresolved);
    TEST_ASSERT_TRUE(result.faultCode == FaultCode::None);
}

void test_stale_sensor_blocks_and_ack_does_not_clear_gate() {
    SafetyCore safety;
    SafetyCoreInput input;
    device_platform::SensorQualitySnapshot sensor;
    SensorSelectionRuntimeState selection;
    RunPersistenceSnapshot snapshot;
    validBootEvidence(input, sensor, selection, snapshot);
    sensor.quality = device_platform::SensorQuality::Stale;

    const auto blocked = safety.evaluate(input);
    safety.acknowledge(FaultCode::SafetySensorUnavailable);
    const auto afterAck = safety.evaluate(input);

    TEST_ASSERT_TRUE(blocked.gate.status ==
                     ActuatorSafetyGateStatus::Unresolved);
    TEST_ASSERT_TRUE(blocked.disposition ==
                     SafetyDisposition::BlockedImmediateStop);
    TEST_ASSERT_TRUE(afterAck.gate.status ==
                     ActuatorSafetyGateStatus::Unresolved);
    TEST_ASSERT_TRUE(afterAck.faultCode == FaultCode::SafetySensorUnavailable);
    TEST_ASSERT_TRUE(afterAck.acknowledged);
}

void test_missing_selection_projection_blocks_explicit_activation() {
    SafetyCore safety;
    SafetyCoreInput input;
    device_platform::SensorQualitySnapshot sensor;
    SensorSelectionRuntimeState selection;
    RunPersistenceSnapshot snapshot;
    validBootEvidence(input, sensor, selection, snapshot);
    input.sensorSelectionRuntime = nullptr;

    const auto result = safety.evaluate(input);
    TEST_ASSERT_TRUE(result.faultCode == FaultCode::SafetySensorUnavailable);
    TEST_ASSERT_TRUE(result.gate.status ==
                     ActuatorSafetyGateStatus::Unresolved);
}

void test_watchdog_reset_requires_fresh_evidence_and_is_ram_only() {
    SafetyCore safety;
    ActuatorPlanner planner(watchdogTestParameters());
    tripWatchdog(planner);
    SafetyCoreInput input;
    device_platform::SensorQualitySnapshot sensor;
    SensorSelectionRuntimeState selection;
    RunPersistenceSnapshot snapshot;
    validBootEvidence(input, sensor, selection, snapshot);
    input.actuatorPlanner = &planner;

    static_cast<void>(safety.evaluate(input));
    input.sensorEvidenceValidated = false;
    TEST_ASSERT_FALSE(safety.resetRequestWatchdog(planner, 1U, input));
    input.sensorEvidenceValidated = true;
    TEST_ASSERT_TRUE(safety.resetRequestWatchdog(planner, 2U, input));
    TEST_ASSERT_FALSE(planner.state().latchedWatchdogFault.has_value());
    TEST_ASSERT_TRUE(safety.activeFault() == FaultCode::None);
}

void test_watchdog_fault_is_sticky_until_explicit_reset() {
    SafetyCore safety;
    ActuatorPlanner planner(watchdogTestParameters());
    tripWatchdog(planner);
    SafetyCoreInput input;
    device_platform::SensorQualitySnapshot sensor;
    SensorSelectionRuntimeState selection;
    RunPersistenceSnapshot snapshot;
    validBootEvidence(input, sensor, selection, snapshot);
    input.actuatorPlanner = &planner;
    static_cast<void>(safety.evaluate(input));
    safety.acknowledge(FaultCode::ActuatorRequestWatchdog);

    input.activationPersistenceResult.reset();
    input.processActivationApplied = false;
    const auto missingFaultEvidence = safety.evaluate(input);
    TEST_ASSERT_TRUE(missingFaultEvidence.faultCode ==
                     FaultCode::ActuatorRequestWatchdog);
    TEST_ASSERT_TRUE(missingFaultEvidence.acknowledged);
    TEST_ASSERT_TRUE(missingFaultEvidence.gate.status ==
                     ActuatorSafetyGateStatus::Unresolved);

    input.activationPersistenceResult = RunPersistenceResultStatus::Applied;
    input.processActivationApplied = true;
    input.explicitActivationRequested = true;
    const auto newRequest = safety.evaluate(input);
    TEST_ASSERT_TRUE(newRequest.faultCode ==
                     FaultCode::ActuatorRequestWatchdog);

    TEST_ASSERT_TRUE(safety.resetRequestWatchdog(planner, 3U, input));
    TEST_ASSERT_TRUE(safety.activeFault() == FaultCode::None);
    TEST_ASSERT_TRUE(safety.lastEvaluation().faultCode == FaultCode::None);
    TEST_ASSERT_FALSE(safety.lastEvaluation().acknowledged);
    TEST_ASSERT_TRUE(safety.lastEvaluation().gate.status ==
                     ActuatorSafetyGateStatus::Unresolved);

    const auto afterReset = safety.evaluate(input);
    TEST_ASSERT_TRUE(afterReset.gate.status ==
                     ActuatorSafetyGateStatus::Allowed);
}

void test_configuration_fault_requires_explicit_start_to_clear() {
    SafetyCore safety;
    SafetyCoreInput input;
    device_platform::SensorQualitySnapshot sensor;
    SensorSelectionRuntimeState selection;
    RunPersistenceSnapshot snapshot;
    validBootEvidence(input, sensor, selection, snapshot);
    input.configurationServiceMode = ConfigurationServiceMode::RuntimeFailure;
    const auto fault = safety.evaluate(input);
    TEST_ASSERT_TRUE(fault.faultCode == FaultCode::ConfigurationRuntimeFailure);

    input.configurationServiceMode = ConfigurationServiceMode::Operational;
    input.configurationRecoveryStatus =
        ConfigurationRecoveryStatus::RuntimeReady;
    input.explicitActivationRequested = false;
    const auto withoutStart = safety.evaluate(input);
    TEST_ASSERT_TRUE(withoutStart.faultCode ==
                     FaultCode::ConfigurationRuntimeFailure);

    input.explicitActivationRequested = true;
    const auto withStart = safety.evaluate(input);
    TEST_ASSERT_TRUE(withStart.faultCode == FaultCode::None);
    TEST_ASSERT_TRUE(withStart.gate.status ==
                     ActuatorSafetyGateStatus::Unresolved);
    TEST_ASSERT_FALSE(safety.isAcknowledged());

    const auto afterClear = safety.evaluate(input);
    TEST_ASSERT_TRUE(afterClear.gate.status ==
                     ActuatorSafetyGateStatus::Allowed);
}

void test_safe_boot_fault_is_not_cleared_by_missing_producer() {
    SafetyCore safety;
    SafetyCoreInput input;
    input.bootValidationComplete = true;
    input.configurationValidated = true;
    input.configurationRecoveryStatus =
        ConfigurationRecoveryStatus::ConfigurationUnavailable;
    input.configurationProducer =
        ConfigurationSafetyProducer::ConfigurationUnavailable;
    input.persistenceValidated = true;
    input.persistenceLoadStatus = RunPersistenceLoadStatus::NoPersistedRun;
    input.persistenceCoordinatorState =
        RunPersistenceCoordinatorState::ReadyEmpty;
    const auto fault = safety.evaluate(input);
    TEST_ASSERT_TRUE(fault.faultCode == FaultCode::ConfigurationUnavailable);

    input.configurationRecoveryStatus.reset();
    const auto missingProducer = safety.evaluate(input);
    TEST_ASSERT_TRUE(missingProducer.faultCode ==
                     FaultCode::ConfigurationUnavailable);

    input.configurationRecoveryStatus =
        ConfigurationRecoveryStatus::RuntimeReady;
    input.configurationProducer.reset();
    const auto revalidated = safety.evaluate(input);
    TEST_ASSERT_TRUE(revalidated.faultCode == FaultCode::None);
    TEST_ASSERT_TRUE(revalidated.bootDisposition ==
                     SafetyBootDisposition::Standby);
    TEST_ASSERT_FALSE(safety.isAcknowledged());
}

void test_multiple_faults_keep_each_clear_contract() {
    SafetyCore safety;
    SafetyCoreInput input;
    device_platform::SensorQualitySnapshot sensor;
    SensorSelectionRuntimeState selection;
    RunPersistenceSnapshot snapshot;
    validBootEvidence(input, sensor, selection, snapshot);
    input.configurationProducer =
        ConfigurationSafetyProducer::ConfigurationUnavailable;
    sensor.quality = device_platform::SensorQuality::Stale;

    const auto combined = safety.evaluate(input);
    TEST_ASSERT_TRUE(combined.faultCode == FaultCode::ConfigurationUnavailable);
    TEST_ASSERT_TRUE(combined.disposition == SafetyDisposition::SafeBoot);

    sensor.quality = device_platform::SensorQuality::Valid;
    const auto configStillActive = safety.evaluate(input);
    TEST_ASSERT_TRUE(configStillActive.faultCode ==
                     FaultCode::ConfigurationUnavailable);

    input.configurationProducer.reset();
    const auto cleared = safety.evaluate(input);
    TEST_ASSERT_TRUE(cleared.faultCode == FaultCode::None);
    TEST_ASSERT_TRUE(cleared.gate.status ==
                     ActuatorSafetyGateStatus::Unresolved);
    const auto afterClear = safety.evaluate(input);
    TEST_ASSERT_TRUE(afterClear.gate.status ==
                     ActuatorSafetyGateStatus::Allowed);
}

void test_acknowledgement_is_scoped_to_each_fault_code() {
    SafetyCore safety;
    SafetyCoreInput input;
    device_platform::SensorQualitySnapshot sensor;
    SensorSelectionRuntimeState selection;
    RunPersistenceSnapshot snapshot;
    validBootEvidence(input, sensor, selection, snapshot);
    input.configurationProducer =
        ConfigurationSafetyProducer::ConfigurationUnavailable;
    sensor.quality = device_platform::SensorQuality::Stale;

    static_cast<void>(safety.evaluate(input));
    safety.acknowledge(FaultCode::ConfigurationUnavailable);
    TEST_ASSERT_TRUE(
        safety.isAcknowledged(FaultCode::ConfigurationUnavailable));
    TEST_ASSERT_FALSE(
        safety.isAcknowledged(FaultCode::SafetySensorUnavailable));

    const auto stillBlocked = safety.evaluate(input);
    TEST_ASSERT_TRUE(stillBlocked.faultCode ==
                     FaultCode::ConfigurationUnavailable);
    TEST_ASSERT_TRUE(stillBlocked.gate.status ==
                     ActuatorSafetyGateStatus::Unresolved);
}

void test_watchdog_fault_survives_safe_boot_clear_until_explicit_reset() {
    SafetyCore safety;
    ActuatorPlanner planner(watchdogTestParameters());
    tripWatchdog(planner);
    SafetyCoreInput input;
    device_platform::SensorQualitySnapshot sensor;
    SensorSelectionRuntimeState selection;
    RunPersistenceSnapshot snapshot;
    validBootEvidence(input, sensor, selection, snapshot);
    input.actuatorPlanner = &planner;
    input.configurationProducer =
        ConfigurationSafetyProducer::ConfigurationUnavailable;

    const auto safeBoot = safety.evaluate(input);
    TEST_ASSERT_TRUE(safeBoot.faultCode == FaultCode::ConfigurationUnavailable);

    input.configurationProducer.reset();
    const auto watchdogRemains = safety.evaluate(input);
    TEST_ASSERT_TRUE(watchdogRemains.faultCode ==
                     FaultCode::ActuatorRequestWatchdog);
    TEST_ASSERT_TRUE(watchdogRemains.gate.status ==
                     ActuatorSafetyGateStatus::Unresolved);
    TEST_ASSERT_TRUE(safety.resetRequestWatchdog(planner, 2U, input));
    TEST_ASSERT_TRUE(safety.activeFault() == FaultCode::None);
}

void test_integrity_and_commit_faults_require_matching_resolution() {
    SafetyCore integrity;
    SafetyCoreInput integrityInput;
    device_platform::SensorQualitySnapshot integritySensor;
    SensorSelectionRuntimeState integritySelection;
    RunPersistenceSnapshot integritySnapshot;
    validBootEvidence(integrityInput, integritySensor, integritySelection,
                      integritySnapshot);
    integrityInput.configurationRecoveryStatus =
        ConfigurationRecoveryStatus::ConfigurationIntegrityFailure;
    integrityInput.configurationProducer =
        ConfigurationSafetyProducer::ConfigurationIntegrityFailure;
    TEST_ASSERT_TRUE(integrity.evaluate(integrityInput).faultCode ==
                     FaultCode::ConfigurationIntegrityFailure);
    integrityInput.configurationRecoveryStatus =
        ConfigurationRecoveryStatus::RuntimeReady;
    integrityInput.configurationProducer.reset();
    TEST_ASSERT_TRUE(integrity.evaluate(integrityInput).faultCode ==
                     FaultCode::None);

    SafetyCore commit;
    SafetyCoreInput commitInput;
    device_platform::SensorQualitySnapshot commitSensor;
    SensorSelectionRuntimeState commitSelection;
    RunPersistenceSnapshot commitSnapshot;
    validBootEvidence(commitInput, commitSensor, commitSelection,
                      commitSnapshot);
    commitInput.configurationServiceMode =
        ConfigurationServiceMode::CommitIndeterminate;
    TEST_ASSERT_TRUE(commit.evaluate(commitInput).faultCode ==
                     FaultCode::ConfigurationCommitIndeterminate);
    commitInput.configurationServiceMode =
        ConfigurationServiceMode::Operational;
    commitInput.configurationCommitStatus =
        ConfigurationCommitStatus::Activated;
    TEST_ASSERT_TRUE(commit.evaluate(commitInput).faultCode == FaultCode::None);
}

void test_schema_three_neutral_fields_do_not_block_simple_resume() {
    RunPersistenceSnapshot snapshot;
    snapshot.variant = RunCheckpointVariant::ProgramRun;
    snapshot.processState.state = ProcessState::Preheating;
    TEST_ASSERT_TRUE(SafetyCore::isR1ResumeEligible(snapshot));
    TEST_ASSERT_TRUE(SafetyCore::classifyRunLoad(
                         RunPersistenceLoadStatus::Current, &snapshot) ==
                     RunLoadDisposition::ResumeOffer);

    snapshot.processState.state = ProcessState::Fermenting;
    TEST_ASSERT_FALSE(SafetyCore::isR1ResumeEligible(snapshot));
    TEST_ASSERT_TRUE(SafetyCore::classifyRunLoad(
                         RunPersistenceLoadStatus::Current, &snapshot) ==
                     RunLoadDisposition::NoActiveRun);

    snapshot.processState.state = ProcessState::Preheating;
    snapshot.pendingRecoveryAnchor = PendingRecoveryAnchor{};
    TEST_ASSERT_FALSE(SafetyCore::isR1ResumeEligible(snapshot));
}

void test_load_matrix_rejects_fallback_and_untrusted_states() {
    TEST_ASSERT_TRUE(
        SafetyCore::classifyRunLoad(RunPersistenceLoadStatus::FallbackRecovered,
                                    nullptr) == RunLoadDisposition::SafeBoot);
    TEST_ASSERT_TRUE(SafetyCore::classifyRunLoad(
                         RunPersistenceLoadStatus::PreparedInterrupted,
                         nullptr) == RunLoadDisposition::SafeBoot);
    TEST_ASSERT_TRUE(SafetyCore::classifyRunLoad(
                         RunPersistenceLoadStatus::NoActiveRun, nullptr) ==
                     RunLoadDisposition::Standby);
}

void test_unknown_producer_is_fail_closed() {
    SafetyCore safety;
    SafetyCoreInput input;
    input.bootValidationComplete = true;
    input.configurationValidated = true;
    input.persistenceValidated = true;
    input.persistenceLoadStatus = RunPersistenceLoadStatus::NoPersistedRun;
    input.persistenceCoordinatorState =
        RunPersistenceCoordinatorState::ReadyEmpty;
    input.configurationServiceMode =
        static_cast<ConfigurationServiceMode>(0xFFU);

    const auto result = safety.evaluate(input);
    TEST_ASSERT_TRUE(result.faultCode == FaultCode::SystemProducerUnknown);
    TEST_ASSERT_TRUE(result.disposition == SafetyDisposition::SafeBoot);

    // Omitting the producer is not positive resolution.
    input.configurationServiceMode.reset();
    const auto missingProducer = safety.evaluate(input);
    TEST_ASSERT_TRUE(missingProducer.faultCode ==
                     FaultCode::SystemProducerUnknown);
    safety.acknowledge(FaultCode::SystemProducerUnknown);
    TEST_ASSERT_TRUE(safety.isAcknowledged(FaultCode::SystemProducerUnknown));

    // The same source must later provide a known value.  The clear cycle is
    // still fail-closed and cannot become an Allowed gate.
    input.configurationServiceMode = ConfigurationServiceMode::Operational;
    const auto resolved = safety.evaluate(input);
    TEST_ASSERT_TRUE(resolved.faultCode == FaultCode::None);
    TEST_ASSERT_TRUE(resolved.gate.status ==
                     ActuatorSafetyGateStatus::Unresolved);
}

void test_unknown_producer_sources_resolve_independently() {
    SafetyCore safety;
    SafetyCoreInput input;
    input.bootValidationComplete = true;
    input.configurationValidated = true;
    input.configurationServiceMode =
        static_cast<ConfigurationServiceMode>(0xFFU);
    input.persistenceLoadStatus = static_cast<RunPersistenceLoadStatus>(0xFFU);
    input.persistenceValidated = true;
    input.persistenceCoordinatorState =
        RunPersistenceCoordinatorState::ReadyEmpty;

    TEST_ASSERT_TRUE(safety.evaluate(input).faultCode ==
                     FaultCode::SystemProducerUnknown);

    // Resolve only the configuration source; the omitted persistence source
    // remains unresolved and keeps the bounded SystemProducerUnknown fault.
    input.configurationServiceMode = ConfigurationServiceMode::Operational;
    input.persistenceLoadStatus.reset();
    const auto oneSourceRemaining = safety.evaluate(input);
    TEST_ASSERT_TRUE(oneSourceRemaining.faultCode ==
                     FaultCode::SystemProducerUnknown);

    input.persistenceLoadStatus = RunPersistenceLoadStatus::NoPersistedRun;
    const auto bothResolved = safety.evaluate(input);
    TEST_ASSERT_TRUE(bothResolved.faultCode == FaultCode::None);
    TEST_ASSERT_TRUE(bothResolved.gate.status ==
                     ActuatorSafetyGateStatus::Unresolved);
}

void test_configuration_recovery_status_requires_canonical_producer() {
    constexpr ConfigurationRecoveryStatus rejectedStatuses[] = {
        ConfigurationRecoveryStatus::ConfigurationMutationBusy,
        ConfigurationRecoveryStatus::ConfigurationModelBudgetBusy,
        ConfigurationRecoveryStatus::StateTransitionRejected,
        ConfigurationRecoveryStatus::CounterOverflow,
        ConfigurationRecoveryStatus::PersistenceWriteFailure,
        ConfigurationRecoveryStatus::RuntimePreparationFailure,
    };
    for (const auto status : rejectedStatuses) {
        SafetyCore safety;
        SafetyCoreInput input;
        validOperationalStandbyEvidence(input);
        input.configurationRecoveryStatus = status;
        const auto result = safety.evaluate(input);
        TEST_ASSERT_TRUE(result.faultCode == FaultCode::None);
        TEST_ASSERT_TRUE(result.bootDisposition ==
                         SafetyBootDisposition::Standby);
        TEST_ASSERT_TRUE(result.gate.status ==
                         ActuatorSafetyGateStatus::Unresolved);
    }

    SafetyCore unavailable;
    SafetyCoreInput unavailableInput;
    validOperationalStandbyEvidence(unavailableInput);
    unavailableInput.configurationRecoveryStatus =
        ConfigurationRecoveryStatus::CounterOverflow;
    unavailableInput.configurationProducer =
        ConfigurationSafetyProducer::ConfigurationUnavailable;
    const auto unavailableResult = unavailable.evaluate(unavailableInput);
    TEST_ASSERT_TRUE(unavailableResult.faultCode ==
                     FaultCode::ConfigurationUnavailable);
    TEST_ASSERT_TRUE(unavailableResult.bootDisposition ==
                     SafetyBootDisposition::SafeBoot);

    SafetyCore integrity;
    SafetyCoreInput integrityInput;
    validOperationalStandbyEvidence(integrityInput);
    integrityInput.configurationRecoveryStatus =
        ConfigurationRecoveryStatus::ConfigurationIntegrityFailure;
    integrityInput.configurationProducer =
        ConfigurationSafetyProducer::ConfigurationIntegrityFailure;
    const auto integrityResult = integrity.evaluate(integrityInput);
    TEST_ASSERT_TRUE(integrityResult.faultCode ==
                     FaultCode::ConfigurationIntegrityFailure);
    TEST_ASSERT_TRUE(integrityResult.bootDisposition ==
                     SafetyBootDisposition::SafeBoot);

    SafetyCore contradictory;
    SafetyCoreInput contradictoryInput;
    validOperationalStandbyEvidence(contradictoryInput);
    contradictoryInput.configurationRecoveryStatus =
        ConfigurationRecoveryStatus::ConfigurationIntegrityFailure;
    const auto contradictoryResult = contradictory.evaluate(contradictoryInput);
    TEST_ASSERT_TRUE(contradictoryResult.faultCode ==
                     FaultCode::SystemProducerUnknown);
    TEST_ASSERT_TRUE(contradictoryResult.bootDisposition ==
                     SafetyBootDisposition::SafeBoot);
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
        SafetyCore safety;
        SafetyCoreInput input;
        validOperationalStandbyEvidence(input);
        input.configurationCommitStatus = status;
        const auto result = safety.evaluate(input);
        TEST_ASSERT_TRUE(result.faultCode == FaultCode::None);
        TEST_ASSERT_TRUE(result.bootDisposition ==
                         SafetyBootDisposition::Standby);
        TEST_ASSERT_TRUE(result.gate.status ==
                         ActuatorSafetyGateStatus::Unresolved);
    }

    SafetyCore commit;
    SafetyCoreInput commitInput;
    validOperationalStandbyEvidence(commitInput);
    commitInput.configurationCommitStatus =
        ConfigurationCommitStatus::ConfigurationCommitIndeterminate;
    const auto commitResult = commit.evaluate(commitInput);
    TEST_ASSERT_TRUE(commitResult.faultCode ==
                     FaultCode::ConfigurationCommitIndeterminate);
    TEST_ASSERT_TRUE(commitResult.bootDisposition ==
                     SafetyBootDisposition::SafeBoot);

    SafetyCore runtime;
    SafetyCoreInput runtimeInput;
    validOperationalStandbyEvidence(runtimeInput);
    runtimeInput.configurationServiceMode =
        ConfigurationServiceMode::RuntimeFailure;
    const auto runtimeResult = runtime.evaluate(runtimeInput);
    TEST_ASSERT_TRUE(runtimeResult.faultCode ==
                     FaultCode::ConfigurationRuntimeFailure);
    TEST_ASSERT_TRUE(runtimeResult.disposition ==
                     SafetyDisposition::BlockedImmediateStop);
    TEST_ASSERT_TRUE(runtimeResult.gate.status ==
                     ActuatorSafetyGateStatus::Unresolved);
}

void test_configuration_fault_projection_uses_stable_r1_codes() {
    const auto evaluate = [](std::optional<ConfigurationRecoveryStatus>
                                 recovery,
                             std::optional<ConfigurationServiceMode> mode,
                             std::optional<ConfigurationCommitStatus> commit) {
        SafetyCore safety;
        safety.beginBoot(device_platform::ResetCause::PowerOn);
        SafetyCoreInput input;
        input.configurationValidated = true;
        input.persistenceValidated = true;
        input.persistenceLoadStatus = RunPersistenceLoadStatus::NoPersistedRun;
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
        return safety.evaluate(input);
    };

    const auto unavailable =
        evaluate(ConfigurationRecoveryStatus::ConfigurationUnavailable,
                 std::nullopt, std::nullopt);
    TEST_ASSERT_TRUE(unavailable.faultCode ==
                     FaultCode::ConfigurationUnavailable);
    TEST_ASSERT_TRUE(unavailable.gate.status ==
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
    TEST_ASSERT_TRUE(runtime.disposition ==
                     SafetyDisposition::BlockedImmediateStop);

    const auto indeterminate =
        evaluate(std::nullopt, std::nullopt,
                 ConfigurationCommitStatus::ConfigurationCommitIndeterminate);
    TEST_ASSERT_TRUE(indeterminate.faultCode ==
                     FaultCode::ConfigurationCommitIndeterminate);
}

void test_all_technical_load_statuses_are_safe_boot() {
    constexpr RunPersistenceLoadStatus technicalStatuses[] = {
        RunPersistenceLoadStatus::FallbackRecovered,
        RunPersistenceLoadStatus::PreparedInterrupted,
        RunPersistenceLoadStatus::NotReconstructible,
        RunPersistenceLoadStatus::NotReconstructibleOrphanedState,
        RunPersistenceLoadStatus::ReadFailed,
        RunPersistenceLoadStatus::CapacityExceeded,
        RunPersistenceLoadStatus::UnsupportedSchema,
        RunPersistenceLoadStatus::ForeignEpoch,
        RunPersistenceLoadStatus::AlreadyInitialized,
    };
    for (const auto status : technicalStatuses) {
        TEST_ASSERT_TRUE(SafetyCore::classifyRunLoad(status, nullptr) ==
                         RunLoadDisposition::SafeBoot);
    }
}

void test_resume_phase_matrix_is_explicit() {
    constexpr ProcessState states[] = {
        ProcessState::Preheating,     ProcessState::WaitingForProduct,
        ProcessState::ReachingTarget, ProcessState::QualifyingTarget,
        ProcessState::Fermenting,     ProcessState::Cooling,
        ProcessState::CoolHolding,    ProcessState::ManualHolding,
    };
    constexpr bool eligible[] = {true,  false, false, false,
                                 false, true,  false, true};
    for (std::size_t index = 0U; index < std::size(states); ++index) {
        RunPersistenceSnapshot snapshot;
        snapshot.variant = RunCheckpointVariant::ProgramRun;
        snapshot.processState.state = states[index];
        TEST_ASSERT_TRUE(SafetyCore::isR1ResumeEligible(snapshot) ==
                         eligible[index]);
    }
}

void test_last_evaluation_preserves_fail_closed_gate_for_composition_root() {
    SafetyCore safety;
    safety.beginBoot(device_platform::ResetCause::Unknown);
    const auto result = safety.evaluate({});
    TEST_ASSERT_TRUE(safety.lastEvaluation().faultCode == result.faultCode);
    TEST_ASSERT_TRUE(safety.lastEvaluation().gate.status ==
                     ActuatorSafetyGateStatus::Unresolved);
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
        SafetyCore safety;
        safety.beginBoot(cause);
        const auto result = safety.evaluate({});
        TEST_ASSERT_TRUE(result.gate.status ==
                         ActuatorSafetyGateStatus::Unresolved);
        TEST_ASSERT_TRUE(result.bootDisposition ==
                         SafetyBootDisposition::SafeBoot);
        TEST_ASSERT_TRUE(result.resetCause == cause);
    }
}

}  // namespace

void setup() {}
void teardown() {}

void setup_suite() {
    UNITY_BEGIN();
    RUN_TEST(test_missing_boot_evidence_is_safe_boot);
    RUN_TEST(test_no_active_run_is_standby_without_actuator_allow);
    RUN_TEST(test_no_active_run_load_is_not_a_resume_offer);
    RUN_TEST(
        test_validated_explicit_activation_is_allowed_only_after_all_evidence);
    RUN_TEST(test_fresh_start_stays_unresolved_until_new_run_is_applied);
    RUN_TEST(test_fresh_start_commit_failure_never_allows);
    RUN_TEST(test_resume_offer_stays_unresolved_until_apply_and_fsm_evidence);
    RUN_TEST(
        test_non_resumable_current_never_becomes_allowed_from_boolean_evidence);
    RUN_TEST(test_stale_sensor_blocks_and_ack_does_not_clear_gate);
    RUN_TEST(test_missing_selection_projection_blocks_explicit_activation);
    RUN_TEST(test_watchdog_reset_requires_fresh_evidence_and_is_ram_only);
    RUN_TEST(test_watchdog_fault_is_sticky_until_explicit_reset);
    RUN_TEST(test_configuration_fault_requires_explicit_start_to_clear);
    RUN_TEST(test_safe_boot_fault_is_not_cleared_by_missing_producer);
    RUN_TEST(test_multiple_faults_keep_each_clear_contract);
    RUN_TEST(test_acknowledgement_is_scoped_to_each_fault_code);
    RUN_TEST(test_watchdog_fault_survives_safe_boot_clear_until_explicit_reset);
    RUN_TEST(test_integrity_and_commit_faults_require_matching_resolution);
    RUN_TEST(test_schema_three_neutral_fields_do_not_block_simple_resume);
    RUN_TEST(test_load_matrix_rejects_fallback_and_untrusted_states);
    RUN_TEST(test_unknown_producer_is_fail_closed);
    RUN_TEST(test_unknown_producer_sources_resolve_independently);
    RUN_TEST(test_configuration_recovery_status_requires_canonical_producer);
    RUN_TEST(
        test_normal_configuration_commit_rejections_keep_operational_runtime);
    RUN_TEST(test_configuration_fault_projection_uses_stable_r1_codes);
    RUN_TEST(test_all_technical_load_statuses_are_safe_boot);
    RUN_TEST(test_resume_phase_matrix_is_explicit);
    RUN_TEST(
        test_last_evaluation_preserves_fail_closed_gate_for_composition_root);
    RUN_TEST(test_reset_cause_port_is_diagnostic_only);
    RUN_TEST(test_every_reset_cause_starts_fail_closed_without_resume);
    UNITY_END();
}

int main() {
    setup_suite();
    return 0;
}
