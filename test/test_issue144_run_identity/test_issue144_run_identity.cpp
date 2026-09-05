#include <unity.h>

#include <limits>
#include <optional>
#include <utility>
#include <variant>

#include "application_run_identity.hpp"
#include "configuration_bootstrap_store.hpp"
#include "configuration_graph_store.hpp"
#include "configuration_mutation_coordinator.hpp"
#include "configuration_recovery_service.hpp"
#include "configuration_service.hpp"
#include "device_platform.hpp"
#include "fermentation_application.hpp"
#include "fermentation_ui_commands.hpp"
#include "mock_time_zone_resolver.hpp"
#include "run_persistence_coordinator.hpp"
#include "state_store.hpp"
#include "state_store_key.hpp"
#include "standard_program_catalog.hpp"
#include "simulated_persistent_state_store.hpp"

namespace fermentation {

class ApplicationRunIdentityTestAccess {
   public:
    static ApplicationCommandIdentity allocate(ApplicationRunIdentity& value) {
        return *value.allocateForApplication().identity;
    }
    static ApplicationCommandIdentityResult allocateResult(
        ApplicationRunIdentity& value) {
        return value.allocateForApplication();
    }
    static std::optional<std::string> makeRunId(
        const ApplicationRunIdentity& value, CommandId commandId) {
        return value.makeRunId(commandId);
    }
};

}  // namespace fermentation

namespace {

using device_platform::StorageEpoch;
using fermentation::ApplicationRunIdentity;
using fermentation::ApplicationRunIdentityTestAccess;
using fermentation::CommandId;
using fermentation::ProgramCatalogRevision;

fermentation::FermentationApplicationOwningEvidence owningEvidence();

class FailNextRunSlotStore final : public device_platform::IStateStore {
   public:
    device_platform::StateStoreWriteStatus write(
        const device_platform::StateStoreKey& key,
        const std::string& value) override {
        if (!failNextRunSlotKey_.empty() &&
            key.bytes() == failNextRunSlotKey_) {
            failNextRunSlotKey_.clear();
            return device_platform::StateStoreWriteStatus::WriteError;
        }
        return backing_.write(key, value);
    }

    device_platform::StateStoreReadResult read(
        const device_platform::StateStoreKey& key,
        std::size_t maxBytes) const override {
        return backing_.read(key, maxBytes);
    }

    void failNextRunSlot(const char* key = "rc0") { failNextRunSlotKey_ = key; }
    void restart() { backing_.restart(); }

   private:
    device_platform_test_support::SimulatedPersistentStateStore backing_;
    std::string failNextRunSlotKey_;
};

void test_empty_identity_space_allocates_from_one() {
    auto identity = ApplicationRunIdentity::create(
        StorageEpoch{7U}, std::optional<CommandId>{CommandId{0U}});
    TEST_ASSERT_TRUE(identity.has_value());

    auto allocator = std::move(*identity);
    const auto first = ApplicationRunIdentityTestAccess::allocate(allocator);
    TEST_ASSERT_EQUAL_UINT64(1U, first.commandId());
    TEST_ASSERT_EQUAL_UINT64(1U, first.uiRequestId().value);
    const auto runId = ApplicationRunIdentityTestAccess::makeRunId(
        allocator, first.commandId());
    TEST_ASSERT_TRUE(runId.has_value());
    TEST_ASSERT_EQUAL_STRING("e7-c1", runId->c_str());
}

void test_identity_uses_committed_high_water_and_rejects_invalid_bases() {
    auto identity = ApplicationRunIdentity::create(
        StorageEpoch{3U}, std::optional<CommandId>{CommandId{41U}});
    TEST_ASSERT_TRUE(identity.has_value());
    auto allocator = std::move(*identity);
    TEST_ASSERT_EQUAL_UINT64(
        42U, ApplicationRunIdentityTestAccess::allocate(allocator).commandId());

    TEST_ASSERT_FALSE(ApplicationRunIdentity::create(
                          StorageEpoch{}, std::optional<CommandId>{0U})
                          .has_value());
    TEST_ASSERT_FALSE(
        ApplicationRunIdentity::create(StorageEpoch{3U}, std::nullopt)
            .has_value());
}

void test_identity_overflow_does_not_wrap_or_issue_zero() {
    auto identity = ApplicationRunIdentity::create(
        StorageEpoch{1U},
        std::optional<CommandId>{std::numeric_limits<CommandId>::max()});
    TEST_ASSERT_TRUE(identity.has_value());
    auto allocator = std::move(*identity);
    const auto allocation =
        ApplicationRunIdentityTestAccess::allocateResult(allocator);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::ApplicationRunIdentityStatus::Overflow),
        static_cast<int>(allocation.status));
    TEST_ASSERT_FALSE(allocation.identity.has_value());
}

