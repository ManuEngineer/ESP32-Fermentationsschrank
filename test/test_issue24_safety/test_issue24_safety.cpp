#include <unity.h>

#include <array>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <string>

#include "actuator_plan_types.hpp"
#include "actuator_plan_sink_driver.hpp"
#include "actuator_planner.hpp"
#include "configuration_bootstrap_store.hpp"
#include "configuration_graph_store.hpp"
#include "configuration_mutation_coordinator.hpp"
#include "configuration_recovery_service.hpp"
#include "configuration_service.hpp"
#include "device_platform.hpp"
#include "fault_types.hpp"
#include "fermentation_application.hpp"
#include "restart_episode.hpp"
#include "run_persistence_contract.hpp"
#include "run_persistence_coordinator.hpp"
#include "safety_fault_service.hpp"
#include "safety_state_store.hpp"
#include "sensor_selection.hpp"
#include "simulated_persistent_state_store.hpp"
#include "simulated_reset_controller.hpp"
#include "standard_program_catalog.hpp"
#include "mock_bidirectional_actuator_sink.hpp"
#include "mock_binary_output_sink.hpp"
#include "mock_event_journal.hpp"
#include "temperature_control.hpp"
#include "temperature_control_orchestrator.hpp"
#include "temperature_control_types.hpp"
#include "virtual_time_source.hpp"

