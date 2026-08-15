#include <unity.h>

#include <array>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <string>

#include "actuator_plan_types.hpp"
#include "configuration_bootstrap_store.hpp"
#include "configuration_graph_store.hpp"
#include "configuration_mutation_coordinator.hpp"
#include "configuration_recovery_service.hpp"
#include "configuration_service.hpp"
#include "device_platform.hpp"
#include "fault_types.hpp"
#include "fermentation_application.hpp"
#include "restart_episode.hpp"
#include "safety_fault_service.hpp"
#include "safety_state_store.hpp"
#include "sensor_selection.hpp"
#include "simulated_persistent_state_store.hpp"
#include "simulated_reset_controller.hpp"
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
    TEST_ASSERT_FALSE(episode.advanceStableWindow(record, 0U, true));
    TEST_ASSERT_FALSE(episode.advanceStableWindow(
        record, kStableRestartWindowMillis - 1U, true));
    TEST_ASSERT_TRUE(
        episode.advanceStableWindow(record, kStableRestartWindowMillis, true));
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
    const auto clearRevision = fault->faultRevision;
    TEST_ASSERT_TRUE(
        service.clearFaultCause(fault->instanceId, clearRevision) ==
        SafetyServiceStatus::Ready);
    const auto* cleared = service.faultCore().find(fault->instanceId);
    TEST_ASSERT_NOT_NULL(cleared);
    TEST_ASSERT_TRUE(
        service.resetFault(cleared->instanceId, cleared->faultRevision)
            .status == SafetyServiceStatus::ResetCommitted);
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
    TEST_ASSERT_TRUE(service.actuatorGateInput().status ==
                     ActuatorSafetyGateStatus::ImmediateStop);
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
        test_real_application_boundary_consumes_public_configuration_result);
    return UNITY_END();
}