void test_catalog_revision_maps_to_neutral_run_provenance_without_truncation() {
    const auto mapped = fermentation::makeRunProgramSourceRevision(
        ProgramCatalogRevision{0x1'0000'0000ULL + 9U});
    TEST_ASSERT_TRUE(mapped.has_value());
    TEST_ASSERT_EQUAL_UINT64(0x1'0000'0000ULL + 9U, mapped->value());
    TEST_ASSERT_FALSE(
        fermentation::makeRunProgramSourceRevision(ProgramCatalogRevision{})
            .has_value());
}

void test_ui_id_is_application_bound_to_existing_command_envelope() {
    device_platform::DevicePlatform platform;
    device_platform_test_support::SimulatedPersistentStateStore store;
    device_platform_test_support::MockTimeZoneResolver timeZoneResolver;
    fermentation::FermentationApplication application;
    TEST_ASSERT_TRUE(platform.begin({true}));
    TEST_ASSERT_TRUE(application.begin(platform, store, timeZoneResolver));

    fermentation::FermentationUiCommandContext context;
    context.surface = device_platform::UiSurface::WebInterface;
    context.monotonicMillis = 100U;
    fermentation::FermentationUiStartManualHoldingIntent manual;
    manual.plan.targetTemperatureCelsius = 30.0;
    const auto prepared = application.prepareStartManualHolding(
        context, manual, owningEvidence());
    TEST_ASSERT_TRUE(prepared.request.has_value());
    TEST_ASSERT_TRUE(prepared.uiRequestId.has_value());
    TEST_ASSERT_EQUAL_UINT64(prepared.uiRequestId->value,
                             prepared.request->commandEnvelope().id);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::CommandSource::WebInterface),
        static_cast<int>(prepared.request->commandEnvelope().source));
}

void test_application_prepares_manual_timed_with_shared_identity() {
    device_platform::DevicePlatform platform;
    device_platform_test_support::SimulatedPersistentStateStore store;
    device_platform_test_support::MockTimeZoneResolver timeZoneResolver;
    fermentation::FermentationApplication application;

    TEST_ASSERT_TRUE(platform.begin({true}));
    TEST_ASSERT_TRUE(application.begin(platform, store, timeZoneResolver));

    fermentation::FermentationUiCommandContext context;
    context.surface = device_platform::UiSurface::LocalDisplay;
    context.monotonicMillis = 100U;
    fermentation::ManualTimedRunValues values;
    values.targetTemperatureCelsius = 30.0;
    values.durationMinutes = 60U;
    values.sensorMode = fermentation::RunSensorMode::Product;
    values.preheatEnabled = true;
    values.maximumProductWaitMinutes = 30U;
    values.qualificationBandCelsius = 0.5;
    values.qualificationDurationMinutes = 10U;
    values.maximumTargetReachMinutes = 180U;

    const auto prepared =
        application.prepareStartManualTimed(context, values, owningEvidence());
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            fermentation::FermentationApplicationRequestStatus::Prepared),
        static_cast<int>(prepared.status));
    TEST_ASSERT_TRUE(prepared.request.has_value());
    TEST_ASSERT_EQUAL_UINT64(1U, prepared.request->commandId());
    TEST_ASSERT_TRUE(prepared.request->runId().has_value());
    TEST_ASSERT_EQUAL_STRING("e1-c1", prepared.request->runId()->c_str());

    const auto confirmed =
        fermentation::FermentationApplication::confirmPrepared(prepared);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            fermentation::FermentationApplicationRequestStatus::Prepared),
        static_cast<int>(confirmed.status));
    TEST_ASSERT_EQUAL_UINT64(prepared.request->commandId(),
                             confirmed.request->commandId());
    TEST_ASSERT_EQUAL_STRING(prepared.request->runId()->c_str(),
                             confirmed.request->runId()->c_str());
    TEST_ASSERT_FALSE(prepared.request->commandEnvelope().confirmed);
    TEST_ASSERT_TRUE(confirmed.request->commandEnvelope().confirmed);
}