namespace {

using namespace fermentation;
using device_platform::ResetCause;
using device_platform::RestartRequestResult;
using device_platform_test_support::SimulatedPersistentStateStore;

class SafetyTestTimeZoneResolver final
    : public device_platform::ITimeZoneResolver {
   public:
    [[nodiscard]] device_platform::TimeZonePrepareResult prepare(
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

FaultResetSafetyEvidence passedResetChecks() {
    return {FaultResetCheckStatus::Passed, FaultResetCheckStatus::Passed,
            FaultResetCheckStatus::Passed, FaultResetCheckStatus::Passed};
}

FaultResetRequest resetRequestFor(FaultInstanceId id, std::uint32_t revision) {
    FaultResetRequest request;
    request.envelope.id = 100U;
    request.envelope.monotonicMillis = 0U;
    request.envelope.expectedStateSequence = 0U;
    request.envelope.expectedFaultRevision = revision;
    request.envelope.confirmed = true;
    request.targetFault = id;
    return request;
}

FaultResetAuthorizationEvidence authorizationFor(
    FaultInstanceId id, std::uint32_t revision,
    FaultResetAuthorizationLevel level =
        FaultResetAuthorizationLevel::Service) {
    FaultResetAuthorizationEvidence authorization;
    authorization.evidenceId = 200U;
    authorization.level = level;
    authorization.issuedAtMonotonicMillis = 0U;
    authorization.expiresAtMonotonicMillis = 100U;
    authorization.targetFault = id;
    authorization.targetFaultRevision = revision;
    return authorization;
}

void qualifyConfiguration(SafetyFaultService& service) {
    TEST_ASSERT_TRUE(service.consumeConfigurationStatus(
                         ConfigurationSafetyStatus::Operational, 56U, 1U) ==
                     SafetyServiceStatus::Ready);
}

class SafetyOutcomeStore final : public device_platform::IStateStore {
   public:
    enum class Fault : std::uint8_t {
        None,
        CommitUnknownNew,
        CommitUnknownOld,
        CommitUnknownReadError,
        CommitUnknownMismatch,
    };

    device_platform::StateStoreWriteStatus write(
        const device_platform::StateStoreKey& key,
        const std::string& value) override {
        const auto fault = nextFault_;
        nextFault_ = Fault::None;
        if (fault == Fault::CommitUnknownOld) {
            return device_platform::StateStoreWriteStatus::CommitOutcomeUnknown;
        }
        const auto status = backing_.write(key, value);
        if (status != device_platform::StateStoreWriteStatus::Success) {
            return status;
        }
        if (fault == Fault::CommitUnknownReadError) readError_ = true;
        if (fault == Fault::CommitUnknownMismatch) mismatch_ = true;
        return fault == Fault::CommitUnknownNew ||
                       fault == Fault::CommitUnknownReadError
                   ? device_platform::StateStoreWriteStatus::
                         CommitOutcomeUnknown
                   : device_platform::StateStoreWriteStatus::Success;
    }

    device_platform::StateStoreReadResult read(
        const device_platform::StateStoreKey& key,
        std::size_t maxBytes) const override {
        if (readError_) {
            readError_ = false;
            return {device_platform::StateStoreReadStatus::ReadError, {}};
        }
        if (mismatch_) {
            mismatch_ = false;
            return {device_platform::StateStoreReadStatus::Success,
                    "readback-mismatch"};
        }
        return backing_.read(key, maxBytes);
    }

    void setNextFault(Fault fault) { nextFault_ = fault; }

   private:
    device_platform_test_support::SimulatedPersistentStateStore backing_;
    Fault nextFault_{Fault::None};
    mutable bool readError_{false};
    mutable bool mismatch_{false};
};

void test_r3_code_matrix_and_unknown_are_fail_closed() {
    const std::array<FaultCode, 21U> codes = {
        FaultCode::P1_001, FaultCode::O2_001, FaultCode::O2_002,
        FaultCode::S3_001, FaultCode::S3_002, FaultCode::S3_003,
        FaultCode::S3_004, FaultCode::S3_005, FaultCode::S3_006,
        FaultCode::S3_007, FaultCode::S3_008, FaultCode::S3_009,
        FaultCode::Y4_001, FaultCode::Y4_002, FaultCode::Y4_003,
        FaultCode::Y4_004, FaultCode::Y4_005, FaultCode::Y4_006,
        FaultCode::Y4_007, FaultCode::Y4_008, FaultCode::Y4_009};
    for (const auto code : codes) {
        TEST_ASSERT_TRUE(isKnownFaultCode(code));
        TEST_ASSERT_TRUE(isKnownFaultClass(faultClassForCode(code)));
    }
    TEST_ASSERT_FALSE(isKnownFaultCode(static_cast<FaultCode>(0x400BU)));
    TEST_ASSERT_EQUAL_STRING("Y4-008", faultCodeText(FaultCode::Unknown));

    FaultCore core;
    TEST_ASSERT_TRUE(
        core.raise({FaultCode::P1_001, 1U, 1U, 1U, std::nullopt}).status ==
        FaultRaiseStatus::Created);
    TEST_ASSERT_TRUE(
        core.raise({FaultCode::S3_003, 2U, 1U, 2U, std::nullopt}).status ==
        FaultRaiseStatus::Created);
    TEST_ASSERT_TRUE(core.dominant()->code == FaultCode::S3_003);
    TEST_ASSERT_TRUE(core.hasBlockingFault());
}

void test_r3_record_is_fixed_1253_bytes_and_has_17_slots() {
    TEST_ASSERT_EQUAL_UINT32(17U, kMaximumPersistedLatches);
    TEST_ASSERT_EQUAL_UINT32(1216U, kSafetyRecordPayloadBytes);
    SafetyStateRecord record;
    std::string encoded;
    TEST_ASSERT_TRUE(encodeSafetyStateRecord(record, encoded) ==
                     SafetyRecordEncodeStatus::Success);
    TEST_ASSERT_EQUAL_UINT32(1253U, encoded.size());

    record.recordRevision = 2U;
    record.faultRevision = 1U;
    record.faultInstanceSequence = 1U;
    record.latchCount = 1U;
    record.latches[0].instanceId = {1U};
    record.latches[0].code = FaultCode::S3_003;
    record.latches[0].faultClass = FaultClass::LatchedSafetyFault;
    record.latches[0].faultRevision = 1U;
    record.latches[0].causeActive = true;
    record.latches[0].latched = true;
    TEST_ASSERT_TRUE(encodeSafetyStateRecord(record, encoded) ==
                     SafetyRecordEncodeStatus::Success);
    SafetyStateRecord decoded;
    TEST_ASSERT_TRUE(decodeSafetyStateRecord(encoded, decoded) ==
                     SafetyRecordDecodeStatus::Success);
    TEST_ASSERT_EQUAL_UINT32(17U, decoded.latches.size());
    TEST_ASSERT_EQUAL_UINT32(1U, decoded.latchCount);
    TEST_ASSERT_TRUE(decoded.latches[0].code == FaultCode::S3_003);
}

void test_neutral_reset_port_has_stable_observation_and_result() {
    device_platform_test_support::SimulatedResetController reset;
    reset.setBootReset(ResetCause::Brownout, true, 42U);
    TEST_ASSERT_EQUAL_UINT32(42U, reset.observeBootReset().observationId);
    TEST_ASSERT_TRUE(reset.observeBootReset().cause == ResetCause::Brownout);
    TEST_ASSERT_TRUE(reset.requestRestart() == RestartRequestResult::Accepted);
    TEST_ASSERT_EQUAL_UINT32(1U, reset.restartRequestCount());
}

void test_restart_episode_counts_once_third_and_stable_window() {
    SafetyStateRecord record;
    RestartEpisodeCoordinator episode;
    const auto first =
        episode.evaluateBoot(record, {ResetCause::WatchdogOrPanic, true, 1U});
    TEST_ASSERT_TRUE(first.status == RestartBootStatus::AbnormalRecorded);
    const auto repeated =
        episode.evaluateBoot(record, {ResetCause::WatchdogOrPanic, true, 1U});
    TEST_ASSERT_FALSE(repeated.recordNeedsCommit);
    TEST_ASSERT_EQUAL_UINT32(1U, record.restartEpisode.abnormalRestartCount);
    TEST_ASSERT_TRUE(
        episode.evaluateBoot(record, {ResetCause::Brownout, true, 2U}).status ==
        RestartBootStatus::AbnormalRecorded);
    TEST_ASSERT_TRUE(
        episode.evaluateBoot(record, {ResetCause::ExternalOrOther, true, 3U})
            .status == RestartBootStatus::SafeBootRequired);
    TEST_ASSERT_TRUE(record.safeBootRequired);

    record.safeBootRequired = false;
    TEST_ASSERT_FALSE(episode.advanceStableWindow(record, 0U));
    TEST_ASSERT_FALSE(
        episode.advanceStableWindow(record, kStableRestartWindowMillis - 1U));
    TEST_ASSERT_TRUE(
        episode.advanceStableWindow(record, kStableRestartWindowMillis));
    TEST_ASSERT_FALSE(record.restartEpisode.open);
}

void test_s3_003_is_explicit_injection_not_thermal_enum() {
    SimulatedPersistentStateStore store;
    device_platform_test_support::SimulatedResetController reset;
    device_platform::VirtualTimeSource time;
    SafetyFaultService service(store, reset, time);
    TEST_ASSERT_TRUE(service.begin({true, true, true}) ==
                     SafetyServiceStatus::Ready);

    // This is the real #21 return-validation evidence shape: all role
    // snapshots are valid while compatibility is Incompatible. The #21
    // decision remains its own AirFallback/ReturnValidation contract.
    SensorSelectionStateView view;
    view.activeRunId = "run-1";
    view.runtime.phase = SensorSelectionPhase::ReturnValidationPending;
    view.runtime.permission = SensorPeltierPermission::Allowed;
    view.activeMode = RunSensorMode::Air;
    view.persisted = PersistedSensorSelectionState{
        SensorSelectionProvenance::FallbackActive,
        SensorSelectionDecisionCause::FallbackToAir, 2U};
    view.runRevision = 2U;
    view.runtime.returnValidation.enteredAtMonotonicMillis = 100U;
    view.runtime.returnValidation.lastObservedProfileRevision = 7U;
    SensorSelectionDecision decision;
    decision.expected = view;
    decision.program.sensorPreference =
        SensorPreference::ProductIfAvailableElseAir;
    decision.program.policy =
        ProductSensorFailurePolicy::FallbackToAirAfterTimeout;
    decision.program.returnStrategy =
        ReturnStrategy::AutomaticValidatedReturnToProduct;
    decision.plausibility.air.quality = device_platform::SensorQuality::Valid;
    decision.plausibility.product.quality =
        device_platform::SensorQuality::Valid;
    decision.plausibility.cooling.quality =
        device_platform::SensorQuality::Valid;
    decision.plausibility.evaluationMonotonicMillis = 300U;
    decision.plausibility.thermalCompatibility = {
        ThermalCompatibility::Incompatible, 7U, 200U};
    const auto selection = applySensorSelectionDecision(view, decision, 300U);
    TEST_ASSERT_TRUE(selection.status ==
                     SensorSelectionApplyStatus::AppliedPersistentCandidate);
    TEST_ASSERT_TRUE(selection.runtime.phase ==
                     SensorSelectionPhase::AirFallbackActive);
    TEST_ASSERT_TRUE(selection.notice->cause ==
                     SensorSelectionDecisionCause::ReturnValidationAborted);
    TEST_ASSERT_TRUE(service.faultCore().snapshot().count == 0U);

    // The stronger #24 cause has a separate reproducer and stable code.
    TEST_ASSERT_TRUE(
        service.raiseFault({FaultCode::S3_003, 24U, 1U, 1U, std::nullopt}) ==
        SafetyServiceStatus::Ready);
    TEST_ASSERT_TRUE(service.actuatorGateInput().status ==
                     ActuatorSafetyGateStatus::ImmediateStop);
    const auto* fault = service.faultCore().dominant();
    TEST_ASSERT_NOT_NULL(fault);
    const auto faultId = fault->instanceId;
    const auto clearRevision = fault->faultRevision;
    TEST_ASSERT_TRUE(
        service.clearFaultCause(fault->instanceId, clearRevision) ==
        SafetyServiceStatus::Ready);
    const auto* cleared = service.faultCore().find(faultId);
    TEST_ASSERT_NOT_NULL(cleared);
    FaultResetRequest resetRequest;
    resetRequest.envelope.id = 2U;
    resetRequest.envelope.monotonicMillis = 1U;
    resetRequest.envelope.expectedStateSequence = 0U;
    resetRequest.envelope.expectedFaultRevision = cleared->faultRevision;
    resetRequest.envelope.confirmed = true;
    resetRequest.targetFault = faultId;
    FaultResetAuthorizationEvidence authorization;
    authorization.evidenceId = 3U;
    authorization.level = FaultResetAuthorizationLevel::Service;
    authorization.expiresAtMonotonicMillis = 1U;
    authorization.targetFault = faultId;
    authorization.targetFaultRevision = cleared->faultRevision;
    FaultResetSafetyEvidence safetyEvidence{
        FaultResetCheckStatus::Passed, FaultResetCheckStatus::Passed,
        FaultResetCheckStatus::Passed, FaultResetCheckStatus::Passed};
    TEST_ASSERT_TRUE(service.consumeConfigurationStatus(
                         ConfigurationSafetyStatus::Operational, 56U, 1U) ==
                     SafetyServiceStatus::Ready);
    TEST_ASSERT_TRUE(
        service.resetFault(resetRequest, authorization, safetyEvidence)
            .status == SafetyServiceStatus::ResetCommitted);
    TEST_ASSERT_NULL(service.faultCore().find(faultId));
    TEST_ASSERT_EQUAL_UINT32(0U, reset.restartRequestCount());
}

void test_air_limit_is_normal_control_state_without_fault() {
    TemperatureControlResult reduced;
    reduced.status = TemperatureControlStatus::Demand;
    reduced.reason = TemperatureControlReason::AirLimitReduced;
    reduced.airLimitState = AirLimitState::Reduced;
    TEST_ASSERT_TRUE(classifyActuatorDemand(reduced) ==
                     ActuatorDemandClass::AirLimitReducedDemand);

    TemperatureControlResult blocked;
    blocked.status = TemperatureControlStatus::Off;
    blocked.reason = TemperatureControlReason::AirLimitBlocked;
    blocked.airLimitState = AirLimitState::Blocked;
    TEST_ASSERT_TRUE(classifyActuatorDemand(blocked) ==
                     ActuatorDemandClass::AirLimitBlockedOff);

    FaultCore core;
    TEST_ASSERT_EQUAL_UINT32(0U, core.snapshot().count);
}

void test_real_p1_and_sensor_quality_producers_keep_o2_without_o2_003() {
    SimulatedPersistentStateStore store;
    device_platform_test_support::SimulatedResetController reset;
    device_platform::VirtualTimeSource time;
    SafetyFaultService service(store, reset, time);
    TEST_ASSERT_TRUE(service.begin({true, true, true}) ==
                     SafetyServiceStatus::Ready);
    TEST_ASSERT_TRUE(service.consumeProcessMessage(
                         ProcessMessage::TargetReachTimeExceeded, 22U, 1U) ==
                     SafetyServiceStatus::Ready);
    TEST_ASSERT_TRUE(service.faultCore().dominant()->code == FaultCode::P1_001);

    device_platform::SensorQualitySnapshot product;
    product.quality = device_platform::SensorQuality::Stale;
    TEST_ASSERT_TRUE(service.consumeSensorQuality(SafetySensorRole::Product,
                                                  product, 20U, 2U) ==
                     SafetyServiceStatus::Ready);
    TEST_ASSERT_TRUE(isKnownFaultCode(FaultCode::O2_001));
    TEST_ASSERT_FALSE(isKnownFaultCode(static_cast<FaultCode>(0x2003U)));
}

void test_s3_004_is_contract_only_and_capacity_uses_marker_without_evict() {
    SimulatedPersistentStateStore store;
    device_platform_test_support::SimulatedResetController reset;
    device_platform::VirtualTimeSource time;
    SafetyFaultService service(store, reset, time);
    TEST_ASSERT_TRUE(service.begin({true, true, true}) ==
                     SafetyServiceStatus::Ready);
    TEST_ASSERT_TRUE(
        service.raiseFault({FaultCode::S3_004, 24U, 1U, 1U, std::nullopt}) ==
        SafetyServiceStatus::Ready);
    TEST_ASSERT_TRUE(service.actuatorGateInput().status ==
                     ActuatorSafetyGateStatus::ImmediateStop);

    SimulatedPersistentStateStore capacityStore;
    device_platform_test_support::SimulatedResetController capacityReset;
    device_platform::VirtualTimeSource capacityTime;
    SafetyFaultService capacity(capacityStore, capacityReset, capacityTime);
    TEST_ASSERT_TRUE(capacity.begin({true, true, true}) ==
                     SafetyServiceStatus::Ready);
    for (std::uint32_t index = 0U; index < 17U; ++index) {
        TEST_ASSERT_TRUE(
            capacity.raiseFault({FaultCode::S3_001, 100U + index, index + 1U,
                                 index + 1U, std::nullopt}) ==
            SafetyServiceStatus::Ready);
    }
    TEST_ASSERT_TRUE(capacity.raiseFault(
                         {FaultCode::S3_002, 999U, 999U, 999U, std::nullopt}) ==
                     SafetyServiceStatus::FaultCapacityReached);
    TEST_ASSERT_EQUAL_UINT32(17U, capacity.record().latchCount);
    TEST_ASSERT_TRUE(capacity.record().capacityFailureLatched);
    TEST_ASSERT_TRUE(capacity.safeBootRequired());
}

void test_safety_write_failure_keeps_ram_locked() {
    SimulatedPersistentStateStore store;
    device_platform_test_support::SimulatedResetController reset;
    device_platform::VirtualTimeSource time;
    SafetyFaultService service(store, reset, time);
    TEST_ASSERT_TRUE(service.begin({true, true, true}) ==
                     SafetyServiceStatus::Ready);
    store.setNextWriteFault(
        SimulatedPersistentStateStore::WriteFault::FailBeforeBegin);
    TEST_ASSERT_TRUE(
        service.raiseFault({FaultCode::S3_003, 24U, 2U, 1U, std::nullopt}) ==
        SafetyServiceStatus::PersistentWriteFailed);
    TEST_ASSERT_TRUE(service.safeBootRequired());
    TEST_ASSERT_TRUE(service.record().capacityFailureLatched);
    TEST_ASSERT_EQUAL_UINT32(1U, service.record().latchCount);
    TEST_ASSERT_TRUE(service.record().latches[0].code == FaultCode::S3_003);
    TEST_ASSERT_TRUE(service.actuatorGateInput().status ==
                     ActuatorSafetyGateStatus::ImmediateStop);

    TEST_ASSERT_TRUE(
        service.raiseFault({FaultCode::S3_005, 24U, 3U, 2U, std::nullopt}) ==
        SafetyServiceStatus::Ready);
    TEST_ASSERT_TRUE(service.record().capacityFailureLatched);
    TEST_ASSERT_TRUE(service.safeBootRequired());
}

void test_safety_commit_outcomes_are_fail_closed_and_exact_unknown_is_confirmed() {
    {
        SafetyOutcomeStore store;
        device_platform_test_support::SimulatedResetController reset;
        device_platform::VirtualTimeSource time;
        SafetyFaultService service(store, reset, time);
        TEST_ASSERT_TRUE(service.begin({true, true, true}) ==
                         SafetyServiceStatus::Ready);
        store.setNextFault(SafetyOutcomeStore::Fault::CommitUnknownNew);
        TEST_ASSERT_TRUE(service.raiseFault(
                             {FaultCode::S3_003, 24U, 1U, 1U, std::nullopt}) ==
                         SafetyServiceStatus::Ready);
        TEST_ASSERT_FALSE(service.record().capacityFailureLatched);
    }
    {
        SafetyOutcomeStore store;
        device_platform_test_support::SimulatedResetController reset;
        device_platform::VirtualTimeSource time;
        SafetyFaultService service(store, reset, time);
        TEST_ASSERT_TRUE(service.begin({true, true, true}) ==
                         SafetyServiceStatus::Ready);
        store.setNextFault(SafetyOutcomeStore::Fault::CommitUnknownOld);
        TEST_ASSERT_TRUE(service.raiseFault(
                             {FaultCode::S3_003, 24U, 1U, 1U, std::nullopt}) ==
                         SafetyServiceStatus::PersistentWriteFailed);
        TEST_ASSERT_TRUE(service.record().capacityFailureLatched);
    }
    {
        SafetyOutcomeStore store;
        device_platform_test_support::SimulatedResetController reset;
        device_platform::VirtualTimeSource time;
        SafetyFaultService service(store, reset, time);
        TEST_ASSERT_TRUE(service.begin({true, true, true}) ==
                         SafetyServiceStatus::Ready);
        store.setNextFault(SafetyOutcomeStore::Fault::CommitUnknownReadError);
        TEST_ASSERT_TRUE(service.raiseFault(
                             {FaultCode::S3_003, 24U, 1U, 1U, std::nullopt}) ==
                         SafetyServiceStatus::PersistentWriteFailed);
        TEST_ASSERT_TRUE(service.record().capacityFailureLatched);
    }
    {
        SafetyOutcomeStore store;
        device_platform_test_support::SimulatedResetController reset;
        device_platform::VirtualTimeSource time;
        SafetyFaultService service(store, reset, time);
        TEST_ASSERT_TRUE(service.begin({true, true, true}) ==
                         SafetyServiceStatus::Ready);
        store.setNextFault(SafetyOutcomeStore::Fault::CommitUnknownMismatch);
        TEST_ASSERT_TRUE(service.raiseFault(
                             {FaultCode::S3_003, 24U, 1U, 1U, std::nullopt}) ==
                         SafetyServiceStatus::PersistentWriteFailed);
        TEST_ASSERT_TRUE(service.record().capacityFailureLatched);
    }
    {
        SimulatedPersistentStateStore store;
        device_platform_test_support::SimulatedResetController reset;
        device_platform::VirtualTimeSource time;
        SafetyFaultService service(store, reset, time);
        TEST_ASSERT_TRUE(service.begin({true, true, true}) ==
                         SafetyServiceStatus::Ready);
        store.setNextWriteFault(SimulatedPersistentStateStore::WriteFault::
                                    PowerCutAfterCommitBeforeReturn);
        TEST_ASSERT_TRUE(service.raiseFault(
                             {FaultCode::S3_003, 24U, 1U, 1U, std::nullopt}) ==
                         SafetyServiceStatus::Ready);
        TEST_ASSERT_FALSE(service.record().capacityFailureLatched);
        TEST_ASSERT_FALSE(service.safeBootRequired());
    }
    {
        SimulatedPersistentStateStore store;
        device_platform_test_support::SimulatedResetController reset;
        device_platform::VirtualTimeSource time;
        SafetyFaultService service(store, reset, time);
        TEST_ASSERT_TRUE(service.begin({true, true, true}) ==
                         SafetyServiceStatus::Ready);
        store.setNextWriteFault(
            SimulatedPersistentStateStore::WriteFault::PowerCutBeforeCommit);
        TEST_ASSERT_TRUE(service.raiseFault(
                             {FaultCode::S3_003, 24U, 1U, 1U, std::nullopt}) ==
                         SafetyServiceStatus::PersistentWriteFailed);
        TEST_ASSERT_TRUE(service.record().capacityFailureLatched);
        TEST_ASSERT_TRUE(service.record().latchCount == 1U);
    }
    {
        SimulatedPersistentStateStore store;
        device_platform_test_support::SimulatedResetController reset;
        device_platform::VirtualTimeSource time;
        SafetyFaultService service(store, reset, time);
        TEST_ASSERT_TRUE(service.begin({true, true, true}) ==
                         SafetyServiceStatus::Ready);
        const auto key = device_platform::StateStoreKey::create("safety24");
        TEST_ASSERT_TRUE(key.key.has_value());
        store.injectReadFailure(*key.key, true);
        store.setNextWriteFault(SimulatedPersistentStateStore::WriteFault::
                                    PowerCutAfterCommitBeforeReturn);
        TEST_ASSERT_TRUE(service.raiseFault(
                             {FaultCode::S3_003, 24U, 1U, 1U, std::nullopt}) ==
                         SafetyServiceStatus::PersistentWriteFailed);
        TEST_ASSERT_TRUE(service.record().capacityFailureLatched);
        TEST_ASSERT_TRUE(service.safeBootRequired());
    }
    {
        SimulatedPersistentStateStore store;
        device_platform_test_support::SimulatedResetController reset;
        device_platform::VirtualTimeSource time;
        SafetyFaultService service(store, reset, time);
        TEST_ASSERT_TRUE(service.begin({true, true, true}) ==
                         SafetyServiceStatus::Ready);
        const auto key = device_platform::StateStoreKey::create("safety24");
        TEST_ASSERT_TRUE(key.key.has_value());
        store.injectCorruption(*key.key, "corrupt-safety-record");
        SafetyFaultService restarted(store, reset, time);
        TEST_ASSERT_TRUE(restarted.begin() ==
                         SafetyServiceStatus::PersistentReadFailed);
    }
}

void test_contract_injection_codes_and_journal_failure_remain_distinct() {
    SimulatedPersistentStateStore store;
    device_platform_test_support::SimulatedResetController reset;
    device_platform::VirtualTimeSource time;
    device_platform_test_support::MockEventJournal journal;
    journal.injectWriteFailure(true);
    SafetyFaultService service(store, reset, time, &journal);
    TEST_ASSERT_TRUE(service.begin({true, true, true}) ==
                     SafetyServiceStatus::Ready);
    const std::array<FaultCode, 5U> codes = {
        FaultCode::S3_004, FaultCode::S3_005, FaultCode::S3_006,
        FaultCode::S3_007, FaultCode::S3_009};
    for (std::uint32_t index = 0U; index < codes.size(); ++index) {
        TEST_ASSERT_TRUE(service.raiseFault({codes[index], 24U, index + 1U,
                                             index + 1U, std::nullopt}) ==
                         SafetyServiceStatus::Ready);
    }
    const auto snapshot = service.faultCore().snapshot();
    TEST_ASSERT_EQUAL_UINT32(codes.size(), snapshot.count);
    for (const auto code : codes) {
        bool found = false;
        for (std::size_t index = 0U; index < snapshot.count; ++index) {
            found = found || snapshot.records[index].code == code;
        }
        TEST_ASSERT_TRUE(found);
    }
    TEST_ASSERT_EQUAL_UINT32(0U, journal.entries().size());
}

void test_fault_reset_evaluation_requires_auth_and_all_safety_checks() {
    SimulatedPersistentStateStore store;
    device_platform_test_support::SimulatedResetController reset;
    device_platform::VirtualTimeSource time;
    SafetyFaultService service(store, reset, time);
    TEST_ASSERT_TRUE(service.begin({true, true, true}) ==
                     SafetyServiceStatus::Ready);
    qualifyConfiguration(service);
    TEST_ASSERT_TRUE(
        service.raiseFault({FaultCode::S3_003, 24U, 1U, 1U, std::nullopt}) ==
        SafetyServiceStatus::Ready);
    const auto* active = service.faultCore().dominant();
    TEST_ASSERT_NOT_NULL(active);
    const auto id = active->instanceId;
    TEST_ASSERT_TRUE(service.clearFaultCause(id, active->faultRevision) ==
                     SafetyServiceStatus::Ready);
    active = service.faultCore().find(id);
    TEST_ASSERT_NOT_NULL(active);
    const auto revision = active->faultRevision;
    const auto request = resetRequestFor(id, revision);

    auto missing = service.evaluateFaultReset(request, {}, passedResetChecks());
    TEST_ASSERT_FALSE(missing.allowed);
    TEST_ASSERT_TRUE(missing.rejection ==
                     FaultResetRejection::AuthorizationMissing);

    const auto lowAuthorization =
        authorizationFor(id, revision, FaultResetAuthorizationLevel::Operator);
    auto low = service.evaluateFaultReset(request, lowAuthorization,
                                          passedResetChecks());
    TEST_ASSERT_FALSE(low.allowed);
    TEST_ASSERT_TRUE(low.rejection ==
                     FaultResetRejection::AuthorizationMissing);

    auto failedSensor = passedResetChecks();
    failedSensor.sensor = FaultResetCheckStatus::Failed;
    auto sensor = service.evaluateFaultReset(
        request, authorizationFor(id, revision), failedSensor);
    TEST_ASSERT_FALSE(sensor.allowed);
    TEST_ASSERT_FALSE(sensor.safetyChecksPassed);

    auto failedActuator = passedResetChecks();
    failedActuator.actuator = FaultResetCheckStatus::Failed;
    TEST_ASSERT_FALSE(service
                          .evaluateFaultReset(request,
                                              authorizationFor(id, revision),
                                              failedActuator)
                          .allowed);
    auto failedPersistence = passedResetChecks();
    failedPersistence.persistence = FaultResetCheckStatus::Failed;
    TEST_ASSERT_FALSE(service
                          .evaluateFaultReset(request,
                                              authorizationFor(id, revision),
                                              failedPersistence)
                          .allowed);
    auto failedIntegrity = passedResetChecks();
    failedIntegrity.integrity = FaultResetCheckStatus::Failed;
    TEST_ASSERT_FALSE(service
                          .evaluateFaultReset(request,
                                              authorizationFor(id, revision),
                                              failedIntegrity)
                          .allowed);

    // Acknowledgement is a separate command state; it does not reset the
    // cleared-and-locked latch.
    TEST_ASSERT_TRUE(service.acknowledgeFault(id, revision) ==
                     SafetyServiceStatus::Ready);
    active = service.faultCore().find(id);
    TEST_ASSERT_NOT_NULL(active);
    const auto acknowledgedRevision = active->faultRevision;
    const auto acknowledgedRequest = resetRequestFor(id, acknowledgedRevision);
    const auto acknowledgedAuthorization =
        authorizationFor(id, acknowledgedRevision);
    const auto committed = service.resetFault(
        acknowledgedRequest, acknowledgedAuthorization, passedResetChecks());
    TEST_ASSERT_TRUE(committed.status == SafetyServiceStatus::ResetCommitted);
    TEST_ASSERT_NULL(service.faultCore().find(id));
    TEST_ASSERT_EQUAL_UINT32(0U, reset.restartRequestCount());
}

void test_fault_reset_rejects_stale_revision_and_other_blocking_fault() {
    SimulatedPersistentStateStore store;
    device_platform_test_support::SimulatedResetController reset;
    device_platform::VirtualTimeSource time;
    SafetyFaultService service(store, reset, time);
    TEST_ASSERT_TRUE(service.begin({true, true, true}) ==
                     SafetyServiceStatus::Ready);
    qualifyConfiguration(service);
    TEST_ASSERT_TRUE(
        service.raiseFault({FaultCode::S3_003, 24U, 1U, 1U, std::nullopt}) ==
        SafetyServiceStatus::Ready);
    const auto* target = service.faultCore().dominant();
    TEST_ASSERT_NOT_NULL(target);
    const auto targetId = target->instanceId;
    TEST_ASSERT_TRUE(service.clearFaultCause(targetId, target->faultRevision) ==
                     SafetyServiceStatus::Ready);
    target = service.faultCore().find(targetId);
    TEST_ASSERT_NOT_NULL(target);
    const auto revision = target->faultRevision;
    auto staleRequest = resetRequestFor(targetId, revision - 1U);
    auto staleAuthorization = authorizationFor(targetId, revision);
    const auto stale = service.evaluateFaultReset(
        staleRequest, staleAuthorization, passedResetChecks());
    TEST_ASSERT_TRUE(stale.rejection == FaultResetRejection::StaleEvaluation);

    TEST_ASSERT_TRUE(
        service.raiseFault({FaultCode::S3_004, 24U, 2U, 2U, std::nullopt}) ==
        SafetyServiceStatus::Ready);
    const auto blocked = service.evaluateFaultReset(
        resetRequestFor(targetId, revision),
        authorizationFor(targetId, revision), passedResetChecks());
    TEST_ASSERT_FALSE(blocked.allowed);
    TEST_ASSERT_TRUE(blocked.rejection ==
                     FaultResetRejection::OtherActiveFault);
}

void test_o2_and_p1_lifecycle_uses_producer_specific_resolution() {
    SimulatedPersistentStateStore store;
    device_platform_test_support::SimulatedResetController reset;
    device_platform::VirtualTimeSource time;
    SafetyFaultService service(store, reset, time);
    TEST_ASSERT_TRUE(service.begin({true, true, true}) ==
                     SafetyServiceStatus::Ready);
    qualifyConfiguration(service);

    device_platform::SensorQualitySnapshot staleProduct;
    staleProduct.quality = device_platform::SensorQuality::Stale;
    TEST_ASSERT_TRUE(service.consumeSensorQuality(SafetySensorRole::Product,
                                                  staleProduct, 21U, 1U) ==
                     SafetyServiceStatus::Ready);
    TEST_ASSERT_TRUE(service.actuatorGateInput().status ==
                     ActuatorSafetyGateStatus::ImmediateStop);

    SensorSelectionStateView selection;
    selection.runtime.phase = SensorSelectionPhase::AirFallbackActive;
    selection.runtime.permission = SensorPeltierPermission::Allowed;
    selection.activeMode = RunSensorMode::Air;
    CrossRolePlausibilityContext fallbackEvidence;
    fallbackEvidence.air.quality = device_platform::SensorQuality::Valid;
    fallbackEvidence.cooling.quality = device_platform::SensorQuality::Valid;
    TEST_ASSERT_TRUE(service.consumeSensorSelectionEvidence(
                         selection, fallbackEvidence, 21U, 1U) ==
                     SafetyServiceStatus::Ready);
    TEST_ASSERT_TRUE(service.actuatorGateInput().status ==
                     ActuatorSafetyGateStatus::Allowed);

    device_platform::SensorQualitySnapshot staleSafety;
    staleSafety.quality = device_platform::SensorQuality::Stale;
    TEST_ASSERT_TRUE(service.consumeSensorQuality(SafetySensorRole::Cooling,
                                                  staleSafety, 22U, 2U) ==
                     SafetyServiceStatus::Ready);
    device_platform::SensorQualitySnapshot validSafety;
    validSafety.quality = device_platform::SensorQuality::Valid;
    TEST_ASSERT_TRUE(service.consumeSensorQuality(SafetySensorRole::Cooling,
                                                  validSafety, 22U, 3U) ==
                     SafetyServiceStatus::Ready);
    for (std::size_t index = 0U; index < service.faultCore().snapshot().count;
         ++index) {
        TEST_ASSERT_FALSE(service.faultCore().snapshot().records[index].code ==
                          FaultCode::O2_002);
    }

    TEST_ASSERT_TRUE(service.consumeProcessMessage(
                         ProcessMessage::TargetReachTimeExceeded, 23U, 4U) ==
                     SafetyServiceStatus::Ready);
    TEST_ASSERT_TRUE(
        service.consumeProcessMessage(ProcessMessage::RunCompleted, 23U, 5U) ==
        SafetyServiceStatus::Ready);
    for (std::size_t index = 0U; index < service.faultCore().snapshot().count;
         ++index) {
        TEST_ASSERT_FALSE(service.faultCore().snapshot().records[index].code ==
                          FaultCode::P1_001);
    }
}

void test_automatic_restart_is_code_policy_bounded_and_once_per_episode() {
    SimulatedPersistentStateStore store;
    device_platform_test_support::SimulatedResetController reset;
    device_platform::VirtualTimeSource time;
    SafetyFaultService service(store, reset, time);
    TEST_ASSERT_TRUE(service.begin({true, true, true}) ==
                     SafetyServiceStatus::Ready);
    qualifyConfiguration(service);

    const std::array<FaultCode, 3U> physicalCodes = {
        FaultCode::S3_001, FaultCode::S3_002, FaultCode::S3_004};
    for (std::uint32_t index = 0U; index < physicalCodes.size(); ++index) {
        TEST_ASSERT_TRUE(service.raiseFault(
                             {physicalCodes[index], 24U, index + 1U, index + 1U,
                              std::nullopt}) == SafetyServiceStatus::Ready);
        const auto snapshot = service.faultCore().snapshot();
        const FaultRecord* physical = nullptr;
        for (std::size_t recordIndex = 0U; recordIndex < snapshot.count;
             ++recordIndex) {
            if (snapshot.records[recordIndex].code == physicalCodes[index]) {
                physical = &snapshot.records[recordIndex];
                break;
            }
        }
        TEST_ASSERT_NOT_NULL(physical);
        TEST_ASSERT_TRUE(service.requestControlledSafetyRestart(
                             physical->instanceId, physical->faultRevision) ==
                         SafetyServiceStatus::SafetyRejected);
    }

    TEST_ASSERT_TRUE(
        service.raiseFault({FaultCode::Y4_007, 24U, 2U, 2U, std::nullopt}) ==
        SafetyServiceStatus::Ready);
    const auto snapshot = service.faultCore().snapshot();
    const FaultRecord* recovery = nullptr;
    for (std::size_t index = 0U; index < snapshot.count; ++index) {
        if (snapshot.records[index].code == FaultCode::Y4_007) {
            recovery = &snapshot.records[index];
            break;
        }
    }
    TEST_ASSERT_NOT_NULL(recovery);
    const auto recoveryId = recovery->instanceId;
    const auto recoveryRevision = recovery->faultRevision;
    TEST_ASSERT_TRUE(
        service.requestControlledSafetyRestart(recoveryId, recoveryRevision) ==
        SafetyServiceStatus::Ready);
    TEST_ASSERT_EQUAL_UINT32(1U, reset.restartRequestCount());
    const auto* retained = service.faultCore().find(recoveryId);
    TEST_ASSERT_NOT_NULL(retained);
    TEST_ASSERT_TRUE(retained->causeActive);
    TEST_ASSERT_TRUE(retained->automaticRecoveryRestartUsed);
    TEST_ASSERT_TRUE(service.requestControlledSafetyRestart(
                         recoveryId, retained->faultRevision) ==
                     SafetyServiceStatus::SafetyRejected);
}

void test_stability_window_is_derived_from_central_safety_state() {
    SimulatedPersistentStateStore store;
    device_platform_test_support::SimulatedResetController reset;
    device_platform::VirtualTimeSource time;
    SafetyFaultService service(store, reset, time);
    TEST_ASSERT_TRUE(service.begin({true, true, true}) ==
                     SafetyServiceStatus::Ready);
    reset.setBootReset(device_platform::ResetCause::Brownout, true, 1U);
    TEST_ASSERT_TRUE(service.evaluateBoot().restart.status ==
                     RestartBootStatus::AbnormalRecorded);
    qualifyConfiguration(service);
    TEST_ASSERT_TRUE(service.advanceStableWindow() ==
                     SafetyServiceStatus::Ready);
    time.advanceMonotonicMillis(kStableRestartWindowMillis - 1U);
    TEST_ASSERT_TRUE(service.advanceStableWindow() ==
                     SafetyServiceStatus::Ready);
    TEST_ASSERT_TRUE(service.record().restartEpisode.stableWindowRunning);
    time.advanceMonotonicMillis(1U);
    TEST_ASSERT_TRUE(service.advanceStableWindow() ==
                     SafetyServiceStatus::Ready);
    TEST_ASSERT_FALSE(service.record().restartEpisode.stableWindowRunning);
    time.advanceMonotonicMillis(1U);
    TEST_ASSERT_TRUE(service.advanceStableWindow() ==
                     SafetyServiceStatus::Ready);
    TEST_ASSERT_FALSE(service.record().restartEpisode.stableWindowRunning);

    TEST_ASSERT_TRUE(
        service.raiseFault({FaultCode::S3_004, 24U, 1U, 1U, std::nullopt}) ==
        SafetyServiceStatus::Ready);
    TEST_ASSERT_TRUE(service.advanceStableWindow() ==
                     SafetyServiceStatus::Ready);
    TEST_ASSERT_FALSE(service.record().restartEpisode.stableWindowRunning);
}

void test_cleared_history_reuses_active_capacity_but_not_instance_ids() {
    SimulatedPersistentStateStore store;
    device_platform_test_support::SimulatedResetController reset;
    device_platform::VirtualTimeSource time;
    SafetyFaultService service(store, reset, time);
    TEST_ASSERT_TRUE(service.begin({true, true, true}) ==
                     SafetyServiceStatus::Ready);
    qualifyConfiguration(service);
    FaultInstanceId last;
    for (std::uint32_t index = 0U; index < 25U; ++index) {
        TEST_ASSERT_TRUE(service.raiseFault({FaultCode::S3_003, 24U, index + 1U,
                                             index + 1U, std::nullopt}) ==
                         SafetyServiceStatus::Ready);
        const auto* fault = service.faultCore().dominant();
        TEST_ASSERT_NOT_NULL(fault);
        last = fault->instanceId;
        TEST_ASSERT_TRUE(service.clearFaultCause(last, fault->faultRevision) ==
                         SafetyServiceStatus::Ready);
        fault = service.faultCore().find(last);
        TEST_ASSERT_NOT_NULL(fault);
        const auto request = resetRequestFor(last, fault->faultRevision);
        TEST_ASSERT_TRUE(
            service
                .resetFault(request,
                            authorizationFor(last, fault->faultRevision),
                            passedResetChecks())
                .status == SafetyServiceStatus::ResetCommitted);
        TEST_ASSERT_EQUAL_UINT32(0U, service.faultCore().snapshot().count);
    }
    TEST_ASSERT_TRUE(last.value > 20U);
    TEST_ASSERT_FALSE(service.record().capacityFailureLatched);
}

void test_configuration_forwarding_reuses_producer_correlation() {
    ConfigurationSafetyFixture fixture;
    device_platform_test_support::SimulatedResetController reset;
    device_platform::VirtualTimeSource time;
    SafetyFaultService service(fixture.store, reset, time);
    TEST_ASSERT_TRUE(service.begin({true, true, true}) ==
                     SafetyServiceStatus::Ready);
    ConfigurationSafetyIntegrationGate gate(*fixture.recovery, service);
    for (std::size_t index = 0U; index < 100U; ++index) {
        static_cast<void>(gate.forward(ConfigurationRecoveryResult{
            ConfigurationRecoveryStatus::ConfigurationUnavailable, {}}));
    }
    TEST_ASSERT_EQUAL_UINT32(1U, service.faultCore().snapshot().count);
    TEST_ASSERT_TRUE(service.faultCore().dominant()->code == FaultCode::Y4_003);
    TEST_ASSERT_TRUE(
        gate.forward(ConfigurationCommitResult{
            ConfigurationCommitStatus::ConfigurationRuntimeFailure}) ==
        SafetyServiceStatus::Ready);
    TEST_ASSERT_TRUE(
        gate.forward(ConfigurationCommitResult{
            ConfigurationCommitStatus::ConfigurationCommitIndeterminate}) ==
        SafetyServiceStatus::Ready);
    static_cast<void>(gate.forward(ConfigurationRecoveryResult{
        ConfigurationRecoveryStatus::ConfigurationIntegrityFailure, {}}));
    TEST_ASSERT_EQUAL_UINT32(4U, service.faultCore().snapshot().count);
}

void test_safe_boot_exit_is_authorized_after_configuration_qualification() {
    SimulatedPersistentStateStore store;
    SafetyStateStore seedStore(store);
    SafetyStateRecord seed;
    seed.safeBootRequired = true;
    TEST_ASSERT_TRUE(seedStore.commit(seed).status ==
                     SafetyRecordCommitStatus::Committed);
    device_platform_test_support::SimulatedResetController reset;
    device_platform::VirtualTimeSource time;
    SafetyFaultService service(store, reset, time);
    TEST_ASSERT_TRUE(service.begin() == SafetyServiceStatus::Ready);

    FaultResetAuthorizationEvidence authorization;
    authorization.evidenceId = 700U;
    authorization.level = FaultResetAuthorizationLevel::Technical;
    authorization.expiresAtMonotonicMillis = 10U;
    TEST_ASSERT_TRUE(service.requestAuthorizedSafeBootExit(authorization) ==
                     SafetyServiceStatus::Ready);
    reset.setBootReset(device_platform::ResetCause::SoftwareRestart, true, 2U);
    const auto boot = service.evaluateBoot();
    TEST_ASSERT_TRUE(boot.restart.status == RestartBootStatus::AuthorizedReset);
    TEST_ASSERT_TRUE(service.safeBootRequired());
    TEST_ASSERT_TRUE(service.consumeConfigurationStatus(
                         ConfigurationSafetyStatus::Operational, 56U, 1U) ==
                     SafetyServiceStatus::Ready);
    TEST_ASSERT_FALSE(service.safeBootRequired());
    TEST_ASSERT_TRUE(service.actuatorGateInput().status ==
                     ActuatorSafetyGateStatus::Allowed);

    const auto repeated = service.evaluateBoot();
    TEST_ASSERT_TRUE(repeated.restart.status ==
                     RestartBootStatus::ControlledEvidenceConsumed);
    TEST_ASSERT_FALSE(service.safeBootRequired());
    TEST_ASSERT_EQUAL_UINT32(1U, reset.restartRequestCount());
}

void test_third_abnormal_restart_reaches_authorized_safe_boot_exit() {
    SimulatedPersistentStateStore store;
    device_platform_test_support::SimulatedResetController reset;
    device_platform::VirtualTimeSource time;
    SafetyFaultService service(store, reset, time);
    TEST_ASSERT_TRUE(service.begin({true, true, true}) ==
                     SafetyServiceStatus::Ready);

    reset.setBootReset(device_platform::ResetCause::Brownout, true, 1U);
    TEST_ASSERT_TRUE(service.evaluateBoot().restart.status ==
                     RestartBootStatus::AbnormalRecorded);
    reset.setBootReset(device_platform::ResetCause::ExternalOrOther, true, 2U);
    TEST_ASSERT_TRUE(service.evaluateBoot().restart.status ==
                     RestartBootStatus::AbnormalRecorded);
    reset.setBootReset(device_platform::ResetCause::WatchdogOrPanic, true, 3U);
    TEST_ASSERT_TRUE(service.evaluateBoot().restart.status ==
                     RestartBootStatus::SafeBootRequired);
    TEST_ASSERT_TRUE(service.safeBootRequired());
    TEST_ASSERT_TRUE(service.faultCore().dominant()->code == FaultCode::Y4_009);

    FaultResetAuthorizationEvidence authorization;
    authorization.evidenceId = 701U;
    authorization.level = FaultResetAuthorizationLevel::Technical;
    authorization.expiresAtMonotonicMillis = 10U;
    TEST_ASSERT_TRUE(service.requestAuthorizedSafeBootExit(authorization) ==
                     SafetyServiceStatus::Ready);
    reset.setBootReset(device_platform::ResetCause::SoftwareRestart, true, 4U);
    TEST_ASSERT_TRUE(service.evaluateBoot().restart.status ==
                     RestartBootStatus::AuthorizedReset);
    TEST_ASSERT_TRUE(service.safeBootRequired());
    TEST_ASSERT_TRUE(service.consumeConfigurationStatus(
                         ConfigurationSafetyStatus::Operational, 56U, 1U) ==
                     SafetyServiceStatus::Ready);
    TEST_ASSERT_FALSE(service.safeBootRequired());
    TEST_ASSERT_EQUAL_UINT32(0U, service.faultCore().snapshot().count);
    TEST_ASSERT_TRUE(service.actuatorGateInput().status ==
                     ActuatorSafetyGateStatus::Allowed);
    TEST_ASSERT_EQUAL_UINT32(1U, reset.restartRequestCount());
}

void test_normal_restart_and_missing_config_do_not_exit_safe_boot() {
    SimulatedPersistentStateStore store;
    SafetyStateStore seedStore(store);
    SafetyStateRecord seed;
    seed.safeBootRequired = true;
    TEST_ASSERT_TRUE(seedStore.commit(seed).status ==
                     SafetyRecordCommitStatus::Committed);
    device_platform_test_support::SimulatedResetController reset;
    device_platform::VirtualTimeSource time;
    SafetyFaultService service(store, reset, time);
    TEST_ASSERT_TRUE(service.begin() == SafetyServiceStatus::Ready);
    reset.setBootReset(device_platform::ResetCause::SoftwareRestart, true, 2U);
    const auto boot = service.evaluateBoot();
    TEST_ASSERT_TRUE(boot.restart.status ==
                     RestartBootStatus::EvidenceMismatch);
    TEST_ASSERT_TRUE(service.safeBootRequired());
    TEST_ASSERT_TRUE(service.consumeConfigurationStatus(
                         ConfigurationSafetyStatus::Operational, 56U, 1U) ==
                     SafetyServiceStatus::Ready);
    TEST_ASSERT_TRUE(service.safeBootRequired());
}

void test_real_application_boundary_consumes_public_configuration_result() {
    ConfigurationSafetyFixture fixture;
    device_platform_test_support::SimulatedResetController reset;
    reset.setBootReset(device_platform::ResetCause::PowerOn, true, 1U);
    device_platform::VirtualTimeSource time;
    device_platform::DevicePlatform platform;
    TEST_ASSERT_TRUE(platform.begin({true}));

    fermentation::FermentationApplication application;
    fermentation::FermentationApplication::SafetyDependencies dependencies{
        fixture.store, reset, time, *fixture.recovery, nullptr};
    TEST_ASSERT_TRUE(
        application.begin(platform, dependencies, {true, true, true}));
    TEST_ASSERT_TRUE(application.ready());
    TEST_ASSERT_TRUE(application.actuatorSafetyGateInput().status ==
                     ActuatorSafetyGateStatus::Allowed);

    TEST_ASSERT_TRUE(application.forward(ConfigurationCommitResult{
                         ConfigurationCommitStatus::Activated}) ==
                     SafetyServiceStatus::Ready);
    TEST_ASSERT_TRUE(application.actuatorSafetyGateInput().status ==
                     ActuatorSafetyGateStatus::Allowed);

    TEST_ASSERT_TRUE(application.forward(ConfigurationRecoveryResult{
                         ConfigurationRecoveryStatus::ConfigurationUnavailable,
                         {}}) == SafetyServiceStatus::Ready);
    TEST_ASSERT_TRUE(application.actuatorSafetyGateInput().status ==
                     ActuatorSafetyGateStatus::ImmediateStop);
    TEST_ASSERT_TRUE(
        application.forward(ConfigurationRecoveryResult{
            ConfigurationRecoveryStatus::ConfigurationCommitIndeterminate,
            {}}) == SafetyServiceStatus::Ready);
    TEST_ASSERT_TRUE(
        application.forward(ConfigurationRecoveryResult{
            ConfigurationRecoveryStatus::ConfigurationIntegrityFailure, {}}) ==
        SafetyServiceStatus::Ready);
}

}  // namespace

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_r3_code_matrix_and_unknown_are_fail_closed);
    RUN_TEST(test_r3_record_is_fixed_1253_bytes_and_has_17_slots);
    RUN_TEST(test_neutral_reset_port_has_stable_observation_and_result);
    RUN_TEST(test_restart_episode_counts_once_third_and_stable_window);
    RUN_TEST(test_s3_003_is_explicit_injection_not_thermal_enum);
    RUN_TEST(test_air_limit_is_normal_control_state_without_fault);
    RUN_TEST(test_real_p1_and_sensor_quality_producers_keep_o2_without_o2_003);
    RUN_TEST(
        test_s3_004_is_contract_only_and_capacity_uses_marker_without_evict);
    RUN_TEST(test_safety_write_failure_keeps_ram_locked);
    RUN_TEST(
        test_safety_commit_outcomes_are_fail_closed_and_exact_unknown_is_confirmed);
    RUN_TEST(test_contract_injection_codes_and_journal_failure_remain_distinct);
    RUN_TEST(test_fault_reset_evaluation_requires_auth_and_all_safety_checks);
    RUN_TEST(test_fault_reset_rejects_stale_revision_and_other_blocking_fault);
    RUN_TEST(test_o2_and_p1_lifecycle_uses_producer_specific_resolution);
    RUN_TEST(
        test_automatic_restart_is_code_policy_bounded_and_once_per_episode);
    RUN_TEST(test_stability_window_is_derived_from_central_safety_state);
    RUN_TEST(test_cleared_history_reuses_active_capacity_but_not_instance_ids);
    RUN_TEST(test_configuration_forwarding_reuses_producer_correlation);
    RUN_TEST(
        test_safe_boot_exit_is_authorized_after_configuration_qualification);
    RUN_TEST(test_third_abnormal_restart_reaches_authorized_safe_boot_exit);
    RUN_TEST(test_normal_restart_and_missing_config_do_not_exit_safe_boot);
    RUN_TEST(
        test_real_application_boundary_consumes_public_configuration_result);
    return UNITY_END();
}
