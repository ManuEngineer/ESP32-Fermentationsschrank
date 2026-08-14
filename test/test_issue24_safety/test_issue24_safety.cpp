#include <unity.h>

#include "actuator_plan_types.hpp"
#include "actuator_planner.hpp"
#include "fault_types.hpp"
#include "restart_episode.hpp"
#include "safety_state_store.hpp"
#include "safety_fault_service.hpp"
#include "simulated_persistent_state_store.hpp"
#include "simulated_reset_controller.hpp"
#include "virtual_time_source.hpp"

namespace {

using namespace fermentation;
using device_platform::ControlledRestartResult;
using device_platform::ResetCause;
using device_platform_test_support::SimulatedPersistentStateStore;

ActuatorPlannerParameters recoveryPlannerParameters() {
    ActuatorPlannerParameters parameters;
    parameters.switchingWindowMillis = 100U;
    parameters.minimumOnMillis = 10U;
    parameters.minimumOffMillis = 10U;
    parameters.polarityDeadTimeMillis = 10U;
    parameters.pulseAccumulatorCapMillis = 100U;
    parameters.counterDirectionConfirmationQuoteThreshold = 0.5;
    parameters.counterDirectionConfirmationDurationMillis = 10U;
    parameters.requestWatchdogMillis = 1000U;
    parameters.outerFanPostRunMillis = 10U;
    parameters.innerFanPostRunMillis = 10U;
    return parameters;
}

void test_fault_classes_codes_and_dominance_are_deterministic() {
    FaultCore faults;
    const auto warning = faults.raise(
        {FaultCode::P1_001, 1U, 1U, 10U, std::nullopt});
    TEST_ASSERT_TRUE(warning.status == FaultRaiseStatus::Created);
    const auto safety = faults.raise(
        {FaultCode::S3_004, 2U, 1U, 20U, std::nullopt});
    TEST_ASSERT_TRUE(safety.status == FaultRaiseStatus::Created);
    TEST_ASSERT_TRUE(faults.dominant() != nullptr);
    TEST_ASSERT_TRUE(faults.dominant()->code == FaultCode::S3_004);
    TEST_ASSERT_TRUE(faults.disposition() == SafetyDisposition::ImmediateStop);
    TEST_ASSERT_TRUE(faults.hasBlockingFault());
    TEST_ASSERT_EQUAL_STRING("Y4-011", faultCodeText(FaultCode::Unknown));
}

void test_fault_correlation_retains_instance_and_primary_is_bounded() {
    FaultCore faults;
    const auto first = faults.raise(
        {FaultCode::S3_001, 4U, 8U, 10U, std::nullopt});
    const auto repeated = faults.raise(
        {FaultCode::S3_001, 4U, 8U, 11U, std::nullopt});
    TEST_ASSERT_TRUE(repeated.status == FaultRaiseStatus::Existing);
    TEST_ASSERT_TRUE(repeated.instanceId == first.instanceId);
    const auto followUp = faults.raise(
        {FaultCode::S3_006, 5U, 9U, 12U, first.instanceId});
    TEST_ASSERT_TRUE(followUp.status == FaultRaiseStatus::Created);
    TEST_ASSERT_TRUE(faults.find(followUp.instanceId)->primaryFaultId.has_value());
    TEST_ASSERT_TRUE(*faults.find(followUp.instanceId)->primaryFaultId ==
                     first.instanceId);
}

void test_safety_record_round_trip_and_factory_not_found_gate() {
    SimulatedPersistentStateStore store;
    SafetyStateStore safetyStore(store);
    const auto blocked = safetyStore.load();
    TEST_ASSERT_TRUE(blocked.status ==
                     SafetyRecordLoadStatus::NotFoundOutsideFactoryBootstrap);

    const auto initialized = safetyStore.load({true, true, true});
    TEST_ASSERT_TRUE(initialized.status == SafetyRecordLoadStatus::FactoryInitialized);
    TEST_ASSERT_EQUAL_UINT32(1U, initialized.record.recordRevision);

    SafetyStateRecord changed = initialized.record;
    changed.recordRevision = 2U;
    changed.faultRevision = 1U;
    changed.faultInstanceSequence = 1U;
    changed.latchCount = 1U;
    changed.latches[0].instanceId = {1U};
    changed.latches[0].code = FaultCode::S3_001;
    changed.latches[0].faultClass = FaultClass::LatchedSafetyFault;
    changed.latches[0].faultRevision = 1U;
    changed.latches[0].latched = true;
    changed.latches[0].causeActive = true;
    TEST_ASSERT_TRUE(safetyStore.commit(changed).status ==
                     SafetyRecordCommitStatus::Committed);
    const auto loaded = safetyStore.load();
    TEST_ASSERT_TRUE(loaded.status == SafetyRecordLoadStatus::Loaded);
    TEST_ASSERT_EQUAL_UINT32(2U, loaded.record.recordRevision);
    TEST_ASSERT_EQUAL_UINT32(1U, loaded.record.latches[0].instanceId.value);
}

void test_restart_episode_counts_once_and_closes_after_stable_window() {
    SafetyStateRecord record;
    RestartEpisodeCoordinator episode;
    device_platform::ResetCauseSnapshot watchdog{
        ResetCause::WatchdogOrPanic, true, 1U};
    const auto first = episode.evaluateBoot(record, watchdog);
    TEST_ASSERT_TRUE(first.status == RestartBootStatus::AbnormalRecorded);
    TEST_ASSERT_EQUAL_UINT32(1U, record.restartEpisode.abnormalRestartCount);
    const auto second = episode.evaluateBoot(record, {ResetCause::Brownout, true, 2U});
    TEST_ASSERT_TRUE(second.status == RestartBootStatus::AbnormalRecorded);
    const auto third = episode.evaluateBoot(record, {ResetCause::WatchdogOrPanic, true, 3U});
    TEST_ASSERT_TRUE(third.status == RestartBootStatus::SafeBootRequired);
    TEST_ASSERT_TRUE(record.safeBootRequired);

    record.safeBootRequired = false;
    TEST_ASSERT_FALSE(episode.advanceStableWindow(record, 0U, true));
    TEST_ASSERT_FALSE(episode.advanceStableWindow(record, 30U * 60U * 1000U - 1U,
                                                  true));
    TEST_ASSERT_TRUE(episode.advanceStableWindow(record, 30U * 60U * 1000U,
                                                  true));
    TEST_ASSERT_FALSE(record.restartEpisode.open);
}

void test_reset_port_is_stable_and_has_no_fault_semantics() {
    device_platform_test_support::SimulatedResetController reset;
    reset.setBootReset(ResetCause::Brownout, true, 42U);
    const auto first = reset.observeBootReset();
    const auto second = reset.observeBootReset();
    TEST_ASSERT_EQUAL_UINT32(first.observationId, second.observationId);
    TEST_ASSERT_TRUE(reset.requestRestart({}) == ControlledRestartResult::Accepted);
    TEST_ASSERT_EQUAL_UINT32(1U, reset.restartRequestCount());
}

void test_safety_recovery_requires_every_qualified_precondition() {
    SafetyRecoveryRequest request;
    request.targetFault = {1U};
    request.triggeringDirection = AbstractControlDirection::Heating;
    request.recoveryDirection = AbstractControlDirection::Cooling;
    request.faultRevision = 1U;
    request.safetyEvidenceRevision = 1U;
    request.attemptIndex = 1U;
    request.maxAttempts = 2U;
    request.sequence = 1U;
    request.timeQuote = 0.5;
    request.qualifiedSensorEvidence = 1U;
    request.qualifiedFanEvidence = 1U;
    request.qualifiedActuatorEvidence = 1U;
    request.hardLimitNotReached = true;
    request.noSensorConflict = true;
    request.triggeringDirectionOff = true;
    request.safeCurrentWhenAvailable = true;
    request.minimumOffTimeElapsed = true;
    request.polarityDeadTimeElapsed = true;
    request.safetyRecoveryParametersRevision = 1U;
    request.createdAtMonotonicMillis = 1U;
    TEST_ASSERT_TRUE(request.structurallyValid());
    request.noSensorConflict = false;
    TEST_ASSERT_FALSE(request.structurallyValid());
}

void test_safety_service_persists_latch_projects_gate_and_requires_reset() {
    SimulatedPersistentStateStore store;
    device_platform_test_support::SimulatedResetController reset;
    device_platform::VirtualTimeSource time;
    SafetyFaultService service(store, reset, time);
    TEST_ASSERT_TRUE(service.begin({true, true, true}) ==
                     SafetyServiceStatus::Ready);
    TEST_ASSERT_TRUE(service.consumeConfigurationStatus(
                         ConfigurationSafetyStatus::ConfigurationUnavailable,
                         56U, 1U) == SafetyServiceStatus::Ready);
    TEST_ASSERT_TRUE(service.safeBootRequired());
    TEST_ASSERT_TRUE(service.disposition() == SafetyDisposition::ImmediateStop);
    RunCommandState projected;
    service.projectTo(projected);
    TEST_ASSERT_TRUE(projected.criticalSafetyEventPending);

    const auto* fault = service.faultCore().dominant();
    TEST_ASSERT_TRUE(fault != nullptr);
    TEST_ASSERT_TRUE(service.clearFaultCause(fault->instanceId,
                                             fault->faultRevision) ==
                     SafetyServiceStatus::Ready);
    TEST_ASSERT_TRUE(service.resetFault(fault->instanceId,
                                        service.faultCore().find(fault->instanceId)
                         ->faultRevision,
                                        false)
                         .status == SafetyServiceStatus::SafetyRejected);
}

void test_authorized_reset_is_write_before_apply_and_consumes_once() {
    SimulatedPersistentStateStore store;
    device_platform_test_support::SimulatedResetController reset;
    device_platform::VirtualTimeSource time;
    SafetyFaultService service(store, reset, time);
    TEST_ASSERT_TRUE(service.begin({true, true, true}) ==
                     SafetyServiceStatus::Ready);
    TEST_ASSERT_TRUE(service.raiseFault(
                         {FaultCode::S3_004, 24U, 1U, 1U, std::nullopt}) ==
                     SafetyServiceStatus::Ready);
    const auto* raised = service.faultCore().dominant();
    TEST_ASSERT_TRUE(raised != nullptr);
    const FaultInstanceId id = raised->instanceId;
    const std::uint32_t raisedRevision = raised->faultRevision;
    TEST_ASSERT_TRUE(service.clearFaultCause(id, raisedRevision) ==
                     SafetyServiceStatus::Ready);
    const auto* cleared = service.faultCore().find(id);
    TEST_ASSERT_TRUE(cleared != nullptr);
    const auto resetResult = service.resetFault(id, cleared->faultRevision, true);
    TEST_ASSERT_TRUE(resetResult.status == SafetyServiceStatus::ResetCommitted);
    TEST_ASSERT_TRUE(reset.restartRequestCount() == 1U);
    TEST_ASSERT_TRUE(service.faultCore().find(id)->status == FaultStatus::Cleared);

    reset.setBootReset(ResetCause::AuthorizedRestart, true, 2U);
    const auto boot = service.evaluateBoot();
    TEST_ASSERT_TRUE(boot.status == SafetyServiceStatus::Ready);
    TEST_ASSERT_FALSE(boot.safeBootRequired);
    TEST_ASSERT_FALSE(service.record().faultResetBootIntent.pending);
    const auto secondBoot = service.evaluateBoot();
    TEST_ASSERT_TRUE(secondBoot.status == SafetyServiceStatus::Ready);
    TEST_ASSERT_EQUAL_UINT32(1U, reset.restartRequestCount());
}

void test_controlled_restart_and_recovery_gate_share_canonical_fault() {
    SimulatedPersistentStateStore store;
    device_platform_test_support::SimulatedResetController reset;
    device_platform::VirtualTimeSource time;
    SafetyFaultService service(store, reset, time);
    TEST_ASSERT_TRUE(service.begin({true, true, true}) ==
                     SafetyServiceStatus::Ready);
    TEST_ASSERT_TRUE(service.raiseFault(
                         {FaultCode::S3_004, 24U, 2U, 1U, std::nullopt}) ==
                     SafetyServiceStatus::Ready);
    const auto* fault = service.faultCore().dominant();
    TEST_ASSERT_TRUE(fault != nullptr);

    SafetyRecoveryRequest recovery;
    recovery.targetFault = fault->instanceId;
    recovery.triggeringDirection = AbstractControlDirection::Heating;
    recovery.recoveryDirection = AbstractControlDirection::Cooling;
    recovery.faultRevision = fault->faultRevision;
    recovery.safetyEvidenceRevision = 1U;
    recovery.attemptIndex = 1U;
    recovery.maxAttempts = 2U;
    recovery.sequence = 1U;
    recovery.createdAtMonotonicMillis = 1U;
    recovery.timeQuote = 0.5;
    recovery.qualifiedSensorEvidence = 1U;
    recovery.qualifiedFanEvidence = 1U;
    recovery.qualifiedActuatorEvidence = 1U;
    recovery.hardLimitNotReached = true;
    recovery.noSensorConflict = true;
    recovery.triggeringDirectionOff = true;
    recovery.safeCurrentWhenAvailable = true;
    recovery.minimumOffTimeElapsed = true;
    recovery.polarityDeadTimeElapsed = true;
    recovery.safetyRecoveryParametersRevision = 1U;
    const auto gate = service.actuatorGateInput(recovery);
    TEST_ASSERT_TRUE(gate.status == ActuatorSafetyGateStatus::SafetyRecovery);

    ActuatorPlanner planner(recoveryPlannerParameters());
    const auto plan = planner.tick(
        {10U, std::nullopt, recovery.contextAtQualification, true, gate});
    TEST_ASSERT_TRUE(plan.status == ActuatorPlanStatus::Active);
    TEST_ASSERT_TRUE(plan.reason == ActuatorPlanReason::SafetyRecovery);

    TEST_ASSERT_TRUE(service.requestControlledSafetyRestart(
                         fault->instanceId, fault->faultRevision) ==
                     SafetyServiceStatus::Ready);
    TEST_ASSERT_TRUE(reset.lastPurpose() ==
                     device_platform::ControlledRestartPurpose::ControlledSafetyRestart);
    reset.setBootReset(ResetCause::ControlledSafetyRestart, true, 3U);
    const auto boot = service.evaluateBoot();
    TEST_ASSERT_TRUE(boot.status == SafetyServiceStatus::Ready);
    TEST_ASSERT_FALSE(boot.safeBootRequired);
    TEST_ASSERT_EQUAL_UINT32(1U,
                             service.record().restartEpisode.abnormalRestartCount);
    TEST_ASSERT_TRUE(service.record().restartEvidence.state ==
                     RestartEvidenceState::Consumed);
}

void test_unknown_boot_reset_creates_system_fault_and_safe_boots() {
    SimulatedPersistentStateStore store;
    device_platform_test_support::SimulatedResetController reset;
    device_platform::VirtualTimeSource time;
    SafetyFaultService service(store, reset, time);
    TEST_ASSERT_TRUE(service.begin({true, true, true}) ==
                     SafetyServiceStatus::Ready);
    reset.setBootReset(ResetCause::Unknown, true, 4U);
    const auto boot = service.evaluateBoot();
    TEST_ASSERT_TRUE(boot.status == SafetyServiceStatus::SafetyRejected);
    TEST_ASSERT_TRUE(boot.safeBootRequired);
    TEST_ASSERT_TRUE(service.faultCore().dominant() != nullptr);
    TEST_ASSERT_TRUE(service.faultCore().dominant()->code == FaultCode::Y4_006);
    TEST_ASSERT_TRUE(service.safeBootRequired());
}

}  // namespace

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_fault_classes_codes_and_dominance_are_deterministic);
    RUN_TEST(test_fault_correlation_retains_instance_and_primary_is_bounded);
    RUN_TEST(test_safety_record_round_trip_and_factory_not_found_gate);
    RUN_TEST(test_restart_episode_counts_once_and_closes_after_stable_window);
    RUN_TEST(test_reset_port_is_stable_and_has_no_fault_semantics);
    RUN_TEST(test_safety_recovery_requires_every_qualified_precondition);
    RUN_TEST(test_safety_service_persists_latch_projects_gate_and_requires_reset);
    RUN_TEST(test_authorized_reset_is_write_before_apply_and_consumes_once);
    RUN_TEST(test_controlled_restart_and_recovery_gate_share_canonical_fault);
    RUN_TEST(test_unknown_boot_reset_creates_system_fault_and_safe_boots);
    return UNITY_END();
}