fermentation::FermentationApplicationOwningEvidence owningEvidence() {
    fermentation::FermentationApplicationOwningEvidence evidence;
    evidence.safetyAllowsStart = true;
    evidence.safetyAllowsCooling = true;
    evidence.airSensorValid = true;
    evidence.coolingSensorValid = true;
    evidence.productSensorValid = true;
    return evidence;
}

fermentation::FermentationUiManualRunPlanValues coolingValues() {
    fermentation::FermentationUiManualRunPlanValues values;
    values.targetTemperatureCelsius = 8.0;
    values.sensorMode = fermentation::RunSensorMode::Air;
    values.qualificationBandCelsius = 0.5;
    values.qualificationDurationMinutes = 10U;
    values.maximumTargetReachMinutes = 60U;
    return values;
}

void seedActiveRunForApplication(device_platform::IStateStore& store) {
    fermentation::RunPersistenceCoordinator coordinator(
        store, device_platform::StorageEpoch{1U},
        fermentation::RunCheckpointSchedule{});
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            fermentation::RunPersistenceLoadStatus::NoPersistedRun),
        static_cast<int>(coordinator.loadAndInitialize().status));
    auto program = fermentation::FactoryProgramCatalog::find("water-kefir");
    TEST_ASSERT_TRUE(program.has_value());
    program->program.productSensorFailure.fallbackDelaySeconds = 30U;
    program->program.fermentationStages.front().targetTemperatureCelsius = 38.0;
    program->program.fermentationStages.front().durationMinutes = 120U;
    program->program.targetQualification.bandCelsius = 0.5;
    program->program.targetQualification.durationMinutes = 10U;
    program->program.maximumTargetReachMinutes = 180U;
    TEST_ASSERT_TRUE(fermentation::validateProgram(*program).valid());

    fermentation::RunCommandState state;
    state.processState.state = fermentation::ProcessState::Standby;
    fermentation::ProgramStartRequest request;
    request.envelope = {11U,
                        fermentation::CommandSource::LocalDisplay,
                        100U,
                        state.processState.transitionSequence,
                        state.runRevision,
                        std::nullopt,
                        std::nullopt,
                        true,
                        std::nullopt};
    request.runId = "e1-c11";
    request.program = *program;
    request.sourceProgramRevision = fermentation::RunProgramSourceRevision{1U};
    request.sensorMode = fermentation::RunSensorMode::Product;
    request.safetyAllowsStart = true;
    request.airSensorValid = true;
    request.coolingSensorValid = true;
    request.productSensorValid = true;
    const auto decision = fermentation::decideProgramStart(state, request);
    TEST_ASSERT_TRUE(decision.proposed());
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::RunPersistenceResultStatus::Applied),
        static_cast<int>(coordinator
                             .persistCommand(state, decision,
                                             fermentation::RunCheckpointTime{
                                                 100U, 1'700'000'000LL})
                             .status));
}

