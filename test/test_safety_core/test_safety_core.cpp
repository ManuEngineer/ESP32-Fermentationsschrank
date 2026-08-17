#include <iterator>

#include <unity.h>

#include "actuator_planner.hpp"
#include "mock_reset_cause_source.hpp"
#include "safety_core.hpp"

namespace {

using namespace fermentation;

void validBootEvidence(SafetyCoreInput& input,
                       device_platform::SensorQualitySnapshot& sensor,
                       SensorSelectionRuntimeState& selection) {
    input.bootValidationComplete = true;
    input.configurationValidated = true;
    input.persistenceValidated = true;
    input.sensorEvidenceValidated = true;
    input.explicitActivationRequested = true;
    input.plannerEvidenceValidated = true;
    input.persistenceLoadStatus = RunPersistenceLoadStatus::Current;
    input.persistenceCoordinatorState = RunPersistenceCoordinatorState::Ready;
    sensor.quality = device_platform::SensorQuality::Valid;
    selection.phase = SensorSelectionPhase::NormalProduct;
    selection.permission = SensorPeltierPermission::Allowed;
    input.peltierSensor = &sensor;
    input.sensorSelectionRuntime = &selection;
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

void test_validated_explicit_activation_is_allowed_only_after_all_evidence() {
    SafetyCore safety;
    safety.beginBoot(device_platform::ResetCause::PowerOn);
    SafetyCoreInput input;
    device_platform::SensorQualitySnapshot sensor;
    SensorSelectionRuntimeState selection;
    validBootEvidence(input, sensor, selection);

    const auto result = safety.evaluate(input);

    TEST_ASSERT_TRUE(result.gate.status == ActuatorSafetyGateStatus::Allowed);
    TEST_ASSERT_TRUE(result.bootDisposition ==
                     SafetyBootDisposition::ResumeOffer);
    TEST_ASSERT_TRUE(result.faultCode == FaultCode::None);
}

void test_stale_sensor_blocks_and_ack_does_not_clear_gate() {
    SafetyCore safety;
    SafetyCoreInput input;
    device_platform::SensorQualitySnapshot sensor;
    SensorSelectionRuntimeState selection;
    validBootEvidence(input, sensor, selection);
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
    validBootEvidence(input, sensor, selection);
    input.sensorSelectionRuntime = nullptr;

    const auto result = safety.evaluate(input);
    TEST_ASSERT_TRUE(result.faultCode == FaultCode::SafetySensorUnavailable);
    TEST_ASSERT_TRUE(result.gate.status ==
                     ActuatorSafetyGateStatus::Unresolved);
}

void test_watchdog_reset_requires_fresh_evidence_and_is_ram_only() {
    SafetyCore safety;
    SafetyCoreInput input;
    device_platform::SensorQualitySnapshot sensor;
    SensorSelectionRuntimeState selection;
    validBootEvidence(input, sensor, selection);
    input.requestWatchdogTripped = true;

    static_cast<void>(safety.evaluate(input));
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
    ActuatorPlanner planner(parameters);

    TEST_ASSERT_FALSE(safety.resetRequestWatchdog(planner, 1U, false));
    TEST_ASSERT_TRUE(safety.resetRequestWatchdog(planner, 2U, true));
    TEST_ASSERT_TRUE(safety.activeFault() == FaultCode::None);
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
    input.configurationValidated = true;
    input.persistenceValidated = true;
    input.persistenceLoadStatus = RunPersistenceLoadStatus::Current;
    input.persistenceCoordinatorState = RunPersistenceCoordinatorState::Ready;
    input.configurationServiceMode =
        static_cast<ConfigurationServiceMode>(0xFFU);

    const auto result = safety.evaluate(input);
    TEST_ASSERT_TRUE(result.faultCode == FaultCode::SystemProducerUnknown);
    TEST_ASSERT_TRUE(result.disposition == SafetyDisposition::SafeBoot);
}

void test_configuration_fault_projection_uses_stable_r1_codes() {
    const auto evaluate =
        [](std::optional<ConfigurationRecoveryStatus> recovery,
           std::optional<ConfigurationServiceMode> mode,
           std::optional<ConfigurationCommitStatus> commit) {
            SafetyCore safety;
            safety.beginBoot(device_platform::ResetCause::PowerOn);
            SafetyCoreInput input;
            input.configurationValidated = true;
            input.persistenceValidated = true;
            input.persistenceLoadStatus =
                RunPersistenceLoadStatus::NoPersistedRun;
            input.persistenceCoordinatorState =
                RunPersistenceCoordinatorState::ReadyEmpty;
            input.configurationRecoveryStatus = recovery;
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
    RUN_TEST(
        test_validated_explicit_activation_is_allowed_only_after_all_evidence);
    RUN_TEST(test_stale_sensor_blocks_and_ack_does_not_clear_gate);
    RUN_TEST(test_missing_selection_projection_blocks_explicit_activation);
    RUN_TEST(test_watchdog_reset_requires_fresh_evidence_and_is_ram_only);
    RUN_TEST(test_schema_three_neutral_fields_do_not_block_simple_resume);
    RUN_TEST(test_load_matrix_rejects_fallback_and_untrusted_states);
    RUN_TEST(test_unknown_producer_is_fail_closed);
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
