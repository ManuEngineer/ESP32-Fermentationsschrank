#include <unity.h>

#include <string>
#include <memory>
#include <limits>
#include <vector>

#include "actuator_plan_types.hpp"
#include "actuator_planner.hpp"
#include "configuration_safety_integration_gate.hpp"
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

class RecordingJournal final : public device_platform::IEventJournal {
   public:
    bool record(std::uint64_t, const std::string& message) override {
        messages.push_back(message);
        return !fail;
    }

    bool fail{false};
    std::vector<std::string> messages;
};

class SafetyTestTimeZoneResolver final
    : public device_platform::ITimeZoneResolver {
   public:
    device_platform::TimeZonePrepareResult prepare(
        const std::string& identifier) const override {
        if (identifier != "Europe/Zurich") {
            return {
                device_platform::TimeZonePrepareStatus::UnsupportedIdentifier,
                std::nullopt};
        }
        return {device_platform::TimeZonePrepareStatus::Success,
                device_platform::PreparedTimeZone{identifier}};
    }
};

struct ConfigurationSafetyFixture {
    SimulatedPersistentStateStore store;
    SafetyTestTimeZoneResolver resolver;
    ConfigurationMutationCoordinator coordinator;
    ConfigurationBootstrapStore bootstrap{store};
    ConfigurationGraphStore graph{store, resolver};
    ConfigurationService configuration{coordinator, graph, resolver};
    std::unique_ptr<ConfigurationRecoveryService> recovery =
        ConfigurationRecoveryService::create(store, bootstrap, graph,
                                             configuration, coordinator);
};

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

SafetyRecoveryQualification validRecoveryQualification(FaultInstanceId id,
                                                       std::uint32_t revision) {
    SafetyRecoveryQualification qualification;
    qualification.targetFault = id;
    qualification.triggeringDirection = AbstractControlDirection::Heating;
    qualification.recoveryDirection = AbstractControlDirection::Cooling;
    qualification.faultRevision = revision;
    qualification.safetyEvidenceRevision = 1U;
    qualification.attemptIndex = 1U;
    qualification.maxAttempts = 2U;
    qualification.sequence = 1U;
    qualification.createdAtMonotonicMillis = 1U;
    qualification.timeQuote = 0.5;
    qualification.contextAtQualification.controlSensorRole =
        ControlSensorRole::Air;
    qualification.qualifiedSensorEvidence = 1U;
    qualification.qualifiedFanEvidence = 1U;
    qualification.qualifiedActuatorEvidence = 1U;
    qualification.hardLimit = SafetyRecoveryCheck::Passed;
    qualification.sensorConflict = SafetyRecoveryCheck::Passed;
    qualification.triggeringDirectionOff = SafetyRecoveryCheck::Passed;
    qualification.safeCurrentWhenAvailable = SafetyRecoveryCheck::Passed;
    qualification.minimumOffTimeElapsed = SafetyRecoveryCheck::Passed;
    qualification.polarityDeadTimeElapsed = SafetyRecoveryCheck::Passed;
    qualification.safetyRecoveryParametersRevision = 1U;
    return qualification;
}

void test_fault_classes_codes_and_dominance_are_deterministic() {
    FaultCore faults;
    const auto warning =
        faults.raise({FaultCode::P1_001, 1U, 1U, 10U, std::nullopt});
    TEST_ASSERT_TRUE(warning.status == FaultRaiseStatus::Created);
    const auto safety =
        faults.raise({FaultCode::S3_004, 2U, 1U, 20U, std::nullopt});
    TEST_ASSERT_TRUE(safety.status == FaultRaiseStatus::Created);
    TEST_ASSERT_TRUE(faults.dominant() != nullptr);
    TEST_ASSERT_TRUE(faults.dominant()->code == FaultCode::S3_004);
    TEST_ASSERT_TRUE(faults.disposition() == SafetyDisposition::ImmediateStop);
    TEST_ASSERT_TRUE(faults.hasBlockingFault());
    TEST_ASSERT_EQUAL_STRING("Y4-011", faultCodeText(FaultCode::Unknown));
}