void test_application_composes_all_run_identities_at_one_boundary() {
    device_platform::DevicePlatform platform;
    device_platform_test_support::SimulatedPersistentStateStore store;
    device_platform_test_support::MockTimeZoneResolver timeZoneResolver;
    fermentation::FermentationApplication application;

    TEST_ASSERT_TRUE(platform.begin({true}));
    TEST_ASSERT_TRUE(application.begin(platform, store, timeZoneResolver));
    TEST_ASSERT_TRUE(application.ready());

    fermentation::FermentationUiCommandContext context;
    context.surface = device_platform::UiSurface::LocalDisplay;
    context.monotonicMillis = 100U;
    context.expected.expectedStateSequence = 0U;
    context.expected.expectedProgramCatalogRevision =
        fermentation::ProgramCatalogRevision{1U};
    const auto evidence = owningEvidence();

    fermentation::FermentationUiStartProgramIntent startProgram;
    startProgram.programId = "water-kefir";
    startProgram.sensorMode = fermentation::RunSensorMode::Product;
    const auto preparedProgram =
        application.prepareStartProgram(context, startProgram, evidence);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::FermentationApplicationRequestStatus::
                             ProgramUnavailable),
        static_cast<int>(preparedProgram.status));
    TEST_ASSERT_FALSE(preparedProgram.request.has_value());
    TEST_ASSERT_FALSE(preparedProgram.uiRequestId.has_value());

    auto staleContext = context;
    staleContext.expected.expectedProgramCatalogRevision =
        fermentation::ProgramCatalogRevision{2U};
    const auto stale =
        application.prepareStartProgram(staleContext, startProgram, evidence);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::FermentationApplicationRequestStatus::
                             StaleProgramCatalog),
        static_cast<int>(stale.status));
    TEST_ASSERT_FALSE(stale.request.has_value());

    fermentation::FermentationUiStartManualHoldingIntent manualStart;
    manualStart.plan.targetTemperatureCelsius = 30.0;
    manualStart.plan.sensorMode = fermentation::RunSensorMode::Air;
    const auto preparedManual =
        application.prepareStartManualHolding(context, manualStart, evidence);
    TEST_ASSERT_TRUE(preparedManual.request.has_value());
    TEST_ASSERT_EQUAL_UINT64(1U, preparedManual.request->commandId());
    TEST_ASSERT_TRUE(preparedManual.request->runId().has_value());
    TEST_ASSERT_EQUAL_STRING("e1-c1", preparedManual.request->runId()->c_str());

    fermentation::FermentationUiStopRunIntent stop;
    stop.option = fermentation::StopOption::AbortAndTurnOff;
    const auto preparedStop = application.prepareStop(context, stop, evidence);
    TEST_ASSERT_TRUE(preparedStop.request.has_value());
    TEST_ASSERT_EQUAL_UINT64(2U, preparedStop.request->commandId());
    TEST_ASSERT_FALSE(preparedStop.request->runId().has_value());

    fermentation::FermentationUiCompleteRunIntent complete;
    const auto preparedComplete =
        application.prepareCompletion(context, complete, evidence);
    TEST_ASSERT_TRUE(preparedComplete.request.has_value());
    TEST_ASSERT_EQUAL_UINT64(3U, preparedComplete.request->commandId());
    TEST_ASSERT_FALSE(preparedComplete.request->runId().has_value());

    stop.option = fermentation::StopOption::AbortAndCool;
    stop.coolingPlan = coolingValues();
    const auto preparedCoolingStop =
        application.prepareStop(context, stop, evidence);
    TEST_ASSERT_TRUE(preparedCoolingStop.request.has_value());
    TEST_ASSERT_TRUE(preparedCoolingStop.uiRequestId.has_value());
    TEST_ASSERT_EQUAL_UINT64(4U, preparedCoolingStop.request->commandId());
    TEST_ASSERT_TRUE(preparedCoolingStop.request->runId().has_value());
    TEST_ASSERT_EQUAL_STRING("e1-c4",
                             preparedCoolingStop.request->runId()->c_str());

    complete.startCooling = true;
    complete.coolingPlan = coolingValues();
    const auto preparedCoolingCompletion =
        application.prepareCompletion(context, complete, evidence);
    TEST_ASSERT_TRUE(preparedCoolingCompletion.request.has_value());
    TEST_ASSERT_TRUE(preparedCoolingCompletion.uiRequestId.has_value());
    TEST_ASSERT_EQUAL_UINT64(5U,
                             preparedCoolingCompletion.request->commandId());
    TEST_ASSERT_TRUE(preparedCoolingCompletion.request->runId().has_value());
    TEST_ASSERT_EQUAL_STRING(
        "e1-c5", preparedCoolingCompletion.request->runId()->c_str());
}

