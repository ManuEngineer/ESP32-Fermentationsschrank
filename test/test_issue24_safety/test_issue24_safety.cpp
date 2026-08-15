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

extern "C" void setUp(void) {}
extern "C" void tearDown(void) {}

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

void passedResetChecks(SafetyFaultService& service, FaultInstanceId id,
                       std::uint32_t revision) {
    service.injectResetSafetyEvidenceForTesting(id, revision, 0x0FU);
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

SafetyMarkerRecoveryEvidence passedMarkerRecoveryEvidence(
    const SafetyFaultService& service) {
    SafetyMarkerRecoveryEvidence evidence;
    evidence.markerRevision = service.record().capacityFailureRevision;
    evidence.evidenceRevision = service.record().recordRevision;
    evidence.read = FaultResetCheckStatus::Passed;
    evidence.write = FaultResetCheckStatus::Passed;
    evidence.capacity = FaultResetCheckStatus::Passed;
    evidence.integrity = FaultResetCheckStatus::Passed;
    evidence.readEvidenceRevision = service.record().recordRevision;
    evidence.writeEvidenceRevision = service.record().recordRevision;
    evidence.capacityEvidenceRevision = service.record().recordRevision;
    evidence.integrityEvidenceRevision = service.record().recordRevision;
    return evidence;
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

    const std::array<SafetyMarkerErrorKind, 7U> markerKinds = {
        SafetyMarkerErrorKind::Read,
        SafetyMarkerErrorKind::Write,
        SafetyMarkerErrorKind::Capacity,
        SafetyMarkerErrorKind::Integrity,
        SafetyMarkerErrorKind::ReadbackMismatch,
        SafetyMarkerErrorKind::CommitOutcomeUnknown,
        SafetyMarkerErrorKind::Unknown};
    for (const auto kind : markerKinds) {
        SafetyStateRecord marker;
        marker.safeBootRequired = true;
        marker.capacityFailureLatched = true;
        marker.capacityFailureRevision = 1U;
        marker.capacityFailureKind = kind;
        TEST_ASSERT_TRUE(encodeSafetyStateRecord(marker, encoded) ==
                         SafetyRecordEncodeStatus::Success);
        SafetyStateRecord decodedMarker;
        TEST_ASSERT_TRUE(decodeSafetyStateRecord(encoded, decodedMarker) ==
                         SafetyRecordDecodeStatus::Success);
        TEST_ASSERT_TRUE(decodedMarker.capacityFailureKind == kind);
    }
    SafetyStateRecord invalidMarker;
    invalidMarker.safeBootRequired = true;
    invalidMarker.capacityFailureLatched = true;
    invalidMarker.capacityFailureRevision = 1U;
    invalidMarker.capacityFailureKind = static_cast<SafetyMarkerErrorKind>(7U);
    TEST_ASSERT_TRUE(encodeSafetyStateRecord(invalidMarker, encoded) ==
                     SafetyRecordEncodeStatus::InvalidRecord);
    invalidMarker.capacityFailureKind = SafetyMarkerErrorKind::None;
    TEST_ASSERT_TRUE(encodeSafetyStateRecord(invalidMarker, encoded) ==
                     SafetyRecordEncodeStatus::InvalidRecord);
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
    TEST_ASSERT_TRUE(service.injectFaultForTesting(
                         {FaultCode::S3_003, 24U, 1U, 1U, std::nullopt}) ==
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
    TEST_ASSERT_TRUE(service.consumeConfigurationStatus(
                         ConfigurationSafetyStatus::Operational, 56U, 1U) ==
                     SafetyServiceStatus::Ready);
    passedResetChecks(service, faultId, cleared->faultRevision);
    TEST_ASSERT_TRUE(service.resetFault(resetRequest, authorization).status ==
                     SafetyServiceStatus::ResetCommitted);
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
    TEST_ASSERT_TRUE(service.injectFaultForTesting(
                         {FaultCode::S3_004, 24U, 1U, 1U, std::nullopt}) ==
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
        TEST_ASSERT_TRUE(capacity.injectFaultForTesting(
                             {FaultCode::S3_001, 100U + index, index + 1U,
                              index + 1U, std::nullopt}) ==
                         SafetyServiceStatus::Ready);
    }
    TEST_ASSERT_TRUE(capacity.injectFaultForTesting(
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
    TEST_ASSERT_TRUE(service.injectFaultForTesting(
                         {FaultCode::S3_003, 24U, 2U, 1U, std::nullopt}) ==
                     SafetyServiceStatus::PersistentWriteFailed);
    TEST_ASSERT_TRUE(service.safeBootRequired());
    TEST_ASSERT_TRUE(service.record().capacityFailureLatched);
    TEST_ASSERT_EQUAL_UINT32(1U, service.record().latchCount);
    TEST_ASSERT_TRUE(service.record().latches[0].code == FaultCode::S3_003);
    TEST_ASSERT_TRUE(service.actuatorGateInput().status ==
                     ActuatorSafetyGateStatus::ImmediateStop);

    TEST_ASSERT_TRUE(service.injectFaultForTesting(
                         {FaultCode::S3_005, 24U, 3U, 2U, std::nullopt}) ==
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
        TEST_ASSERT_TRUE(service.injectFaultForTesting(
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
        TEST_ASSERT_TRUE(service.injectFaultForTesting(
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
        TEST_ASSERT_TRUE(service.injectFaultForTesting(
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
        TEST_ASSERT_TRUE(service.injectFaultForTesting(
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
        TEST_ASSERT_TRUE(service.injectFaultForTesting(
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
        TEST_ASSERT_TRUE(service.injectFaultForTesting(
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
        TEST_ASSERT_TRUE(service.injectFaultForTesting(
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

void test_y4_006_requires_controlled_marker_recovery() {
    SimulatedPersistentStateStore store;
    device_platform_test_support::SimulatedResetController reset;
    device_platform::VirtualTimeSource time;
    device_platform_test_support::MockEventJournal journal;
    SafetyFaultService service(store, reset, time, &journal);
    TEST_ASSERT_TRUE(service.begin({true, true, true}) ==
                     SafetyServiceStatus::Ready);
    qualifyConfiguration(service);

    store.setNextWriteFault(
        SimulatedPersistentStateStore::WriteFault::FailBeforeBegin);
    TEST_ASSERT_TRUE(service.injectFaultForTesting(
                         {FaultCode::S3_003, 24U, 1U, 1U, std::nullopt}) ==
                     SafetyServiceStatus::PersistentWriteFailed);
    TEST_ASSERT_TRUE(service.record().capacityFailureLatched);
    TEST_ASSERT_TRUE(service.actuatorGateInput().status ==
                     ActuatorSafetyGateStatus::ImmediateStop);

    FaultResetAuthorizationEvidence markerAuthorization;
    markerAuthorization.evidenceId = 800U;
    markerAuthorization.level = FaultResetAuthorizationLevel::Technical;
    markerAuthorization.expiresAtMonotonicMillis = 100U;
    auto markerEvidence = passedMarkerRecoveryEvidence(service);
    TEST_ASSERT_TRUE(service.recoverSafetyStateMarker({}, markerEvidence) ==
                     SafetyServiceStatus::SafetyRejected);
    markerEvidence.read = FaultResetCheckStatus::Unknown;
    TEST_ASSERT_TRUE(
        service.recoverSafetyStateMarker(markerAuthorization, markerEvidence) ==
        SafetyServiceStatus::SafetyRejected);
    bool recoveryRejectedJournaled = false;
    for (const auto& entry : journal.entries()) {
        recoveryRejectedJournaled =
            recoveryRejectedJournaled ||
            entry.message.find("type=SafetyRecoveryAborted") !=
                std::string::npos;
    }
    TEST_ASSERT_TRUE(recoveryRejectedJournaled);

    const auto* fault = service.faultCore().dominant();
    TEST_ASSERT_NOT_NULL(fault);
    const auto faultId = fault->instanceId;
    const auto faultRevision = fault->faultRevision;
    TEST_ASSERT_TRUE(service.clearFaultCause(faultId, faultRevision) ==
                     SafetyServiceStatus::Ready);
    TEST_ASSERT_TRUE(service.record().capacityFailureLatched);
    TEST_ASSERT_TRUE(service.actuatorGateInput().status ==
                     ActuatorSafetyGateStatus::ImmediateStop);
    fault = service.faultCore().find(faultId);
    TEST_ASSERT_NOT_NULL(fault);
    const auto clearedRevision = fault->faultRevision;
    passedResetChecks(service, faultId, clearedRevision);
    TEST_ASSERT_TRUE(service
                         .resetFault(resetRequestFor(faultId, clearedRevision),
                                     authorizationFor(faultId, clearedRevision))
                         .status == SafetyServiceStatus::ResetCommitted);
    // The successful fault reset is not a Y4-006 recovery operation.
    TEST_ASSERT_TRUE(service.record().capacityFailureLatched);
    TEST_ASSERT_TRUE(service.actuatorGateInput().status ==
                     ActuatorSafetyGateStatus::ImmediateStop);

    TEST_ASSERT_TRUE(service.injectFaultForTesting(
                         {FaultCode::S3_004, 24U, 2U, 2U, std::nullopt}) ==
                     SafetyServiceStatus::Ready);
    markerEvidence = passedMarkerRecoveryEvidence(service);
    TEST_ASSERT_TRUE(
        service.recoverSafetyStateMarker(markerAuthorization, markerEvidence) ==
        SafetyServiceStatus::SafetyRejected);
    const auto* other = service.faultCore().dominant();
    TEST_ASSERT_NOT_NULL(other);
    const auto otherId = other->instanceId;
    const auto otherRevision = other->faultRevision;
    TEST_ASSERT_TRUE(service.clearFaultCause(otherId, otherRevision) ==
                     SafetyServiceStatus::Ready);
    other = service.faultCore().find(otherId);
    TEST_ASSERT_NOT_NULL(other);
    passedResetChecks(service, otherId, other->faultRevision);
    TEST_ASSERT_TRUE(
        service
            .resetFault(resetRequestFor(otherId, other->faultRevision),
                        authorizationFor(otherId, other->faultRevision))
            .status == SafetyServiceStatus::ResetCommitted);

    markerEvidence = passedMarkerRecoveryEvidence(service);
    TEST_ASSERT_TRUE(
        service.recoverSafetyStateMarker(markerAuthorization, markerEvidence) ==
        SafetyServiceStatus::SafetyMarkerRecoveryCommitted);
    TEST_ASSERT_FALSE(service.record().capacityFailureLatched);
    TEST_ASSERT_FALSE(service.safeBootRequired());
    TEST_ASSERT_TRUE(service.actuatorGateInput().status ==
                     ActuatorSafetyGateStatus::Allowed);
    bool recoveryJournaled = false;
    for (const auto& entry : journal.entries()) {
        recoveryJournaled =
            recoveryJournaled ||
            entry.message.find("type=SafetyRecoverySucceeded") !=
                std::string::npos;
    }
    TEST_ASSERT_TRUE(recoveryJournaled);

    store.setNextWriteFault(
        SimulatedPersistentStateStore::WriteFault::FailBeforeBegin);
    TEST_ASSERT_TRUE(service.injectFaultForTesting(
                         {FaultCode::S3_003, 24U, 3U, 3U, std::nullopt}) ==
                     SafetyServiceStatus::PersistentWriteFailed);
    const auto* repeatedFault = service.faultCore().dominant();
    TEST_ASSERT_NOT_NULL(repeatedFault);
    const auto repeatedId = repeatedFault->instanceId;
    TEST_ASSERT_TRUE(
        service.clearFaultCause(repeatedId, repeatedFault->faultRevision) ==
        SafetyServiceStatus::Ready);
    repeatedFault = service.faultCore().find(repeatedId);
    TEST_ASSERT_NOT_NULL(repeatedFault);
    passedResetChecks(service, repeatedId, repeatedFault->faultRevision);
    TEST_ASSERT_TRUE(
        service
            .resetFault(
                resetRequestFor(repeatedId, repeatedFault->faultRevision),
                authorizationFor(repeatedId, repeatedFault->faultRevision))
            .status == SafetyServiceStatus::ResetCommitted);
    const auto journalEntriesBeforeFailure = journal.entries().size();
    journal.injectWriteFailure(true);
    markerEvidence = passedMarkerRecoveryEvidence(service);
    TEST_ASSERT_TRUE(
        service.recoverSafetyStateMarker(markerAuthorization, markerEvidence) ==
        SafetyServiceStatus::SafetyMarkerRecoveryCommitted);
    TEST_ASSERT_FALSE(service.record().capacityFailureLatched);
    TEST_ASSERT_EQUAL_UINT32(journalEntriesBeforeFailure,
                             journal.entries().size());
    TEST_ASSERT_EQUAL_UINT32(0U, reset.restartRequestCount());
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
        TEST_ASSERT_TRUE(service.injectFaultForTesting(
                             {codes[index], 24U, index + 1U, index + 1U,
                              std::nullopt}) == SafetyServiceStatus::Ready);
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
    TEST_ASSERT_TRUE(service.injectFaultForTesting(
                         {FaultCode::S3_003, 24U, 1U, 1U, std::nullopt}) ==
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
    passedResetChecks(service, id, revision);

    auto missing = service.evaluateFaultReset(request, {});
    TEST_ASSERT_FALSE(missing.allowed);
    TEST_ASSERT_TRUE(missing.rejection ==
                     FaultResetRejection::AuthorizationMissing);

    const auto lowAuthorization =
        authorizationFor(id, revision, FaultResetAuthorizationLevel::Operator);
    passedResetChecks(service, id, revision);
    auto low = service.evaluateFaultReset(request, lowAuthorization);
    TEST_ASSERT_FALSE(low.allowed);
    TEST_ASSERT_TRUE(low.rejection ==
                     FaultResetRejection::AuthorizationMissing);

    service.injectResetSafetyEvidenceForTesting(id, revision, 0x0EU);
    auto sensor =
        service.evaluateFaultReset(request, authorizationFor(id, revision));
    TEST_ASSERT_FALSE(sensor.allowed);
    TEST_ASSERT_FALSE(sensor.safetyChecksPassed);

    service.injectResetSafetyEvidenceForTesting(id, revision, 0x0DU);
    TEST_ASSERT_FALSE(
        service.evaluateFaultReset(request, authorizationFor(id, revision))
            .allowed);
    service.injectResetSafetyEvidenceForTesting(id, revision, 0x0BU);
    TEST_ASSERT_FALSE(
        service.evaluateFaultReset(request, authorizationFor(id, revision))
            .allowed);
    service.injectResetSafetyEvidenceForTesting(id, revision, 0x07U);
    TEST_ASSERT_FALSE(
        service.evaluateFaultReset(request, authorizationFor(id, revision))
            .allowed);
    service.injectResetSafetyEvidenceForTesting(FaultInstanceId{999U}, revision,
                                                0x0FU);
    TEST_ASSERT_FALSE(
        service.evaluateFaultReset(request, authorizationFor(id, revision))
            .allowed);
    passedResetChecks(service, id, revision);
    service.invalidateSafetyEvidenceForTesting(FaultResetCheckDomain::Sensor);
    TEST_ASSERT_FALSE(
        service.evaluateFaultReset(request, authorizationFor(id, revision))
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
    passedResetChecks(service, id, acknowledgedRevision);
    const auto committed =
        service.resetFault(acknowledgedRequest, acknowledgedAuthorization);
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
    TEST_ASSERT_TRUE(service.injectFaultForTesting(
                         {FaultCode::S3_003, 24U, 1U, 1U, std::nullopt}) ==
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
    passedResetChecks(service, targetId, revision);
    const auto stale =
        service.evaluateFaultReset(staleRequest, staleAuthorization);
    TEST_ASSERT_TRUE(stale.rejection == FaultResetRejection::StaleEvaluation);

    TEST_ASSERT_TRUE(service.injectFaultForTesting(
                         {FaultCode::S3_004, 24U, 2U, 2U, std::nullopt}) ==
                     SafetyServiceStatus::Ready);
    passedResetChecks(service, targetId, revision);
    const auto blocked =
        service.evaluateFaultReset(resetRequestFor(targetId, revision),
                                   authorizationFor(targetId, revision));
    TEST_ASSERT_FALSE(blocked.allowed);
    TEST_ASSERT_TRUE(blocked.rejection ==
                     FaultResetRejection::OtherActiveFault);
}

void test_real_sensor_producer_evidence_can_reset_matching_fault() {
    SimulatedPersistentStateStore store;
    device_platform_test_support::SimulatedResetController reset;
    device_platform::VirtualTimeSource time;
    SafetyFaultService service(store, reset, time);
    TEST_ASSERT_TRUE(service.begin({true, true, true}) ==
                     SafetyServiceStatus::Ready);
    qualifyConfiguration(service);

    device_platform::SensorQualitySnapshot failed;
    failed.quality = device_platform::SensorQuality::Failed;
    TEST_ASSERT_TRUE(service.consumeSensorQuality(SafetySensorRole::CabinetAir,
                                                  failed, 30U, 9001U) ==
                     SafetyServiceStatus::Ready);
    const auto* target = service.faultCore().dominant();
    TEST_ASSERT_NOT_NULL(target);
    TEST_ASSERT_TRUE(target->code == FaultCode::S3_001);
    const auto targetId = target->instanceId;
    TEST_ASSERT_TRUE(service.clearFaultCause(targetId, target->faultRevision) ==
                     SafetyServiceStatus::Ready);

    device_platform::SensorQualitySnapshot valid;
    valid.quality = device_platform::SensorQuality::Valid;
    TEST_ASSERT_TRUE(service.consumeSensorQuality(SafetySensorRole::CabinetAir,
                                                  valid, 30U, 123456U) ==
                     SafetyServiceStatus::Ready);
    target = service.faultCore().find(targetId);
    TEST_ASSERT_NOT_NULL(target);
    const auto request = resetRequestFor(targetId, target->faultRevision);
    const auto evaluation = service.evaluateFaultReset(
        request, authorizationFor(targetId, target->faultRevision));
    TEST_ASSERT_TRUE(evaluation.allowed);
    TEST_ASSERT_TRUE(
        service
            .resetFault(request,
                        authorizationFor(targetId, target->faultRevision))
            .status == SafetyServiceStatus::ResetCommitted);
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
    TEST_ASSERT_TRUE(service.consumeSensorQuality(SafetySensorRole::CabinetAir,
                                                  staleSafety, 22U, 10U) ==
                     SafetyServiceStatus::Ready);
    TEST_ASSERT_TRUE(service.consumeSensorQuality(SafetySensorRole::Cooling,
                                                  staleSafety, 22U, 20U) ==
                     SafetyServiceStatus::Ready);
    device_platform::SensorQualitySnapshot validSafety;
    validSafety.quality = device_platform::SensorQuality::Valid;
    TEST_ASSERT_TRUE(service.consumeSensorQuality(SafetySensorRole::CabinetAir,
                                                  validSafety, 22U, 10U) ==
                     SafetyServiceStatus::Ready);
    auto afterCabinetRecovery = service.faultCore().snapshot();
    TEST_ASSERT_EQUAL_UINT32(1U, afterCabinetRecovery.count);
    TEST_ASSERT_TRUE(afterCabinetRecovery.records[0].code == FaultCode::O2_002);
    TEST_ASSERT_TRUE(afterCabinetRecovery.records[0].correlationKey != 20U);
    TEST_ASSERT_TRUE(service.actuatorGateInput().status ==
                     ActuatorSafetyGateStatus::ImmediateStop);
    TEST_ASSERT_TRUE(service.consumeSensorQuality(SafetySensorRole::Cooling,
                                                  validSafety, 22U, 20U) ==
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

void test_sensor_role_identity_ignores_external_correlation_keys() {
    SimulatedPersistentStateStore store;
    device_platform_test_support::SimulatedResetController reset;
    device_platform::VirtualTimeSource time;
    SafetyFaultService service(store, reset, time);
    TEST_ASSERT_TRUE(service.begin({true, true, true}) ==
                     SafetyServiceStatus::Ready);

    device_platform::SensorQualitySnapshot stale;
    stale.quality = device_platform::SensorQuality::Stale;
    for (std::uint32_t key = 1U; key <= 100U; ++key) {
        TEST_ASSERT_TRUE(service.consumeSensorQuality(
                             SafetySensorRole::CabinetAir, stale, 22U, key) ==
                         SafetyServiceStatus::Ready);
    }
    TEST_ASSERT_EQUAL_UINT32(1U, service.faultCore().snapshot().count);
    TEST_ASSERT_TRUE(service.faultCore().dominant()->code == FaultCode::O2_002);

    for (std::uint32_t key = 101U; key <= 200U; ++key) {
        TEST_ASSERT_TRUE(service.consumeSensorQuality(SafetySensorRole::Cooling,
                                                      stale, 22U, key) ==
                         SafetyServiceStatus::Ready);
    }
    TEST_ASSERT_EQUAL_UINT32(2U, service.faultCore().snapshot().count);

    device_platform::SensorQualitySnapshot valid;
    valid.quality = device_platform::SensorQuality::Valid;
    TEST_ASSERT_TRUE(service.consumeSensorQuality(SafetySensorRole::CabinetAir,
                                                  valid, 22U, 9999U) ==
                     SafetyServiceStatus::Ready);
    TEST_ASSERT_EQUAL_UINT32(1U, service.faultCore().snapshot().count);

    device_platform::SensorQualitySnapshot failed;
    failed.quality = device_platform::SensorQuality::Failed;
    for (std::uint32_t key = 201U; key <= 300U; ++key) {
        TEST_ASSERT_TRUE(service.consumeSensorQuality(
                             SafetySensorRole::CabinetAir, failed, 22U, key) ==
                         SafetyServiceStatus::Ready);
    }
    TEST_ASSERT_EQUAL_UINT32(2U, service.faultCore().snapshot().count);
    TEST_ASSERT_TRUE(service.faultCore().dominant()->code == FaultCode::S3_001);
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
        TEST_ASSERT_TRUE(service.injectFaultForTesting(
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

    TEST_ASSERT_TRUE(service.injectFaultForTesting(
                         {FaultCode::Y4_007, 24U, 2U, 2U, std::nullopt}) ==
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

void test_restart_evidence_requires_current_fault_revision_and_episode_record() {
    {
        SimulatedPersistentStateStore store;
        device_platform_test_support::SimulatedResetController reset;
        device_platform::VirtualTimeSource time;
        SafetyFaultService service(store, reset, time);
        TEST_ASSERT_TRUE(service.begin({true, true, true}) ==
                         SafetyServiceStatus::Ready);
        TEST_ASSERT_TRUE(service.injectFaultForTesting(
                             {FaultCode::Y4_007, 24U, 1U, 1U, std::nullopt}) ==
                         SafetyServiceStatus::Ready);
        const auto* target = service.faultCore().dominant();
        TEST_ASSERT_NOT_NULL(target);
        TEST_ASSERT_TRUE(service.requestControlledSafetyRestart(
                             target->instanceId, target->faultRevision) ==
                         SafetyServiceStatus::Ready);

        reset.setBootReset(ResetCause::SoftwareRestart, true, 1U);
        SafetyFaultService restarted(store, reset, time);
        TEST_ASSERT_TRUE(restarted.begin() == SafetyServiceStatus::Ready);
        TEST_ASSERT_TRUE(restarted.evaluateBoot().restart.status ==
                         RestartBootStatus::ControlledEvidenceConsumed);
        TEST_ASSERT_FALSE(restarted.safeBootRequired());
    }

    const auto verifyEvidenceMismatch = [](bool removeTarget,
                                           bool staleEvidenceRevision) {
        SimulatedPersistentStateStore store;
        device_platform_test_support::SimulatedResetController reset;
        device_platform::VirtualTimeSource time;
        {
            SafetyFaultService service(store, reset, time);
            TEST_ASSERT_TRUE(service.begin({true, true, true}) ==
                             SafetyServiceStatus::Ready);
            TEST_ASSERT_TRUE(service.injectFaultForTesting(
                                 {FaultCode::Y4_007, 24U, 1U, 1U,
                                  std::nullopt}) == SafetyServiceStatus::Ready);
            const auto* target = service.faultCore().dominant();
            TEST_ASSERT_NOT_NULL(target);
            TEST_ASSERT_TRUE(service.requestControlledSafetyRestart(
                                 target->instanceId, target->faultRevision) ==
                             SafetyServiceStatus::Ready);
        }
        SafetyStateStore persisted(store);
        const auto loaded = persisted.load();
        TEST_ASSERT_TRUE(loaded.status == SafetyRecordLoadStatus::Loaded);
        auto tampered = loaded.record;
        const auto originalEvidenceRevision =
            tampered.restartEvidence.evidenceRevision;
        ++tampered.recordRevision;
        if (removeTarget) {
            // A nonzero but absent instance is wire-valid and represents a
            // target that no longer exists in the current FaultCore.
            tampered.restartEvidence.targetFault = {999U};
        } else {
            ++tampered.restartEvidence.targetFaultRevision;
        }
        if (!staleEvidenceRevision) {
            tampered.restartEvidence.evidenceRevision = tampered.recordRevision;
        } else {
            tampered.restartEvidence.evidenceRevision =
                originalEvidenceRevision;
        }
        TEST_ASSERT_TRUE(persisted.commit(tampered).status ==
                         SafetyRecordCommitStatus::Committed);

        reset.setBootReset(ResetCause::SoftwareRestart, true, 2U);
        SafetyFaultService restarted(store, reset, time);
        TEST_ASSERT_TRUE(restarted.begin() == SafetyServiceStatus::Ready);
        const auto boot = restarted.evaluateBoot();
        TEST_ASSERT_TRUE(boot.restart.status ==
                         RestartBootStatus::EvidenceMismatch);
        TEST_ASSERT_TRUE(restarted.safeBootRequired());
        bool evidenceFaultFound = false;
        const auto faults = restarted.faultCore().snapshot();
        for (std::size_t index = 0U; index < faults.count; ++index) {
            evidenceFaultFound =
                evidenceFaultFound ||
                faults.records[index].code == FaultCode::Y4_008;
        }
        TEST_ASSERT_TRUE(evidenceFaultFound);
    };
    verifyEvidenceMismatch(false, false);
    verifyEvidenceMismatch(true, false);
    verifyEvidenceMismatch(false, true);
}

void test_rejected_or_unknown_restart_evidence_cannot_authorize_later_boot() {
    {
        SimulatedPersistentStateStore store;
        device_platform_test_support::SimulatedResetController reset;
        device_platform::VirtualTimeSource time;
        SafetyFaultService service(store, reset, time);
        TEST_ASSERT_TRUE(service.begin({true, true, true}) ==
                         SafetyServiceStatus::Ready);
        TEST_ASSERT_TRUE(service.injectFaultForTesting(
                             {FaultCode::Y4_007, 24U, 1U, 1U, std::nullopt}) ==
                         SafetyServiceStatus::Ready);
        const auto* target = service.faultCore().dominant();
        TEST_ASSERT_NOT_NULL(target);
        reset.setNextRestartResult(RestartRequestResult::Rejected);
        TEST_ASSERT_TRUE(service.requestControlledSafetyRestart(
                             target->instanceId, target->faultRevision) ==
                         SafetyServiceStatus::ResetBootRejected);
        reset.setBootReset(ResetCause::SoftwareRestart, true, 1U);
        const auto boot = service.evaluateBoot();
        TEST_ASSERT_TRUE(boot.restart.status ==
                         RestartBootStatus::EvidenceMismatch);
        TEST_ASSERT_TRUE(service.safeBootRequired());
        TEST_ASSERT_EQUAL_UINT32(1U, reset.restartRequestCount());
    }
    {
        SimulatedPersistentStateStore store;
        device_platform_test_support::SimulatedResetController reset;
        device_platform::VirtualTimeSource time;
        SafetyFaultService service(store, reset, time);
        TEST_ASSERT_TRUE(service.begin({true, true, true}) ==
                         SafetyServiceStatus::Ready);
        TEST_ASSERT_TRUE(service.injectFaultForTesting(
                             {FaultCode::Y4_007, 24U, 1U, 1U, std::nullopt}) ==
                         SafetyServiceStatus::Ready);
        const auto* target = service.faultCore().dominant();
        TEST_ASSERT_NOT_NULL(target);
        reset.setNextRestartResult(RestartRequestResult::OutcomeUnknown);
        TEST_ASSERT_TRUE(service.requestControlledSafetyRestart(
                             target->instanceId, target->faultRevision) ==
                         SafetyServiceStatus::ResetBootOutcomeUnknown);
        TEST_ASSERT_TRUE(service.safeBootRequired());
        reset.setBootReset(ResetCause::SoftwareRestart, true, 1U);
        const auto boot = service.evaluateBoot();
        TEST_ASSERT_TRUE(boot.restart.status ==
                         RestartBootStatus::EvidenceMismatch);
        TEST_ASSERT_TRUE(service.safeBootRequired());
        TEST_ASSERT_EQUAL_UINT32(1U, reset.restartRequestCount());
    }
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

    TEST_ASSERT_TRUE(service.injectFaultForTesting(
                         {FaultCode::S3_004, 24U, 1U, 1U, std::nullopt}) ==
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
        TEST_ASSERT_TRUE(service.injectFaultForTesting(
                             {FaultCode::S3_003, 24U, index + 1U, index + 1U,
                              std::nullopt}) == SafetyServiceStatus::Ready);
        const auto* fault = service.faultCore().dominant();
        TEST_ASSERT_NOT_NULL(fault);
        last = fault->instanceId;
        TEST_ASSERT_TRUE(service.clearFaultCause(last, fault->faultRevision) ==
                         SafetyServiceStatus::Ready);
        fault = service.faultCore().find(last);
        TEST_ASSERT_NOT_NULL(fault);
        const auto request = resetRequestFor(last, fault->faultRevision);
        TEST_ASSERT_TRUE(
            (passedResetChecks(service, last, fault->faultRevision),
             service.resetFault(request,
                                authorizationFor(last, fault->faultRevision))
                     .status == SafetyServiceStatus::ResetCommitted));
        TEST_ASSERT_EQUAL_UINT32(0U, service.faultCore().snapshot().count);
    }
    TEST_ASSERT_TRUE(last.value > 20U);
    TEST_ASSERT_FALSE(service.record().capacityFailureLatched);
}

void test_active_capacity_includes_two_o2_safety_roles() {
    SimulatedPersistentStateStore store;
    device_platform_test_support::SimulatedResetController reset;
    device_platform::VirtualTimeSource time;
    SafetyFaultService service(store, reset, time);
    TEST_ASSERT_TRUE(service.begin({true, true, true}) ==
                     SafetyServiceStatus::Ready);

    for (std::uint32_t index = 0U; index < kMaximumPersistedLatches; ++index) {
        TEST_ASSERT_TRUE(service.injectFaultForTesting(
                             {FaultCode::S3_003, 100U + index, index + 1U,
                              index + 1U, std::nullopt}) ==
                         SafetyServiceStatus::Ready);
    }
    TEST_ASSERT_TRUE(service.consumeProcessMessage(
                         ProcessMessage::TargetReachTimeExceeded, 200U, 1U) ==
                     SafetyServiceStatus::Ready);

    device_platform::SensorQualitySnapshot stale;
    stale.quality = device_platform::SensorQuality::Stale;
    TEST_ASSERT_TRUE(service.consumeSensorQuality(SafetySensorRole::Product,
                                                  stale, 201U, 1U) ==
                     SafetyServiceStatus::Ready);
    TEST_ASSERT_TRUE(service.consumeSensorQuality(SafetySensorRole::CabinetAir,
                                                  stale, 202U, 2U) ==
                     SafetyServiceStatus::Ready);
    TEST_ASSERT_TRUE(service.consumeSensorQuality(SafetySensorRole::Cooling,
                                                  stale, 202U, 3U) ==
                     SafetyServiceStatus::Ready);

    TEST_ASSERT_EQUAL_UINT32(21U, service.faultCore().snapshot().count);
    TEST_ASSERT_FALSE(service.record().capacityFailureLatched);
    TEST_ASSERT_TRUE(service.actuatorGateInput().status ==
                     ActuatorSafetyGateStatus::ImmediateStop);
}

void test_bounded_watchdog_and_unknown_domains_reuse_identity() {
    SimulatedPersistentStateStore store;
    device_platform_test_support::SimulatedResetController reset;
    device_platform::VirtualTimeSource time;
    SafetyFaultService service(store, reset, time);
    TEST_ASSERT_TRUE(service.begin({true, true, true}) ==
                     SafetyServiceStatus::Ready);
    qualifyConfiguration(service);

    const ActuatorWatchdogFaultEvidence firstWatchdog{10U,
                                                      0x0000000100000001ULL};
    TEST_ASSERT_TRUE(service.consumeWatchdogEvidence(firstWatchdog) ==
                     SafetyServiceStatus::Ready);
    const auto* first = service.faultCore().dominant();
    TEST_ASSERT_NOT_NULL(first);
    TEST_ASSERT_TRUE(first->code == FaultCode::S3_008);
    const auto firstId = first->instanceId;
    const auto firstRevision = first->faultRevision;
    TEST_ASSERT_EQUAL_UINT64(
        firstWatchdog.lastObservedSequenceHighWatermarkBeforeFault,
        first->diagnosticSequenceHighWatermark);

    const ActuatorWatchdogFaultEvidence repeatedWatchdog{20U,
                                                         0xFEDCBA9876543210ULL};
    TEST_ASSERT_TRUE(service.consumeWatchdogEvidence(repeatedWatchdog) ==
                     SafetyServiceStatus::Ready);
    const auto watchdogs = service.faultCore().snapshot();
    TEST_ASSERT_EQUAL_UINT32(1U, watchdogs.count);
    TEST_ASSERT_TRUE(watchdogs.records[0].instanceId == firstId);
    TEST_ASSERT_TRUE(watchdogs.records[0].faultRevision > firstRevision);
    TEST_ASSERT_EQUAL_UINT64(
        repeatedWatchdog.lastObservedSequenceHighWatermarkBeforeFault,
        watchdogs.records[0].diagnosticSequenceHighWatermark);

    TEST_ASSERT_TRUE(
        service.clearFaultCause(firstId, watchdogs.records[0].faultRevision) ==
        SafetyServiceStatus::Ready);
    const auto* cleared = service.faultCore().find(firstId);
    TEST_ASSERT_NOT_NULL(cleared);
    passedResetChecks(service, firstId, cleared->faultRevision);
    TEST_ASSERT_TRUE(
        service
            .resetFault(
                resetRequestFor(firstId, cleared->faultRevision),
                authorizationFor(firstId, cleared->faultRevision,
                                 FaultResetAuthorizationLevel::Technical))
            .status == SafetyServiceStatus::ResetCommitted);
    TEST_ASSERT_EQUAL_UINT32(0U, service.faultCore().snapshot().count);

    const ActuatorWatchdogFaultEvidence afterClear{30U, 0xFFFFFFFF00000001ULL};
    TEST_ASSERT_TRUE(service.consumeWatchdogEvidence(afterClear) ==
                     SafetyServiceStatus::Ready);
    const auto* newWatchdog = service.faultCore().dominant();
    TEST_ASSERT_NOT_NULL(newWatchdog);
    TEST_ASSERT_TRUE(newWatchdog->instanceId != firstId);

    device_platform_test_support::MockEventJournal journal;
    SimulatedPersistentStateStore unknownStore;
    device_platform_test_support::SimulatedResetController unknownReset;
    device_platform::VirtualTimeSource unknownTime;
    SafetyFaultService unknown(unknownStore, unknownReset, unknownTime,
                               &journal);
    TEST_ASSERT_TRUE(unknown.begin({true, true, true}) ==
                     SafetyServiceStatus::Ready);
    unknownReset.setBootReset(ResetCause::SoftwareRestart, true, 1U);
    TEST_ASSERT_TRUE(unknown.evaluateBoot().restart.status ==
                     RestartBootStatus::EvidenceMismatch);
    TEST_ASSERT_TRUE(unknown.consumeConfigurationStatus(
                         ConfigurationSafetyStatus::Unknown, 56U, 900U) ==
                     SafetyServiceStatus::Ready);
    TEST_ASSERT_TRUE(unknown.consumeProcessMessage(
                         static_cast<ProcessMessage>(0xFFU), 22U, 901U) ==
                     SafetyServiceStatus::Ready);
    device_platform::SensorQualitySnapshot unknownQuality;
    unknownQuality.quality = static_cast<device_platform::SensorQuality>(0xFFU);
    TEST_ASSERT_TRUE(unknown.consumeSensorQuality(SafetySensorRole::Product,
                                                  unknownQuality, 20U, 902U) ==
                     SafetyServiceStatus::Ready);
    const auto unknownFaults = unknown.faultCore().snapshot();
    TEST_ASSERT_EQUAL_UINT32(1U, unknownFaults.count);
    TEST_ASSERT_TRUE(unknownFaults.records[0].code == FaultCode::Y4_008);
    bool escalationJournaled = false;
    for (const auto& entry : journal.entries()) {
        escalationJournaled =
            escalationJournaled ||
            entry.message.find("type=FaultEscalated;code=Y4-008") !=
                std::string::npos;
    }
    TEST_ASSERT_TRUE(escalationJournaled);

    const auto unknownId = unknownFaults.records[0].instanceId;
    // B4: a bare id+revision clear is rejected for Y4-008; only the matching
    // real clearance path (here: the sensor domain that most recently
    // reported the unknown quality) may resolve the cause.
    TEST_ASSERT_TRUE(unknown.clearFaultCause(
                         unknownId, unknownFaults.records[0].faultRevision) ==
                     SafetyServiceStatus::SafetyRejected);
    device_platform::SensorQualitySnapshot validProductQuality;
    validProductQuality.quality = device_platform::SensorQuality::Valid;
    TEST_ASSERT_TRUE(unknown.consumeSensorQuality(
                         SafetySensorRole::Product, validProductQuality, 20U,
                         902U) == SafetyServiceStatus::Ready);
    TEST_ASSERT_TRUE(unknown.consumeConfigurationStatus(
                         ConfigurationSafetyStatus::Operational, 56U, 903U) ==
                     SafetyServiceStatus::Ready);
    const auto* unknownCleared = unknown.faultCore().find(unknownId);
    TEST_ASSERT_NOT_NULL(unknownCleared);
    passedResetChecks(unknown, unknownId, unknownCleared->faultRevision);
    TEST_ASSERT_TRUE(
        unknown
            .resetFault(
                resetRequestFor(unknownId, unknownCleared->faultRevision),
                authorizationFor(unknownId, unknownCleared->faultRevision,
                                 FaultResetAuthorizationLevel::Technical))
            .status == SafetyServiceStatus::ResetCommitted);
    TEST_ASSERT_EQUAL_UINT32(0U, unknown.faultCore().snapshot().count);
    TEST_ASSERT_TRUE(unknown.injectFaultForTesting(
                         {FaultCode::Y4_008, 99U, 100U, 2U, std::nullopt}) ==
                     SafetyServiceStatus::Ready);
    TEST_ASSERT_TRUE(unknown.faultCore().dominant()->instanceId != unknownId);
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
    for (const auto status :
         {ConfigurationRecoveryStatus::PersistenceReadFailure,
          ConfigurationRecoveryStatus::PersistenceCapacityFailure,
          ConfigurationRecoveryStatus::PersistenceWriteFailure,
          ConfigurationRecoveryStatus::RuntimePreparationFailure}) {
        static_cast<void>(
            gate.forward(ConfigurationRecoveryResult{status, {}}));
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
    TEST_ASSERT_TRUE(
        gate
            .forward(ConfigurationRecoveryResult{
                ConfigurationRecoveryStatus::BootstrapCommitIndeterminate, {}})
            .safetyStatus == SafetyServiceStatus::Ready);
    TEST_ASSERT_TRUE(
        gate.forward(ConfigurationRecoveryResult{
                         ConfigurationRecoveryStatus::
                             ConfigurationRecordOutcomeIndeterminate,
                         {}})
            .safetyStatus == SafetyServiceStatus::Ready);
    static_cast<void>(gate.forward(ConfigurationRecoveryResult{
        ConfigurationRecoveryStatus::ConfigurationIntegrityFailure, {}}));
    static_cast<void>(gate.forward(ConfigurationRecoveryResult{
        ConfigurationRecoveryStatus::UnsupportedNewerConfigurationSchema, {}}));
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
                     SafetyServiceStatus::SafetyRejected);
    service.injectSafeBootSafetyEvidenceForTesting(0x0FU);
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
                     SafetyServiceStatus::SafetyRejected);
    service.injectSafeBootSafetyEvidenceForTesting(0x0FU);
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

void test_safe_boot_exit_requires_current_four_domain_evidence() {
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
    authorization.evidenceId = 702U;
    authorization.level = FaultResetAuthorizationLevel::Technical;
    authorization.expiresAtMonotonicMillis = 100U;
    TEST_ASSERT_TRUE(service.requestAuthorizedSafeBootExit(authorization) ==
                     SafetyServiceStatus::Ready);
    reset.setBootReset(device_platform::ResetCause::SoftwareRestart, true, 2U);
    TEST_ASSERT_TRUE(service.evaluateBoot().restart.status ==
                     RestartBootStatus::AuthorizedReset);
    TEST_ASSERT_TRUE(service.consumeConfigurationStatus(
                         ConfigurationSafetyStatus::Operational, 56U, 1U) ==
                     SafetyServiceStatus::SafetyRejected);

    for (const auto missing :
         {static_cast<std::uint8_t>(0x0EU), static_cast<std::uint8_t>(0x0DU),
          static_cast<std::uint8_t>(0x0BU), static_cast<std::uint8_t>(0x07U)}) {
        service.injectSafeBootSafetyEvidenceForTesting(missing);
        TEST_ASSERT_TRUE(service.safeBootRequired());
    }
    service.injectSafeBootSafetyEvidenceForTesting(0x0FU);
    TEST_ASSERT_FALSE(service.safeBootRequired());
}

void test_safe_boot_exit_requires_both_required_sensor_roles() {
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
    authorization.evidenceId = 703U;
    authorization.level = FaultResetAuthorizationLevel::Technical;
    authorization.expiresAtMonotonicMillis = 100U;
    TEST_ASSERT_TRUE(service.requestAuthorizedSafeBootExit(authorization) ==
                     SafetyServiceStatus::Ready);
    reset.setBootReset(device_platform::ResetCause::SoftwareRestart, true, 2U);
    TEST_ASSERT_TRUE(service.evaluateBoot().restart.status ==
                     RestartBootStatus::AuthorizedReset);
    TEST_ASSERT_TRUE(service.consumeConfigurationStatus(
                         ConfigurationSafetyStatus::Operational, 56U, 1U) ==
                     SafetyServiceStatus::SafetyRejected);

    // Actuator, persistence and integrity are passed synthetically; the real
    // sensor-role producers below must still qualify both mandatory roles.
    service.injectSafeBootSafetyEvidenceForTesting(0x0EU);
    device_platform::SensorQualitySnapshot valid;
    valid.quality = device_platform::SensorQuality::Valid;
    TEST_ASSERT_TRUE(service.consumeSensorQuality(SafetySensorRole::Cooling,
                                                  valid, 20U, 1U) ==
                     SafetyServiceStatus::Ready);
    TEST_ASSERT_TRUE(service.safeBootRequired());
    TEST_ASSERT_TRUE(service.consumeSensorQuality(SafetySensorRole::CabinetAir,
                                                  valid, 20U, 2U) ==
                     SafetyServiceStatus::Ready);
    TEST_ASSERT_TRUE(service.safeBootRequired());

    device_platform::SensorQualitySnapshot stale;
    stale.quality = device_platform::SensorQuality::Stale;
    TEST_ASSERT_TRUE(service.consumeSensorQuality(SafetySensorRole::Cooling,
                                                  stale, 20U, 3U) ==
                     SafetyServiceStatus::Ready);
    TEST_ASSERT_TRUE(service.consumeSensorQuality(SafetySensorRole::CabinetAir,
                                                  valid, 20U, 4U) ==
                     SafetyServiceStatus::Ready);
    TEST_ASSERT_TRUE(service.consumeSensorQuality(SafetySensorRole::Product,
                                                  valid, 20U, 5U) ==
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
    RUN_TEST(test_y4_006_requires_controlled_marker_recovery);
    RUN_TEST(
        test_safety_commit_outcomes_are_fail_closed_and_exact_unknown_is_confirmed);
    RUN_TEST(test_contract_injection_codes_and_journal_failure_remain_distinct);
    RUN_TEST(test_fault_reset_evaluation_requires_auth_and_all_safety_checks);
    RUN_TEST(test_fault_reset_rejects_stale_revision_and_other_blocking_fault);
    RUN_TEST(test_real_sensor_producer_evidence_can_reset_matching_fault);
    RUN_TEST(test_o2_and_p1_lifecycle_uses_producer_specific_resolution);
    RUN_TEST(test_sensor_role_identity_ignores_external_correlation_keys);
    RUN_TEST(
        test_automatic_restart_is_code_policy_bounded_and_once_per_episode);
    RUN_TEST(
        test_restart_evidence_requires_current_fault_revision_and_episode_record);
    RUN_TEST(
        test_rejected_or_unknown_restart_evidence_cannot_authorize_later_boot);
    RUN_TEST(test_stability_window_is_derived_from_central_safety_state);
    RUN_TEST(test_cleared_history_reuses_active_capacity_but_not_instance_ids);
    RUN_TEST(test_active_capacity_includes_two_o2_safety_roles);
    RUN_TEST(test_bounded_watchdog_and_unknown_domains_reuse_identity);
    RUN_TEST(test_configuration_forwarding_reuses_producer_correlation);
    RUN_TEST(
        test_safe_boot_exit_is_authorized_after_configuration_qualification);
    RUN_TEST(test_third_abnormal_restart_reaches_authorized_safe_boot_exit);
    RUN_TEST(test_normal_restart_and_missing_config_do_not_exit_safe_boot);
    RUN_TEST(test_safe_boot_exit_requires_current_four_domain_evidence);
    RUN_TEST(test_safe_boot_exit_requires_both_required_sensor_roles);
    RUN_TEST(
        test_real_application_boundary_consumes_public_configuration_result);
    return UNITY_END();
}