void test_fault_matrix_covers_codes_priority_capacity_and_overflow() {
    const FaultCode codes[] = {
        FaultCode::P1_001, FaultCode::P1_002, FaultCode::P1_003,
        FaultCode::O2_001, FaultCode::O2_002, FaultCode::O2_003,
        FaultCode::O2_004, FaultCode::S3_001, FaultCode::S3_002,
        FaultCode::S3_003, FaultCode::S3_004, FaultCode::S3_005,
        FaultCode::S3_006, FaultCode::S3_007, FaultCode::S3_008,
        FaultCode::S3_009, FaultCode::Y4_001, FaultCode::Y4_002,
        FaultCode::Y4_003, FaultCode::Y4_004, FaultCode::Y4_005,
        FaultCode::Y4_006, FaultCode::Y4_007, FaultCode::Y4_008,
        FaultCode::Y4_009, FaultCode::Y4_011};
    for (const auto code : codes) {
        TEST_ASSERT_TRUE(isKnownFaultCode(code));
        TEST_ASSERT_TRUE(isKnownFaultClass(faultClassForCode(code)));
    }

    FaultCore priority;
    TEST_ASSERT_TRUE(
        priority.raise({FaultCode::S3_004, 1U, 1U, 1U, std::nullopt}).status ==
        FaultRaiseStatus::Created);
    TEST_ASSERT_TRUE(
        priority.raise({FaultCode::S3_001, 1U, 2U, 2U, std::nullopt}).status ==
        FaultRaiseStatus::Created);
    TEST_ASSERT_TRUE(priority.dominant()->code == FaultCode::S3_001);
    FaultCore unknown;
    TEST_ASSERT_TRUE(
        unknown.raise({FaultCode::Unknown, 1U, 1U, 1U, std::nullopt}).status ==
        FaultRaiseStatus::Created);
    TEST_ASSERT_TRUE(unknown.dominant()->code == FaultCode::Y4_011);

    FaultCore capacity;
    for (std::uint32_t index = 0U; index < kMaximumActiveFaults; ++index) {
        TEST_ASSERT_TRUE(capacity
                             .raise({FaultCode::S3_001, index + 1U, index + 1U,
                                     index + 1U, std::nullopt})
                             .status == FaultRaiseStatus::Created);
    }
    TEST_ASSERT_TRUE(
        capacity.raise({FaultCode::S3_002, 99U, 99U, 99U, std::nullopt})
            .status == FaultRaiseStatus::CapacityReached);

    FaultCore revisionOverflow;
    FaultCoreSnapshot revisionSnapshot;
    revisionSnapshot.revision = std::numeric_limits<std::uint32_t>::max();
    TEST_ASSERT_TRUE(revisionOverflow.restoreSnapshot(revisionSnapshot));
    TEST_ASSERT_TRUE(
        revisionOverflow.raise({FaultCode::S3_001, 1U, 1U, 1U, std::nullopt})
            .status == FaultRaiseStatus::RevisionOverflow);

    FaultCore idOverflow;
    FaultCoreSnapshot idSnapshot;
    idSnapshot.instanceSequenceHighWatermark =
        std::numeric_limits<std::uint32_t>::max() - 1U;
    TEST_ASSERT_TRUE(idOverflow.restoreSnapshot(idSnapshot));
    TEST_ASSERT_TRUE(
        idOverflow.raise({FaultCode::S3_001, 1U, 1U, 1U, std::nullopt})
            .status == FaultRaiseStatus::RevisionOverflow);
}

void test_fault_correlation_retains_instance_and_primary_is_bounded() {
    FaultCore faults;
    const auto first =
        faults.raise({FaultCode::S3_001, 4U, 8U, 10U, std::nullopt});
    const auto repeated =
        faults.raise({FaultCode::S3_001, 4U, 8U, 11U, std::nullopt});
    TEST_ASSERT_TRUE(repeated.status == FaultRaiseStatus::Existing);
    TEST_ASSERT_TRUE(repeated.instanceId == first.instanceId);
    const auto followUp =
        faults.raise({FaultCode::S3_006, 5U, 9U, 12U, first.instanceId});
    TEST_ASSERT_TRUE(followUp.status == FaultRaiseStatus::Created);
    TEST_ASSERT_TRUE(
        faults.find(followUp.instanceId)->primaryFaultId.has_value());
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
    TEST_ASSERT_TRUE(initialized.status ==
                     SafetyRecordLoadStatus::FactoryInitialized);
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
    changed.latches[0].diagnosticSequenceHighWatermark = (1ULL << 40U) + 17U;
    TEST_ASSERT_TRUE(safetyStore.commit(changed).status ==
                     SafetyRecordCommitStatus::Committed);
    const auto loaded = safetyStore.load();
    TEST_ASSERT_TRUE(loaded.status == SafetyRecordLoadStatus::Loaded);
    TEST_ASSERT_EQUAL_UINT32(2U, loaded.record.recordRevision);
    TEST_ASSERT_EQUAL_UINT32(1U, loaded.record.latches[0].instanceId.value);
    TEST_ASSERT_EQUAL_UINT64(
        (1ULL << 40U) + 17U,
        loaded.record.latches[0].diagnosticSequenceHighWatermark);
}

void test_restart_episode_counts_once_and_closes_after_stable_window() {
    SafetyStateRecord record;
    RestartEpisodeCoordinator episode;
    device_platform::ResetCauseSnapshot watchdog{ResetCause::WatchdogOrPanic,
                                                 true, 1U};
    const auto first = episode.evaluateBoot(record, watchdog);
    TEST_ASSERT_TRUE(first.status == RestartBootStatus::AbnormalRecorded);
    TEST_ASSERT_EQUAL_UINT32(1U, record.restartEpisode.abnormalRestartCount);
    const auto repeated = episode.evaluateBoot(record, watchdog);
    TEST_ASSERT_TRUE(repeated.status == RestartBootStatus::AbnormalRecorded);
    TEST_ASSERT_FALSE(repeated.recordNeedsCommit);
    TEST_ASSERT_EQUAL_UINT32(1U, record.restartEpisode.abnormalRestartCount);
    const auto second =
        episode.evaluateBoot(record, {ResetCause::Brownout, true, 2U});
    TEST_ASSERT_TRUE(second.status == RestartBootStatus::AbnormalRecorded);
    const auto third =
        episode.evaluateBoot(record, {ResetCause::WatchdogOrPanic, true, 3U});
    TEST_ASSERT_TRUE(third.status == RestartBootStatus::SafeBootRequired);
    TEST_ASSERT_TRUE(record.safeBootRequired);

    record.safeBootRequired = false;
    TEST_ASSERT_FALSE(episode.advanceStableWindow(record, 0U, true));
    TEST_ASSERT_FALSE(
        episode.advanceStableWindow(record, 30U * 60U * 1000U - 1U, true));
    TEST_ASSERT_TRUE(
        episode.advanceStableWindow(record, 30U * 60U * 1000U, true));
    TEST_ASSERT_FALSE(record.restartEpisode.open);
}