void test_application_reset_hands_off_existing_run_store_to_new_epoch() {
    device_platform::DevicePlatform platform;
    device_platform_test_support::SimulatedPersistentStateStore store;
    device_platform_test_support::MockTimeZoneResolver timeZoneResolver;
    seedActiveRunForApplication(store);
    store.restart();
    fermentation::FermentationApplication application;

    TEST_ASSERT_TRUE(platform.begin({true}));
    TEST_ASSERT_TRUE(application.begin(platform, store, timeZoneResolver));
    TEST_ASSERT_TRUE(application.ready());

    const auto reset = application.beginAuthorizedFactoryReset();
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            fermentation::ConfigurationRecoveryStatus::FactoryResetCompleted),
        static_cast<int>(reset.status));
    TEST_ASSERT_TRUE(application.ready());

    fermentation::FermentationUiCommandContext context;
    context.monotonicMillis = 200U;
    fermentation::FermentationUiStartManualHoldingIntent manual;
    const auto prepared = application.prepareStartManualHolding(
        context, manual, owningEvidence());
    TEST_ASSERT_TRUE(prepared.request.has_value());
    TEST_ASSERT_EQUAL_UINT64(1U, prepared.request->commandId());
    TEST_ASSERT_EQUAL_STRING("e2-c1", prepared.request->runId()->c_str());
}

void test_application_prepares_every_envelope_action_with_one_identity() {
    device_platform::DevicePlatform platform;
    device_platform_test_support::SimulatedPersistentStateStore store;
    device_platform_test_support::MockTimeZoneResolver timeZoneResolver;
    fermentation::FermentationApplication application;

    TEST_ASSERT_TRUE(platform.begin({true}));
    TEST_ASSERT_TRUE(application.begin(platform, store, timeZoneResolver));
    TEST_ASSERT_TRUE(application.ready());

    fermentation::FermentationUiCommandContext context;
    context.monotonicMillis = 100U;
    fermentation::FermentationApplicationOwningEvidence evidence;
    evidence.safetyAllowsChange = true;
    evidence.faultResetEvaluation = fermentation::FaultResetEvaluation{};
    evidence.sensorPlausibility = fermentation::CrossRolePlausibilityContext{};

    const auto prepare = [&application, &context,
                          &evidence](const auto& intent) {
        return application.prepareEnvelope(
            context, fermentation::FermentationUiEnvelopePayload{intent},
            evidence);
    };
    const auto adjustment =
        prepare(fermentation::FermentationUiAdjustRunIntent{});
    const auto correction =
        prepare(fermentation::FermentationUiRecoveryTimeCorrectionIntent{12U});
    const auto acknowledgement =
        prepare(fermentation::FermentationUiAcknowledgeMessageIntent{7U});
    const auto mute =
        prepare(fermentation::FermentationUiMuteMessageIntent{7U});
    const auto reset = prepare(fermentation::FermentationUiResetFaultIntent{});
    const auto sensor =
        prepare(fermentation::FermentationUiSensorSelectionIntent{});

    TEST_ASSERT_EQUAL_UINT64(1U, adjustment.request->commandId());
    TEST_ASSERT_EQUAL_UINT64(2U, correction.request->commandId());
    TEST_ASSERT_EQUAL_UINT64(3U, acknowledgement.request->commandId());
    TEST_ASSERT_EQUAL_UINT64(4U, mute.request->commandId());
    TEST_ASSERT_EQUAL_UINT64(5U, reset.request->commandId());
    TEST_ASSERT_EQUAL_UINT64(6U, sensor.request->commandId());
}

void test_confirmation_reuses_prepared_request_without_reallocation() {
    device_platform::DevicePlatform platform;
    device_platform_test_support::SimulatedPersistentStateStore store;
    device_platform_test_support::MockTimeZoneResolver timeZoneResolver;
    fermentation::FermentationApplication application;

    TEST_ASSERT_TRUE(platform.begin({true}));
    TEST_ASSERT_TRUE(application.begin(platform, store, timeZoneResolver));

    fermentation::FermentationUiCommandContext context;
    context.surface = device_platform::UiSurface::WebInterface;
    context.monotonicMillis = 314U;
    context.expected.expectedStateSequence = 2U;
    context.expected.expectedRunRevision = 3U;
    context.expected.expectedMessageRevision = 4U;
    context.expected.expectedFaultRevision = 5U;
    context.expected.expectedRecoveryEpisodeRevision = 6U;

    fermentation::FermentationUiStartManualHoldingIntent manual;
    const auto prepared = application.prepareStartManualHolding(
        context, manual, owningEvidence());
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            fermentation::FermentationApplicationRequestStatus::Prepared),
        static_cast<int>(prepared.status));
    const auto confirmed =
        fermentation::FermentationApplication::confirmPrepared(prepared);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            fermentation::FermentationApplicationRequestStatus::Prepared),
        static_cast<int>(confirmed.status));
    TEST_ASSERT_EQUAL_UINT64(prepared.request->commandId(),
                             confirmed.request->commandId());
    TEST_ASSERT_EQUAL_STRING(prepared.request->runId()->c_str(),
                             confirmed.request->runId()->c_str());
    const auto& before = prepared.request->commandEnvelope();
    const auto& after = confirmed.request->commandEnvelope();
    TEST_ASSERT_EQUAL_INT(static_cast<int>(before.source),
                          static_cast<int>(after.source));
    TEST_ASSERT_EQUAL_UINT64(before.monotonicMillis, after.monotonicMillis);
    TEST_ASSERT_EQUAL_UINT32(before.expectedStateSequence,
                             after.expectedStateSequence);
    TEST_ASSERT_EQUAL_UINT32(before.expectedRunRevision.value(),
                             after.expectedRunRevision.value());
    TEST_ASSERT_EQUAL_UINT32(before.expectedMessageRevision.value(),
                             after.expectedMessageRevision.value());
    TEST_ASSERT_EQUAL_UINT32(before.expectedFaultRevision.value(),
                             after.expectedFaultRevision.value());
    TEST_ASSERT_EQUAL_UINT32(before.expectedRecoveryEpisodeRevision.value(),
                             after.expectedRecoveryEpisodeRevision.value());
    TEST_ASSERT_FALSE(prepared.request->commandEnvelope().confirmed);
    TEST_ASSERT_TRUE(confirmed.request->commandEnvelope().confirmed);

    const auto next = application.prepareStartManualHolding(
        fermentation::FermentationUiCommandContext{}, manual, owningEvidence());
    TEST_ASSERT_EQUAL_UINT64(prepared.request->commandId() + 1U,
                             next.request->commandId());
}