void test_reset_port_is_stable_and_has_no_fault_semantics() {
    device_platform_test_support::SimulatedResetController reset;
    reset.setBootReset(ResetCause::Brownout, true, 42U);
    const auto first = reset.observeBootReset();
    const auto second = reset.observeBootReset();
    TEST_ASSERT_EQUAL_UINT32(first.observationId, second.observationId);
    TEST_ASSERT_TRUE(reset.requestRestart({}) ==
                     ControlledRestartResult::Accepted);
    TEST_ASSERT_EQUAL_UINT32(1U, reset.restartRequestCount());
}

void test_safety_recovery_requires_every_qualified_precondition() {
    SimulatedPersistentStateStore store;
    device_platform_test_support::SimulatedResetController reset;
    device_platform::VirtualTimeSource time;
    SafetyFaultService service(store, reset, time);
    TEST_ASSERT_TRUE(service.begin({true, true, true}) ==
                     SafetyServiceStatus::Ready);
    TEST_ASSERT_TRUE(
        service.raiseFault({FaultCode::S3_004, 24U, 1U, 1U, std::nullopt}) ==
        SafetyServiceStatus::Ready);
    const auto* fault = service.faultCore().dominant();
    TEST_ASSERT_NOT_NULL(fault);
    auto qualification =
        validRecoveryQualification(fault->instanceId, fault->faultRevision);
    qualification.sensorConflict = SafetyRecoveryCheck::Failed;
    TEST_ASSERT_FALSE(service.issueSafetyRecovery(qualification).has_value());
    qualification.sensorConflict = SafetyRecoveryCheck::Passed;
    const auto capability = service.issueSafetyRecovery(qualification);
    TEST_ASSERT_TRUE(capability.has_value());
    TEST_ASSERT_TRUE(capability->structurallyValid());
    ActuatorSafetyGateInput forged{ActuatorSafetyGateStatus::SafetyRecovery};
    forged.safetyRecovery = *capability;
    TEST_ASSERT_FALSE(forged.hasRecoveryAuthority());
    TEST_ASSERT_TRUE(
        service.actuatorGateInput(*capability).hasRecoveryAuthority());
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
    TEST_ASSERT_TRUE(
        service.clearFaultCause(fault->instanceId, fault->faultRevision) ==
        SafetyServiceStatus::Ready);
    TEST_ASSERT_TRUE(
        service
            .resetFault(
                fault->instanceId,
                service.faultCore().find(fault->instanceId)->faultRevision,
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
    TEST_ASSERT_TRUE(
        service.raiseFault({FaultCode::S3_004, 24U, 1U, 1U, std::nullopt}) ==
        SafetyServiceStatus::Ready);
    const auto* raised = service.faultCore().dominant();
    TEST_ASSERT_TRUE(raised != nullptr);
    const FaultInstanceId id = raised->instanceId;
    const std::uint32_t raisedRevision = raised->faultRevision;
    TEST_ASSERT_TRUE(service.clearFaultCause(id, raisedRevision) ==
                     SafetyServiceStatus::Ready);
    const auto* cleared = service.faultCore().find(id);
    TEST_ASSERT_TRUE(cleared != nullptr);
    TEST_ASSERT_TRUE(service.consumeConfigurationStatus(
                         ConfigurationSafetyStatus::Operational, 56U, 1U) ==
                     SafetyServiceStatus::Ready);
    const auto resetResult = service.resetFault(id, cleared->faultRevision);
    TEST_ASSERT_TRUE(resetResult.status == SafetyServiceStatus::ResetCommitted);
    TEST_ASSERT_TRUE(reset.restartRequestCount() == 1U);
    TEST_ASSERT_TRUE(service.faultCore().find(id)->status ==
                     FaultStatus::Cleared);

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
    TEST_ASSERT_TRUE(
        service.raiseFault({FaultCode::S3_004, 24U, 2U, 1U, std::nullopt}) ==
        SafetyServiceStatus::Ready);
    const auto* fault = service.faultCore().dominant();
    TEST_ASSERT_TRUE(fault != nullptr);

    const auto qualification =
        validRecoveryQualification(fault->instanceId, fault->faultRevision);
    const auto recovery = service.issueSafetyRecovery(qualification);
    TEST_ASSERT_TRUE(recovery.has_value());
    const auto gate = service.actuatorGateInput();
    TEST_ASSERT_TRUE(gate.status == ActuatorSafetyGateStatus::SafetyRecovery);

    ActuatorPlanner planner(recoveryPlannerParameters());
    const auto plan = planner.tick(
        {10U, std::nullopt, recovery->contextAtQualification(), true, gate});
    TEST_ASSERT_TRUE(plan.status == ActuatorPlanStatus::Active);
    TEST_ASSERT_TRUE(plan.reason == ActuatorPlanReason::SafetyRecovery);

    TEST_ASSERT_TRUE(service.requestControlledSafetyRestart(
                         fault->instanceId, fault->faultRevision) ==
                     SafetyServiceStatus::Ready);
    TEST_ASSERT_TRUE(
        reset.lastPurpose() ==
        device_platform::ControlledRestartPurpose::ControlledSafetyRestart);
    reset.setBootReset(ResetCause::ControlledSafetyRestart, true, 3U);
    const auto boot = service.evaluateBoot();
    TEST_ASSERT_TRUE(boot.status == SafetyServiceStatus::Ready);
    TEST_ASSERT_FALSE(boot.safeBootRequired);
    TEST_ASSERT_EQUAL_UINT32(
        1U, service.record().restartEpisode.abnormalRestartCount);
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

void test_repeated_cause_reactivates_same_instance_and_stales_old_reset() {
    FaultCore core;
    const auto first = core.raise(
        {FaultCode::S3_001, 7U, 8U, 10U, std::nullopt, 0x100000001ULL});
    TEST_ASSERT_TRUE(first.status == FaultRaiseStatus::Created);
    const auto* firstRecord = core.find(first.instanceId);
    TEST_ASSERT_NOT_NULL(firstRecord);
    const auto clearedRevision = firstRecord->faultRevision;
    TEST_ASSERT_TRUE(core.markCauseCleared(first.instanceId, clearedRevision));
    const auto reactivated = core.raise(
        {FaultCode::S3_001, 7U, 8U, 20U, std::nullopt, 0x100000001ULL});
    TEST_ASSERT_TRUE(reactivated.status == FaultRaiseStatus::Reactivated);
    TEST_ASSERT_TRUE(reactivated.instanceId == first.instanceId);
    TEST_ASSERT_TRUE(core.find(first.instanceId)->causeActive);
    TEST_ASSERT_TRUE(core.find(first.instanceId)->status ==
                     FaultStatus::ActiveUnacknowledged);
    TEST_ASSERT_TRUE(core.find(first.instanceId)->faultRevision >
                     clearedRevision);

    SimulatedPersistentStateStore store;
    device_platform_test_support::SimulatedResetController reset;
    device_platform::VirtualTimeSource time;
    SafetyFaultService service(store, reset, time);
    TEST_ASSERT_TRUE(service.begin({true, true, true}) ==
                     SafetyServiceStatus::Ready);
    TEST_ASSERT_TRUE(
        service.raiseFault({FaultCode::S3_004, 7U, 8U, 10U, std::nullopt}) ==
        SafetyServiceStatus::Ready);
    const auto* fault = service.faultCore().dominant();
    TEST_ASSERT_NOT_NULL(fault);
    const auto id = fault->instanceId;
    TEST_ASSERT_TRUE(service.clearFaultCause(id, fault->faultRevision) ==
                     SafetyServiceStatus::Ready);
    const auto oldResetRevision = service.faultCore().find(id)->faultRevision;
    TEST_ASSERT_TRUE(
        service.raiseFault({FaultCode::S3_004, 7U, 8U, 20U, std::nullopt}) ==
        SafetyServiceStatus::Ready);
    TEST_ASSERT_TRUE(service.faultCore().find(id)->causeActive);
    TEST_ASSERT_TRUE(service.resetFault(id, oldResetRevision).status ==
                     SafetyServiceStatus::StaleFault);
    TEST_ASSERT_TRUE(service.actuatorGateInput().status ==
                     ActuatorSafetyGateStatus::ImmediateStop);
}

void test_nonpersistent_process_and_operating_faults_keep_record_valid() {
    SimulatedPersistentStateStore store;
    device_platform_test_support::SimulatedResetController reset;
    device_platform::VirtualTimeSource time;
    SafetyFaultService service(store, reset, time);
    TEST_ASSERT_TRUE(service.begin({true, true, true}) ==
                     SafetyServiceStatus::Ready);
    TEST_ASSERT_TRUE(
        service.raiseFault({FaultCode::P1_001, 1U, 1U, 1U, std::nullopt}) ==
        SafetyServiceStatus::Ready);
    TEST_ASSERT_EQUAL_UINT32(0U, service.record().latchCount);
    TEST_ASSERT_TRUE(validateSafetyStateRecord(service.record()) ==
                     SafetyRecordValidation::Valid);
    const auto* warning = service.faultCore().dominant();
    TEST_ASSERT_NOT_NULL(warning);
    TEST_ASSERT_TRUE(
        service.clearFaultCause(warning->instanceId, warning->faultRevision) ==
        SafetyServiceStatus::Ready);
    TEST_ASSERT_TRUE(
        service.raiseFault({FaultCode::O2_001, 2U, 2U, 2U, std::nullopt}) ==
        SafetyServiceStatus::Ready);
    TEST_ASSERT_TRUE(service.disposition() == SafetyDisposition::ImmediateStop);
    TEST_ASSERT_EQUAL_UINT32(0U, service.record().latchCount);
    TEST_ASSERT_TRUE(validateSafetyStateRecord(service.record()) ==
                     SafetyRecordValidation::Valid);
    const auto* operating = service.faultCore().find({2U});
    TEST_ASSERT_NOT_NULL(operating);
    TEST_ASSERT_TRUE(service.clearFaultCause(operating->instanceId,
                                             operating->faultRevision) ==
                     SafetyServiceStatus::Ready);
    TEST_ASSERT_TRUE(service.faultCore().find(operating->instanceId)->status ==
                     FaultStatus::Cleared);
    TEST_ASSERT_TRUE(validateSafetyStateRecord(service.record()) ==
                     SafetyRecordValidation::Valid);
}

void test_fault_sequence_highwater_survives_reset_and_reboot() {
    SimulatedPersistentStateStore store;
    device_platform_test_support::SimulatedResetController reset;
    device_platform::VirtualTimeSource time;
    SafetyFaultService service(store, reset, time);
    TEST_ASSERT_TRUE(service.begin({true, true, true}) ==
                     SafetyServiceStatus::Ready);
    TEST_ASSERT_TRUE(service.consumeConfigurationStatus(
                         ConfigurationSafetyStatus::Operational, 56U, 1U) ==
                     SafetyServiceStatus::Ready);
    TEST_ASSERT_TRUE(
        service.raiseFault({FaultCode::S3_001, 1U, 1U, 1U, std::nullopt}) ==
        SafetyServiceStatus::Ready);
    const auto firstId = service.faultCore().dominant()->instanceId;
    TEST_ASSERT_TRUE(
        service.clearFaultCause(
            firstId, service.faultCore().find(firstId)->faultRevision) ==
        SafetyServiceStatus::Ready);
    const auto resetResult = service.resetFault(
        firstId, service.faultCore().find(firstId)->faultRevision);
    TEST_ASSERT_TRUE(resetResult.status == SafetyServiceStatus::ResetCommitted);
    store.restart();

    device_platform_test_support::SimulatedResetController resetAfterBoot;
    SafetyFaultService afterReboot(store, resetAfterBoot, time);
    TEST_ASSERT_TRUE(afterReboot.begin({true, true, true}) ==
                     SafetyServiceStatus::Ready);
    TEST_ASSERT_TRUE(afterReboot.consumeConfigurationStatus(
                         ConfigurationSafetyStatus::Operational, 56U, 2U) ==
                     SafetyServiceStatus::Ready);
    resetAfterBoot.setBootReset(ResetCause::AuthorizedRestart, true, 100U);
    TEST_ASSERT_TRUE(afterReboot.evaluateBoot().status ==
                     SafetyServiceStatus::Ready);
    TEST_ASSERT_TRUE(
        afterReboot.raiseFault({FaultCode::S3_002, 2U, 2U, 2U, std::nullopt}) ==
        SafetyServiceStatus::Ready);
    const auto secondId = afterReboot.faultCore().dominant()->instanceId;
    TEST_ASSERT_TRUE(secondId.value > firstId.value);

    FaultCore overflow;
    FaultCoreSnapshot maximum;
    maximum.instanceSequenceHighWatermark =
        std::numeric_limits<std::uint32_t>::max();
    TEST_ASSERT_FALSE(overflow.restoreSnapshot(maximum));
}

void test_class4_reset_uses_authorized_boot_exit_and_stays_fail_closed() {
    SimulatedPersistentStateStore store;
    device_platform_test_support::SimulatedResetController reset;
    device_platform::VirtualTimeSource time;
    SafetyFaultService service(store, reset, time);
    TEST_ASSERT_TRUE(service.begin({true, true, true}) ==
                     SafetyServiceStatus::Ready);
    TEST_ASSERT_TRUE(service.consumeConfigurationStatus(
                         ConfigurationSafetyStatus::Operational, 56U, 1U) ==
                     SafetyServiceStatus::Ready);
    TEST_ASSERT_TRUE(
        service.raiseFault({FaultCode::Y4_001, 56U, 1U, 1U, std::nullopt}) ==
        SafetyServiceStatus::Ready);
    TEST_ASSERT_TRUE(service.safeBootRequired());
    const auto id = service.faultCore().dominant()->instanceId;
    TEST_ASSERT_TRUE(service.clearFaultCause(
                         id, service.faultCore().find(id)->faultRevision) ==
                     SafetyServiceStatus::Ready);
    TEST_ASSERT_TRUE(
        service.resetFault(id, service.faultCore().find(id)->faultRevision)
            .status == SafetyServiceStatus::ResetCommitted);
    reset.setBootReset(ResetCause::AuthorizedRestart, true, 200U);
    const auto boot = service.evaluateBoot();
    TEST_ASSERT_TRUE(boot.status == SafetyServiceStatus::Ready);
    TEST_ASSERT_FALSE(boot.safeBootRequired);
    TEST_ASSERT_FALSE(service.record().safeBootRequired);
    const auto second = service.evaluateBoot();
    TEST_ASSERT_TRUE(second.status == SafetyServiceStatus::Ready);
    TEST_ASSERT_FALSE(second.safeBootRequired);

    SimulatedPersistentStateStore blockedStore;
    device_platform_test_support::SimulatedResetController blockedReset;
    SafetyFaultService blocked(blockedStore, blockedReset, time);
    TEST_ASSERT_TRUE(blocked.begin({true, true, true}) ==
                     SafetyServiceStatus::Ready);
    TEST_ASSERT_TRUE(
        blocked.raiseFault({FaultCode::Y4_001, 56U, 1U, 1U, std::nullopt}) ==
        SafetyServiceStatus::Ready);
    TEST_ASSERT_TRUE(
        blocked.raiseFault({FaultCode::Y4_002, 56U, 2U, 2U, std::nullopt}) ==
        SafetyServiceStatus::Ready);
    const auto blockedId = blocked.faultCore().find({1U})->instanceId;
    TEST_ASSERT_TRUE(
        blocked.clearFaultCause(
            blockedId, blocked.faultCore().find(blockedId)->faultRevision) ==
        SafetyServiceStatus::Ready);
    TEST_ASSERT_TRUE(
        blocked
            .resetFault(blockedId,
                        blocked.faultCore().find(blockedId)->faultRevision)
            .status == SafetyServiceStatus::SafetyRejected);

    SimulatedPersistentStateStore failedStore;
    device_platform_test_support::SimulatedResetController failedReset;
    SafetyFaultService failed(failedStore, failedReset, time);
    TEST_ASSERT_TRUE(failed.begin({true, true, true}) ==
                     SafetyServiceStatus::Ready);
    TEST_ASSERT_TRUE(failed.consumeConfigurationStatus(
                         ConfigurationSafetyStatus::Operational, 56U, 1U) ==
                     SafetyServiceStatus::Ready);
    TEST_ASSERT_TRUE(
        failed.raiseFault({FaultCode::Y4_001, 56U, 1U, 1U, std::nullopt}) ==
        SafetyServiceStatus::Ready);
    const auto failedId = failed.faultCore().dominant()->instanceId;
    TEST_ASSERT_TRUE(
        failed.clearFaultCause(
            failedId, failed.faultCore().find(failedId)->faultRevision) ==
        SafetyServiceStatus::Ready);
    TEST_ASSERT_TRUE(
        failed
            .resetFault(failedId,
                        failed.faultCore().find(failedId)->faultRevision)
            .status == SafetyServiceStatus::ResetCommitted);
    failedReset.setBootReset(ResetCause::AuthorizedRestart, true, 201U);
    failedStore.setNextWriteFault(
        SimulatedPersistentStateStore::WriteFault::PowerCutBeforeCommit);
    TEST_ASSERT_TRUE(failed.evaluateBoot().status ==
                     SafetyServiceStatus::PersistentWriteFailed);
    TEST_ASSERT_TRUE(failed.safeBootRequired());
}

void test_normal_reboot_without_intent_does_not_exit_safe_boot() {
    SimulatedPersistentStateStore store;
    device_platform_test_support::SimulatedResetController reset;
    device_platform::VirtualTimeSource time;
    SafetyFaultService service(store, reset, time);
    TEST_ASSERT_TRUE(service.begin({true, true, true}) ==
                     SafetyServiceStatus::Ready);
    TEST_ASSERT_TRUE(
        service.raiseFault({FaultCode::Y4_001, 56U, 1U, 1U, std::nullopt}) ==
        SafetyServiceStatus::Ready);
    reset.setBootReset(ResetCause::PowerOn, true, 300U);
    TEST_ASSERT_TRUE(service.evaluateBoot().safeBootRequired);
    TEST_ASSERT_TRUE(service.safeBootRequired());
}

void test_watchdog_keeps_full_64bit_diagnostic_evidence() {
    SimulatedPersistentStateStore store;
    device_platform_test_support::SimulatedResetController reset;
    device_platform::VirtualTimeSource time;
    SafetyFaultService service(store, reset, time);
    TEST_ASSERT_TRUE(service.begin({true, true, true}) ==
                     SafetyServiceStatus::Ready);
    const std::uint64_t first =
        static_cast<std::uint64_t>(std::numeric_limits<std::uint32_t>::max()) +
        5U;
    const std::uint64_t second = first + (1ULL << 32U);
    TEST_ASSERT_TRUE(service.consumeWatchdogEvidence({1U, first}) ==
                     SafetyServiceStatus::Ready);
    TEST_ASSERT_TRUE(service.consumeWatchdogEvidence({2U, second}) ==
                     SafetyServiceStatus::Ready);
    const auto snapshot = service.faultCore().snapshot();
    TEST_ASSERT_EQUAL_UINT32(2U, snapshot.count);
    TEST_ASSERT_EQUAL_UINT64(
        first, snapshot.records[0].diagnosticSequenceHighWatermark);
    TEST_ASSERT_EQUAL_UINT64(
        second, snapshot.records[1].diagnosticSequenceHighWatermark);
    TEST_ASSERT_TRUE(snapshot.records[0].correlationKey ==
                     snapshot.records[1].correlationKey);
    TEST_ASSERT_TRUE(validateSafetyStateRecord(service.record()) ==
                     SafetyRecordValidation::Valid);
}

void test_event_projection_is_deterministic_and_journal_failure_is_nonblocking() {
    SimulatedPersistentStateStore store;
    device_platform_test_support::SimulatedResetController reset;
    device_platform::VirtualTimeSource time;
    RecordingJournal journal;
    SafetyFaultService service(store, reset, time, &journal);
    TEST_ASSERT_TRUE(service.begin({true, true, true}) ==
                     SafetyServiceStatus::Ready);
    TEST_ASSERT_TRUE(
        service.raiseFault({FaultCode::S3_004, 24U, 1U, 1U, std::nullopt}) ==
        SafetyServiceStatus::Ready);
    const auto* fault = service.faultCore().dominant();
    TEST_ASSERT_NOT_NULL(fault);
    const auto id = fault->instanceId;
    TEST_ASSERT_TRUE(service.acknowledgeFault(id, fault->faultRevision) ==
                     SafetyServiceStatus::Ready);
    TEST_ASSERT_TRUE(service.clearFaultCause(
                         id, service.faultCore().find(id)->faultRevision) ==
                     SafetyServiceStatus::Ready);
    TEST_ASSERT_TRUE(
        service
            .resetFault(id, service.faultCore().find(id)->faultRevision, false)
            .status == SafetyServiceStatus::SafetyRejected);
    TEST_ASSERT_TRUE(service.consumeConfigurationStatus(
                         ConfigurationSafetyStatus::Operational, 56U, 1U) ==
                     SafetyServiceStatus::Ready);
    TEST_ASSERT_TRUE(
        service.resetFault(id, service.faultCore().find(id)->faultRevision)
            .status == SafetyServiceStatus::ResetCommitted);
    reset.setBootReset(ResetCause::AuthorizedRestart, true, 400U);
    TEST_ASSERT_TRUE(service.evaluateBoot().status ==
                     SafetyServiceStatus::Ready);
    bool sawCreated = false;
    bool sawAcknowledged = false;
    bool sawCauseCleared = false;
    bool sawRejected = false;
    bool sawCommitted = false;
    bool sawExit = false;
    for (const auto& message : journal.messages) {
        sawCreated = sawCreated ||
                     message.find("type=FaultCreated") != std::string::npos;
        sawAcknowledged =
            sawAcknowledged ||
            message.find("type=FaultAcknowledged") != std::string::npos;
        sawCauseCleared =
            sawCauseCleared ||
            message.find("type=FaultCauseCleared") != std::string::npos;
        sawRejected = sawRejected || message.find("type=FaultResetRejected") !=
                                         std::string::npos;
        sawCommitted =
            sawCommitted ||
            message.find("type=FaultResetCommitted") != std::string::npos;
        sawExit = sawExit ||
                  message.find("type=SafeBootExitDecided") != std::string::npos;
    }
    TEST_ASSERT_TRUE(sawCreated && sawAcknowledged && sawCauseCleared &&
                     sawRejected && sawCommitted && sawExit);
    TEST_ASSERT_TRUE(journal.messages.front().find("episode=") !=
                     std::string::npos);
    journal.fail = true;
    TEST_ASSERT_TRUE(
        service.raiseFault({FaultCode::P1_001, 1U, 2U, 2U, std::nullopt}) ==
        SafetyServiceStatus::Ready);
    TEST_ASSERT_TRUE(validateSafetyStateRecord(service.record()) ==
                     SafetyRecordValidation::Valid);
}

void test_configuration_recovery_producer_reaches_issue24_gate() {
    {
        ConfigurationSafetyFixture fixture;
        device_platform_test_support::SimulatedResetController reset;
        device_platform::VirtualTimeSource time;
        SafetyFaultService safety(fixture.store, reset, time);
        TEST_ASSERT_TRUE(safety.begin({true, true, true}) ==
                         SafetyServiceStatus::Ready);
        ConfigurationSafetyIntegrationGate gate(*fixture.recovery, safety);
        const auto result = gate.boot();
        TEST_ASSERT_TRUE(
            result.recovery.status ==
            ConfigurationRecoveryStatus::FactoryInitializationCompleted);
        TEST_ASSERT_TRUE(result.safetyStatus == SafetyServiceStatus::Ready);
        TEST_ASSERT_EQUAL_UINT32(0U, safety.record().latchCount);
    }
    {
        ConfigurationSafetyFixture fixture;
        const auto key = device_platform::StateStoreKey::create("cb0");
        TEST_ASSERT_TRUE(key.key.has_value());
        fixture.store.injectReadFailure(*key.key, true);
        device_platform_test_support::SimulatedResetController reset;
        device_platform::VirtualTimeSource time;
        SafetyFaultService safety(fixture.store, reset, time);
        TEST_ASSERT_TRUE(safety.begin({true, true, true}) ==
                         SafetyServiceStatus::Ready);
        ConfigurationSafetyIntegrationGate gate(*fixture.recovery, safety);
        const auto result = gate.boot();
        TEST_ASSERT_TRUE(result.recovery.status ==
                         ConfigurationRecoveryStatus::PersistenceReadFailure);
        TEST_ASSERT_TRUE(result.safetyStatus == SafetyServiceStatus::Ready);
        TEST_ASSERT_TRUE(safety.faultCore().dominant()->code ==
                         FaultCode::Y4_003);
        TEST_ASSERT_TRUE(safety.safeBootRequired());
        TEST_ASSERT_TRUE(validateSafetyStateRecord(safety.record()) ==
                         SafetyRecordValidation::Valid);
    }
    // The two #56 result objects use the real configuration-service result
    // type, not a test-only safety enum. The composition bridge maps both
    // producer failures into the same persistent #24 authority.
    for (const auto status :
         {ConfigurationCommitStatus::ConfigurationRuntimeFailure,
          ConfigurationCommitStatus::ConfigurationCommitIndeterminate}) {
        ConfigurationSafetyFixture fixture;
        device_platform_test_support::SimulatedResetController reset;
        device_platform::VirtualTimeSource time;
        SafetyFaultService safety(fixture.store, reset, time);
        TEST_ASSERT_TRUE(safety.begin({true, true, true}) ==
                         SafetyServiceStatus::Ready);
        ConfigurationSafetyIntegrationGate gate(*fixture.recovery, safety);
        ConfigurationCommitResult producerResult;
        producerResult.status = status;
        TEST_ASSERT_TRUE(gate.forward(producerResult) ==
                         SafetyServiceStatus::Ready);
        TEST_ASSERT_TRUE(safety.faultCore().dominant() != nullptr);
        TEST_ASSERT_TRUE(
            safety.faultCore().dominant()->code ==
            (status == ConfigurationCommitStatus::ConfigurationRuntimeFailure
                 ? FaultCode::Y4_001
                 : FaultCode::Y4_002));
        TEST_ASSERT_TRUE(safety.safeBootRequired());
        TEST_ASSERT_TRUE(validateSafetyStateRecord(safety.record()) ==
                         SafetyRecordValidation::Valid);
    }
    // The two #57 recovery result objects are the actual public recovery
    // result type returned by ConfigurationRecoveryService.
    for (const auto status :
         {ConfigurationRecoveryStatus::ConfigurationUnavailable,
          ConfigurationRecoveryStatus::ConfigurationIntegrityFailure}) {
        ConfigurationSafetyFixture fixture;
        device_platform_test_support::SimulatedResetController reset;
        device_platform::VirtualTimeSource time;
        SafetyFaultService safety(fixture.store, reset, time);
        TEST_ASSERT_TRUE(safety.begin({true, true, true}) ==
                         SafetyServiceStatus::Ready);
        ConfigurationSafetyIntegrationGate gate(*fixture.recovery, safety);
        TEST_ASSERT_TRUE(gate.forward(ConfigurationRecoveryResult{status, {}})
                             .safetyStatus == SafetyServiceStatus::Ready);
        TEST_ASSERT_TRUE(safety.faultCore().dominant() != nullptr);
        TEST_ASSERT_TRUE(
            safety.faultCore().dominant()->code ==
            (status == ConfigurationRecoveryStatus::ConfigurationUnavailable
                 ? FaultCode::Y4_003
                 : FaultCode::Y4_004));
        TEST_ASSERT_TRUE(safety.safeBootRequired());
        TEST_ASSERT_TRUE(validateSafetyStateRecord(safety.record()) ==
                         SafetyRecordValidation::Valid);
    }
}

void test_fault_commit_failure_rolls_back_ram_authority_and_locks_safe() {
    SimulatedPersistentStateStore store;
    device_platform_test_support::SimulatedResetController reset;
    device_platform::VirtualTimeSource time;
    SafetyFaultService service(store, reset, time);
    TEST_ASSERT_TRUE(service.begin({true, true, true}) ==
                     SafetyServiceStatus::Ready);
    store.setNextWriteFault(
        SimulatedPersistentStateStore::WriteFault::PowerCutBeforeCommit);
    TEST_ASSERT_TRUE(
        service.raiseFault({FaultCode::S3_001, 1U, 1U, 1U, std::nullopt}) ==
        SafetyServiceStatus::PersistentWriteFailed);
    TEST_ASSERT_EQUAL_UINT32(0U, service.faultCore().snapshot().count);
    TEST_ASSERT_TRUE(service.safeBootRequired());
    TEST_ASSERT_TRUE(
        service.raiseFault({FaultCode::S3_001, 1U, 1U, 2U, std::nullopt}) ==
        SafetyServiceStatus::Ready);
    TEST_ASSERT_EQUAL_UINT32(1U, service.faultCore().snapshot().count);
    TEST_ASSERT_TRUE(service.faultCore().dominant()->instanceId.value == 1U);
}

}  // namespace

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_fault_classes_codes_and_dominance_are_deterministic);
    RUN_TEST(test_fault_matrix_covers_codes_priority_capacity_and_overflow);
    RUN_TEST(test_fault_correlation_retains_instance_and_primary_is_bounded);
    RUN_TEST(test_safety_record_round_trip_and_factory_not_found_gate);
    RUN_TEST(test_restart_episode_counts_once_and_closes_after_stable_window);
    RUN_TEST(test_reset_port_is_stable_and_has_no_fault_semantics);
    RUN_TEST(test_safety_recovery_requires_every_qualified_precondition);
    RUN_TEST(
        test_safety_service_persists_latch_projects_gate_and_requires_reset);
    RUN_TEST(test_authorized_reset_is_write_before_apply_and_consumes_once);
    RUN_TEST(test_controlled_restart_and_recovery_gate_share_canonical_fault);
    RUN_TEST(test_unknown_boot_reset_creates_system_fault_and_safe_boots);
    RUN_TEST(
        test_repeated_cause_reactivates_same_instance_and_stales_old_reset);
    RUN_TEST(test_nonpersistent_process_and_operating_faults_keep_record_valid);
    RUN_TEST(test_fault_sequence_highwater_survives_reset_and_reboot);
    RUN_TEST(test_class4_reset_uses_authorized_boot_exit_and_stays_fail_closed);
    RUN_TEST(test_normal_reboot_without_intent_does_not_exit_safe_boot);
    RUN_TEST(test_watchdog_keeps_full_64bit_diagnostic_evidence);
    RUN_TEST(
        test_event_projection_is_deterministic_and_journal_failure_is_nonblocking);
    RUN_TEST(test_configuration_recovery_producer_reaches_issue24_gate);
    RUN_TEST(test_fault_commit_failure_rolls_back_ram_authority_and_locks_safe);
    return UNITY_END();
}