void test_application_reconstructs_reset_handoff_after_run_write_cut() {
    FailNextRunSlotStore store;
    seedActiveRunForApplication(store);
    store.restart();
    store.failNextRunSlot();

    device_platform::DevicePlatform platform;
    device_platform_test_support::MockTimeZoneResolver timeZoneResolver;
    fermentation::FermentationApplication application;
    TEST_ASSERT_TRUE(platform.begin({true}));
    TEST_ASSERT_TRUE(application.begin(platform, store, timeZoneResolver));
    TEST_ASSERT_TRUE(application.ready());

    const auto interrupted = application.beginAuthorizedFactoryReset();
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::ConfigurationRecoveryStatus::
                             RunPersistenceHandoffUnavailable),
        static_cast<int>(interrupted.status));
    TEST_ASSERT_FALSE(application.ready());

    store.restart();
    device_platform::DevicePlatform rebootedPlatform;
    fermentation::FermentationApplication rebooted;
    TEST_ASSERT_TRUE(rebootedPlatform.begin({true}));
    TEST_ASSERT_TRUE(rebooted.begin(rebootedPlatform, store, timeZoneResolver));
    TEST_ASSERT_TRUE(rebooted.ready());

    fermentation::FermentationUiStartManualHoldingIntent manual;
    const auto prepared = rebooted.prepareStartManualHolding(
        fermentation::FermentationUiCommandContext{}, manual, owningEvidence());
    TEST_ASSERT_TRUE(prepared.request.has_value());
    TEST_ASSERT_EQUAL_UINT64(1U, prepared.request->commandId());
    TEST_ASSERT_EQUAL_STRING("e2-c1", prepared.request->runId()->c_str());
}

void seedCommittedHandoffWithFinalizedHead(
    device_platform_test_support::SimulatedPersistentStateStore& store,
    device_platform_test_support::MockTimeZoneResolver& timeZoneResolver) {
    fermentation::ConfigurationMutationCoordinator mutationCoordinator;
    fermentation::ConfigurationBootstrapStore bootstrap(store);
    fermentation::ConfigurationGraphStore graph(store, timeZoneResolver);
    fermentation::ConfigurationService configuration(mutationCoordinator, graph,
                                                     timeZoneResolver);
    auto recovery = fermentation::ConfigurationRecoveryService::create(
        store, bootstrap, graph, configuration, mutationCoordinator);
    TEST_ASSERT_TRUE(recovery != nullptr);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::ConfigurationRecoveryStatus::
                             FactoryInitializationCompleted),
        static_cast<int>(recovery->boot().status));
    const auto reset = recovery->beginAuthorizedFactoryReset();
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            fermentation::ConfigurationRecoveryStatus::FactoryResetCompleted),
        static_cast<int>(reset.status));
    auto proof = recovery->takeAuthorizedRunEpochHandoffProof();
    TEST_ASSERT_TRUE(proof.has_value());

    fermentation::RunPersistenceCoordinator runPersistence(
        store, device_platform::StorageEpoch{2U},
        fermentation::RunCheckpointSchedule{});
    const auto prepared = runPersistence.prepareAuthorizedEpochHandoff(*proof);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::RunPersistenceResultStatus::Applied),
        static_cast<int>(prepared.persistenceResult.status));
    TEST_ASSERT_TRUE(prepared.evidence.has_value());
    const auto committed =
        recovery->commitAuthorizedRunEpochHandoff(*proof, *prepared.evidence);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            fermentation::ConfigurationRecoveryStatus::RuntimeReady),
        static_cast<int>(committed.status));
    const auto finalized =
        runPersistence.finalizeAuthorizedEpochHandoff(*proof);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::RunPersistenceResultStatus::Applied),
        static_cast<int>(finalized.persistenceResult.status));
    TEST_ASSERT_TRUE(finalized.evidence.has_value());
    // Deliberately leave the persistent bootstrap record Committed.  The
    // application must verify the exact target head and consume this phase
    // before publishing Ready or initializing the command allocator.
}

void test_application_finishes_committed_handoff_before_ready() {
    device_platform_test_support::SimulatedPersistentStateStore store;
    device_platform_test_support::MockTimeZoneResolver timeZoneResolver;
    seedCommittedHandoffWithFinalizedHead(store, timeZoneResolver);
    store.restart();

    device_platform::DevicePlatform platform;
    fermentation::FermentationApplication application;
    TEST_ASSERT_TRUE(platform.begin({true}));
    TEST_ASSERT_TRUE(application.begin(platform, store, timeZoneResolver));
    TEST_ASSERT_TRUE(application.ready());

    fermentation::ConfigurationBootstrapStore bootstrap(store);
    const auto scan = bootstrap.scan();
    TEST_ASSERT_TRUE(scan.loaded.has_value());
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::RunEpochHandoffState::Consumed),
        static_cast<int>(scan.loaded->record.handoff));

    fermentation::FermentationUiStartManualHoldingIntent manual;
    const auto prepared = application.prepareStartManualHolding(
        fermentation::FermentationUiCommandContext{}, manual, owningEvidence());
    TEST_ASSERT_TRUE(prepared.request.has_value());
    TEST_ASSERT_EQUAL_UINT64(1U, prepared.request->commandId());
    TEST_ASSERT_EQUAL_STRING("e2-c1", prepared.request->runId()->c_str());
}

void test_application_resumes_empty_partial_handoff_before_allocator() {
    FailNextRunSlotStore store;
    device_platform::DevicePlatform platform;
    device_platform_test_support::MockTimeZoneResolver timeZoneResolver;
    fermentation::FermentationApplication application;
    TEST_ASSERT_TRUE(platform.begin({true}));
    TEST_ASSERT_TRUE(application.begin(platform, store, timeZoneResolver));
    TEST_ASSERT_TRUE(application.ready());

    // Empty run store: slot 0 is durably prepared, slot 1 fails.  The head
    // must still be absent because Pending never writes it.
    store.failNextRunSlot("rc1");
    const auto interrupted = application.beginAuthorizedFactoryReset();
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::ConfigurationRecoveryStatus::
                             RunPersistenceHandoffUnavailable),
        static_cast<int>(interrupted.status));
    const auto head =
        store.read(*device_platform::StateStoreKey::create("rh0").key, 256U);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(device_platform::StateStoreReadStatus::NotFound),
        static_cast<int>(head.status));
    const auto firstSlot =
        store.read(*device_platform::StateStoreKey::create("rc0").key, 8240U);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(device_platform::StateStoreReadStatus::Success),
        static_cast<int>(firstSlot.status));

    store.restart();
    device_platform::DevicePlatform rebootedPlatform;
    fermentation::FermentationApplication rebooted;
    TEST_ASSERT_TRUE(rebootedPlatform.begin({true}));
    TEST_ASSERT_TRUE(rebooted.begin(rebootedPlatform, store, timeZoneResolver));
    TEST_ASSERT_TRUE(rebooted.ready());
    fermentation::FermentationUiStartManualHoldingIntent manual;
    const auto prepared = rebooted.prepareStartManualHolding(
        fermentation::FermentationUiCommandContext{}, manual, owningEvidence());
    TEST_ASSERT_TRUE(prepared.request.has_value());
    TEST_ASSERT_EQUAL_UINT64(1U, prepared.request->commandId());
    TEST_ASSERT_EQUAL_STRING("e2-c1", prepared.request->runId()->c_str());
}

}  // namespace

void setUp() {}
void tearDown() {}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_empty_identity_space_allocates_from_one);
    RUN_TEST(test_identity_uses_committed_high_water_and_rejects_invalid_bases);
    RUN_TEST(test_identity_overflow_does_not_wrap_or_issue_zero);
    RUN_TEST(
        test_catalog_revision_maps_to_neutral_run_provenance_without_truncation);
    RUN_TEST(test_ui_id_is_application_bound_to_existing_command_envelope);
    RUN_TEST(test_application_prepares_manual_timed_with_shared_identity);
    RUN_TEST(test_application_composes_all_run_identities_at_one_boundary);
    RUN_TEST(test_application_reset_hands_off_existing_run_store_to_new_epoch);
    RUN_TEST(test_application_prepares_every_envelope_action_with_one_identity);
    RUN_TEST(test_confirmation_reuses_prepared_request_without_reallocation);
    RUN_TEST(test_application_reconstructs_reset_handoff_after_run_write_cut);
    RUN_TEST(test_application_finishes_committed_handoff_before_ready);
    RUN_TEST(test_application_resumes_empty_partial_handoff_before_allocator);
    return UNITY_END();
}
