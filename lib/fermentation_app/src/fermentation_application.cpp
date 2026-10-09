#include "fermentation_application.hpp"

#include "configuration_text.hpp"

#include <algorithm>
#include <cctype>
#include <new>
#include <type_traits>
#include <utility>

#include "configuration_bootstrap_store.hpp"
#include "configuration_graph_store.hpp"
#include "configuration_mutation_coordinator.hpp"
#include "configuration_recovery_service.hpp"
#include "configuration_service.hpp"
#include "connectivity_credentials.hpp"
#include "fermentation_ui_commands.hpp"
#include "network_configuration_service.hpp"
#include "network_setup_routes.hpp"
#include "run_commands.hpp"
#include "run_persistence_coordinator.hpp"
#include "web_application_routes.hpp"

namespace fermentation {
namespace {

std::optional<std::string> networkHostnameFromDeviceName(
    const std::string& deviceName) {
    std::string hostname;
    hostname.reserve(deviceName.size());
    for (const unsigned char value : deviceName) {
        if (value < 0x80U && std::isalnum(value) != 0) {
            hostname.push_back(static_cast<char>(std::tolower(value)));
        } else if (!hostname.empty() && hostname.back() != '-') {
            hostname.push_back('-');
        }
        if (hostname.size() == 63U) {
            break;
        }
    }
    while (!hostname.empty() && hostname.back() == '-') {
        hostname.pop_back();
    }
    if (hostname.empty()) {
        return std::nullopt;
    }
    return hostname;
}

FaultCode configurationFault(ConfigurationRecoveryStatus status) {
    switch (status) {
        case ConfigurationRecoveryStatus::ConfigurationIntegrityFailure:
        case ConfigurationRecoveryStatus::UnsupportedNewerConfigurationSchema:
            return FaultCode::ConfigurationIntegrityFailure;
        case ConfigurationRecoveryStatus::BootstrapCommitIndeterminate:
        case ConfigurationRecoveryStatus::
            ConfigurationRecordOutcomeIndeterminate:
        case ConfigurationRecoveryStatus::ConfigurationCommitIndeterminate:
            return FaultCode::ConfigurationCommitIndeterminate;
        default:
            return FaultCode::ConfigurationUnavailable;
    }
}

FermentationApplicationRequestResult requestFailure(
    FermentationApplicationRequestStatus status) {
    return {status, std::nullopt, std::nullopt};
}

bool completeAuthorizedEpochHandoff(
    ConfigurationRecoveryService& configurationRecoveryService,
    RunPersistenceCoordinator& runPersistenceCoordinator,
    AuthorizedRunEpochHandoffProof& proof) {
    if (proof.phase() == AuthorizedRunEpochHandoffPhase::Pending) {
        const auto prepared =
            runPersistenceCoordinator.prepareAuthorizedEpochHandoff(proof);
        if (prepared.persistenceResult.status !=
                RunPersistenceResultStatus::Applied ||
            !prepared.evidence.has_value()) {
            return false;
        }
        const auto committed =
            configurationRecoveryService.commitAuthorizedRunEpochHandoff(
                proof, *prepared.evidence);
        if (committed.status != ConfigurationRecoveryStatus::RuntimeReady) {
            return false;
        }
    }

    const auto finalized =
        runPersistenceCoordinator.finalizeAuthorizedEpochHandoff(proof);
    if (finalized.persistenceResult.status !=
            RunPersistenceResultStatus::Applied ||
        !finalized.evidence.has_value()) {
        return false;
    }
    const auto consumed =
        configurationRecoveryService.consumeAuthorizedRunEpochHandoff(
            proof, *finalized.evidence);
    return consumed.status == ConfigurationRecoveryStatus::RuntimeReady;
}

FermentationApplicationRequestStatus mapIdentityStatus(
    ApplicationRunIdentityStatus status) {
    switch (status) {
        case ApplicationRunIdentityStatus::Allocated:
            return FermentationApplicationRequestStatus::Prepared;
        case ApplicationRunIdentityStatus::NotInitialized:
            return FermentationApplicationRequestStatus::NotInitialized;
        case ApplicationRunIdentityStatus::Overflow:
            return FermentationApplicationRequestStatus::Overflow;
        case ApplicationRunIdentityStatus::Unavailable:
            return FermentationApplicationRequestStatus::Unavailable;
    }
    return FermentationApplicationRequestStatus::Unavailable;
}

ManualRunPlanRequest makeManualRunPlanRequest(
    const FermentationUiManualRunPlanValues& values, const std::string& runId) {
    ManualRunPlanRequest request;
    request.runId = runId;
    request.targetTemperatureCelsius = values.targetTemperatureCelsius;
    request.sensorMode = values.sensorMode;
    request.preheatEnabled = values.preheatEnabled;
    request.maximumProductWaitMinutes = values.maximumProductWaitMinutes;
    request.qualificationBandCelsius = values.qualificationBandCelsius;
    request.qualificationDurationMinutes = values.qualificationDurationMinutes;
    request.maximumTargetReachMinutes = values.maximumTargetReachMinutes;
    return request;
}

ManualTimedRunSource makeManualTimedSource(const ManualTimedRunValues& values) {
    ManualTimedRunSource source;
    source.stage.targetTemperatureCelsius = values.targetTemperatureCelsius;
    source.stage.durationMinutes = values.durationMinutes;
    source.preheatEnabled = values.preheatEnabled;
    source.maximumProductWaitMinutes = values.maximumProductWaitMinutes;
    source.targetQualification.bandCelsius = values.qualificationBandCelsius;
    source.targetQualification.durationMinutes =
        values.qualificationDurationMinutes;
    source.maximumTargetReachMinutes = values.maximumTargetReachMinutes;
    source.completion.mode = values.completionMode;
    source.completion.coolingTargetCelsius = values.coolingTargetCelsius;
    source.completion.holdDurationMinutes = values.holdDurationMinutes;
    return source;
}

std::optional<ProgramDocument> findProgram(
    const RuntimeConfigurationSnapshot& snapshot,
    const std::string& programId) {
    for (const auto& document : snapshot.programCatalog().programs) {
        if (document.program.id == programId) {
            return document;
        }
    }
    return std::nullopt;
}

// O5 / #172 S9: the technical run limits of a manual run or a cooling plan
// (qualification band/duration, maximum target reach, ...) come only from a
// commissioning-released product/service owner (#34/#35). No such owner exists
// yet, so these requests stay fail-closed on every surface; values supplied by
// a caller (UI, web) never stand in for the missing owner and #172 defines no
// producer.
constexpr bool kManualRunLimitsOwnerAvailable = false;

// Requested start mode: an explicit next-run override wins; otherwise it
// follows the stored SensorPreference. The effective mode, the allowed
// fallback and the rejection stay with the #21 start matrix. An unknown
// enumerator is never mapped to Air.
std::optional<RunSensorMode> requestedProgramSensorMode(
    const ProgramDocument& program,
    const FermentationUiStartCandidate& candidate) noexcept {
    if (candidate.sensorMode.has_value()) {
        return candidate.sensorMode;
    }
    return defaultProgramStartSensorMode(program.program.sensorPreference);
}

// The one preview/commit sequence of the configuration changes (D5): preview
// from the live configuration, the mutation of the build lease, install with
// the given canonical wire values, revision validation, confirmation; every
// exit other than an activated or unchanged configuration releases the one
// visible preview slot.
template <typename Mutate>
ApplicationConfigurationChangeResult commitConfigurationChange(
    ConfigurationService& service, const UserConfigurationRevision& expected,
    const ChangeOrigin& origin, const ChangeOperation& operation,
    Mutate&& mutate) {
    using Preview = ConfigurationPreviewStatus;
    using Commit = ConfigurationCommitStatus;
    auto build = service.beginPreview();
    if (build.status != Preview::Success || !build.lease.valid()) {
        return {build.status == Preview::Success
                    ? Preview::ConfigurationRuntimeUnavailable
                    : build.status,
                Commit::ConfigurationRuntimeFailure};
    }
    mutate(build.lease);
    const auto installed =
        service.installPreview(std::move(build.lease), origin, operation);
    if (installed.status != Preview::Success ||
        !installed.preview.has_value()) {
        return {installed.status == Preview::Success ? Preview::InvalidCandidate
                                                     : installed.status,
                Commit::ConfigurationRuntimeFailure};
    }
    const auto handle = installed.preview->handle;
    const auto validation =
        service.validatePreviewForConfirmation(handle, expected);
    if (validation.status != Commit::ReadyForConfirmation) {
        static_cast<void>(service.cancelPreview(handle));
        return {Preview::Success, validation.status};
    }
    const auto committed = service.confirmPreview(handle);
    if (committed.status != Commit::Activated &&
        committed.status != Commit::NoChange) {
        static_cast<void>(service.cancelPreview(handle));
    }
    return {Preview::Success, committed.status};
}

// User configuration changes of the local surface (canonical wire values
// {LocalDisplay,2U} / {NormalEdit,1U}).
template <typename Mutate>
ApplicationConfigurationChangeResult commitUserConfigurationChange(
    ConfigurationService& service, const UserConfigurationRevision& expected,
    Mutate&& mutate) {
    return commitConfigurationChange(
        service, expected, {ChangeOriginKind::LocalDisplay, 2U},
        {ChangeOperationKind::NormalEdit, 1U},
        [&mutate](ConfigurationPreviewBuildLease& lease) {
            mutate(lease.userConfiguration());
        });
}

}  // namespace

template <typename Request>
FermentationApplicationRequestResult
FermentationApplication::makePreparedRequest(
    Request request,
    std::optional<CrossRolePlausibilityContext> owningPlausibility) {
    FermentationApplicationPreparedRequest prepared(
        FermentationApplicationPreparedRequest::Storage{std::move(request)},
        owningPlausibility);
    const auto uiRequestId = prepared.commandEnvelope().id;
    return {FermentationApplicationRequestStatus::Prepared, std::move(prepared),
            device_platform::UiRequestId{uiRequestId}};
}

FermentationApplication::FermentationApplication() noexcept = default;

FermentationApplication::~FermentationApplication() {
    // The product lifecycle stops the HTTP server before destruction; draining
    // here additionally guarantees that no authentication operation still
    // uses the domain or record store while the members are destroyed.
    authOperationGate_.closeAndDrain();
}

FermentationApplicationRequestResult
FermentationApplication::prepareStartProgram(
    const FermentationUiCommandContext& context,
    const FermentationUiStartProgramIntent& intent) {
    const auto guard = applicationCallSerializer_.enter();
    const auto evidence = resolveRuntimeEvidence();
    if (configurationService_ == nullptr || runIdentity_ == nullptr) {
        return requestFailure(
            FermentationApplicationRequestStatus::NotInitialized);
    }
    if (!context.expected.expectedProgramCatalogRevision.has_value()) {
        return requestFailure(
            FermentationApplicationRequestStatus::InvalidInput);
    }
    const auto runtime = configurationService_->acquireRuntime();
    if (runtime.status != RuntimeConfigurationReadStatus::RuntimeLeaseGranted) {
        return requestFailure(
            FermentationApplicationRequestStatus::Unavailable);
    }
    const auto& snapshot = runtime.lease.get();
    if (*context.expected.expectedProgramCatalogRevision !=
        snapshot.programCatalogRevision()) {
        return requestFailure(
            FermentationApplicationRequestStatus::StaleProgramCatalog);
    }
    const auto& candidate = intent.candidate;
    std::optional<ProgramDocument> program =
        findProgram(snapshot, candidate.programId);
    if (!program.has_value() || !program->program.installed) {
        return requestFailure(
            FermentationApplicationRequestStatus::ProgramUnavailable);
    }
    const bool nextRunOverride = hasStartCandidateOverride(candidate);
    const auto sensorMode = requestedProgramSensorMode(*program, candidate);
    if (!sensorMode.has_value()) {
        return requestFailure(
            FermentationApplicationRequestStatus::InvalidInput);
    }
    applyStartCandidateOverrides(*program, candidate);
    if (!validateProgram(*program, ValidationPurpose::Runnable).valid()) {
        return requestFailure(
            nextRunOverride
                ? FermentationApplicationRequestStatus::InvalidInput
                : FermentationApplicationRequestStatus::ProgramUnavailable);
    }
    const auto sourceRevision =
        makeRunProgramSourceRevision(snapshot.programCatalogRevision());
    if (!sourceRevision.has_value()) {
        return requestFailure(
            FermentationApplicationRequestStatus::Unavailable);
    }
    const auto identity = runIdentity_->allocateForApplication();
    if (!identity.identity.has_value()) {
        return requestFailure(mapIdentityStatus(identity.status));
    }
    const auto runId = runIdentity_->makeRunId(identity.identity->commandId());
    if (!runId.has_value()) {
        return requestFailure(
            FermentationApplicationRequestStatus::Unavailable);
    }
    ProgramStartRequest request;
    request.envelope =
        FermentationUiCommandBridge::makeEnvelope(context, *identity.identity);
    request.runId = *runId;
    request.program = std::move(*program);
    request.sourceProgramRevision = *sourceRevision;
    request.sensorMode = *sensorMode;
    request.safetyAllowsStart = evidence.safetyAllowsStart;
    request.airSensorValid = evidence.airSensorValid;
    request.coolingSensorValid = evidence.coolingSensorValid;
    request.productSensorValid = evidence.productSensorValid;
    return makePreparedRequest(std::move(request), evidence.plausibility);
}

// Requests that start a manual run or a cooling plan need the technical run
// limits from a commissioning-released owner (O5, #34/#35). None exists, so
// every surface gets the existing `Unavailable` before any run or command
// identity is used; caller-supplied limits never stand in for the owner. The
// `...Unguarded` bodies are private and only reachable by the test access
// class, which exercises the downstream owner paths.
FermentationApplicationRequestResult
FermentationApplication::prepareStartManualHolding(
    const FermentationUiCommandContext& context,
    const FermentationUiStartManualHoldingIntent& intent) {
    const auto guard = applicationCallSerializer_.enter();
    if (!kManualRunLimitsOwnerAvailable) {
        return requestFailure(
            FermentationApplicationRequestStatus::Unavailable);
    }
    return prepareStartManualHoldingUnguarded(context, intent);
}

FermentationApplicationRequestResult
FermentationApplication::prepareStartManualTimed(
    const FermentationUiCommandContext& context,
    const ManualTimedRunValues& values) {
    const auto guard = applicationCallSerializer_.enter();
    if (!kManualRunLimitsOwnerAvailable) {
        return requestFailure(
            FermentationApplicationRequestStatus::Unavailable);
    }
    return prepareStartManualTimedUnguarded(context, values);
}

FermentationApplicationRequestResult FermentationApplication::prepareStop(
    const FermentationUiCommandContext& context,
    const FermentationUiStopRunIntent& intent) {
    const auto guard = applicationCallSerializer_.enter();
    // Stopping and turning off needs no manual-run limits; only the cooling
    // start does (its plan is a manual run plan).
    if (intent.option == StopOption::AbortAndCool &&
        !kManualRunLimitsOwnerAvailable) {
        return requestFailure(
            FermentationApplicationRequestStatus::Unavailable);
    }
    return prepareStopUnguarded(context, intent);
}

FermentationApplicationRequestResult FermentationApplication::prepareCompletion(
    const FermentationUiCommandContext& context,
    const FermentationUiCompleteRunIntent& intent) {
    const auto guard = applicationCallSerializer_.enter();
    if (intent.startCooling && !kManualRunLimitsOwnerAvailable) {
        return requestFailure(
            FermentationApplicationRequestStatus::Unavailable);
    }
    return prepareCompletionUnguarded(context, intent);
}

FermentationApplicationRequestResult
FermentationApplication::prepareStartManualHoldingUnguarded(
    const FermentationUiCommandContext& context,
    const FermentationUiStartManualHoldingIntent& intent) {
    const auto guard = applicationCallSerializer_.enter();
    const auto evidence = resolveRuntimeEvidence();
    if (runIdentity_ == nullptr) {
        return requestFailure(
            FermentationApplicationRequestStatus::NotInitialized);
    }
    const auto identity = runIdentity_->allocateForApplication();
    if (!identity.identity.has_value()) {
        return requestFailure(mapIdentityStatus(identity.status));
    }
    const auto runId = runIdentity_->makeRunId(identity.identity->commandId());
    if (!runId.has_value()) {
        return requestFailure(
            FermentationApplicationRequestStatus::Unavailable);
    }
    ManualStartRequest request;
    request.envelope =
        FermentationUiCommandBridge::makeEnvelope(context, *identity.identity);
    request.plan = makeManualRunPlanRequest(intent.plan, *runId);
    request.safetyAllowsStart = evidence.safetyAllowsStart;
    request.airSensorValid = evidence.airSensorValid;
    request.coolingSensorValid = evidence.coolingSensorValid;
    request.productSensorValid = evidence.productSensorValid;
    return makePreparedRequest(std::move(request), evidence.plausibility);
}

FermentationApplicationRequestResult
FermentationApplication::prepareStartManualTimedUnguarded(
    const FermentationUiCommandContext& context,
    const ManualTimedRunValues& values) {
    const auto guard = applicationCallSerializer_.enter();
    const auto evidence = resolveRuntimeEvidence();
    if (runIdentity_ == nullptr) {
        return requestFailure(
            FermentationApplicationRequestStatus::NotInitialized);
    }
    auto source = makeManualTimedSource(values);
    if (!validateManualTimedRunSource(source)) {
        return requestFailure(
            FermentationApplicationRequestStatus::InvalidInput);
    }
    const auto identity = runIdentity_->allocateForApplication();
    if (!identity.identity.has_value()) {
        return requestFailure(mapIdentityStatus(identity.status));
    }
    const auto runId = runIdentity_->makeRunId(identity.identity->commandId());
    if (!runId.has_value()) {
        return requestFailure(
            FermentationApplicationRequestStatus::Unavailable);
    }
    ProgramStartRequest request;
    request.envelope =
        FermentationUiCommandBridge::makeEnvelope(context, *identity.identity);
    request.runId = *runId;
    request.program = source;
    request.sourceProgramRevision.reset();
    request.sensorMode = values.sensorMode;
    request.safetyAllowsStart = evidence.safetyAllowsStart;
    request.airSensorValid = evidence.airSensorValid;
    request.coolingSensorValid = evidence.coolingSensorValid;
    request.productSensorValid = evidence.productSensorValid;
    return makePreparedRequest(std::move(request), evidence.plausibility);
}

FermentationApplicationRequestResult
FermentationApplication::prepareStopUnguarded(
    const FermentationUiCommandContext& context,
    const FermentationUiStopRunIntent& intent) {
    const auto guard = applicationCallSerializer_.enter();
    const auto evidence = resolveRuntimeEvidence();
    if (runIdentity_ == nullptr) {
        return requestFailure(
            FermentationApplicationRequestStatus::NotInitialized);
    }
    const FermentationUiManualRunPlanValues* coolingPlan = nullptr;
    if (intent.option == StopOption::AbortAndCool) {
        if (!intent.coolingPlan.has_value()) {
            return requestFailure(
                FermentationApplicationRequestStatus::InvalidInput);
        }
        coolingPlan = &intent.coolingPlan.value();
    }
    const auto identity = runIdentity_->allocateForApplication();
    if (!identity.identity.has_value()) {
        return requestFailure(mapIdentityStatus(identity.status));
    }
    StopRequest request;
    request.envelope =
        FermentationUiCommandBridge::makeEnvelope(context, *identity.identity);
    request.option = intent.option;
    request.safetyAllowsCooling = evidence.safetyAllowsCooling;
    request.airSensorValid = evidence.airSensorValid;
    request.coolingSensorValid = evidence.coolingSensorValid;
    if (coolingPlan != nullptr) {
        const auto runId =
            runIdentity_->makeRunId(identity.identity->commandId());
        if (!runId.has_value()) {
            return requestFailure(
                FermentationApplicationRequestStatus::Unavailable);
        }
        request.coolingPlan = makeManualRunPlanRequest(*coolingPlan, *runId);
    }
    return makePreparedRequest(std::move(request));
}

FermentationApplicationRequestResult
FermentationApplication::prepareCompletionUnguarded(
    const FermentationUiCommandContext& context,
    const FermentationUiCompleteRunIntent& intent) {
    const auto guard = applicationCallSerializer_.enter();
    const auto evidence = resolveRuntimeEvidence();
    if (runIdentity_ == nullptr) {
        return requestFailure(
            FermentationApplicationRequestStatus::NotInitialized);
    }
    if (intent.startCooling && !intent.coolingPlan.has_value()) {
        return requestFailure(
            FermentationApplicationRequestStatus::InvalidInput);
    }
    const auto identity = runIdentity_->allocateForApplication();
    if (!identity.identity.has_value()) {
        return requestFailure(mapIdentityStatus(identity.status));
    }
    CompletionRequest request;
    request.envelope =
        FermentationUiCommandBridge::makeEnvelope(context, *identity.identity);
    request.startCooling = intent.startCooling;
    request.safetyAllowsCooling = evidence.safetyAllowsCooling;
    request.airSensorValid = evidence.airSensorValid;
    request.coolingSensorValid = evidence.coolingSensorValid;
    if (intent.startCooling) {
        const auto runId =
            runIdentity_->makeRunId(identity.identity->commandId());
        if (!runId.has_value()) {
            return requestFailure(
                FermentationApplicationRequestStatus::Unavailable);
        }
        request.coolingPlan =
            makeManualRunPlanRequest(*intent.coolingPlan, *runId);
    }
    return makePreparedRequest(std::move(request));
}

template <typename Intent>
FermentationApplicationRequestResult
FermentationApplication::prepareAdditionalEnvelope(
    const FermentationUiCommandContext& context, const Intent& intent) {
    if constexpr (std::is_same_v<Intent, FermentationUiResetFaultIntent>) {
        // #24's Planner/Watchdog owner is not part of the current #168
        // composition. Do not manufacture a second owner or a generic reset
        // request; the typed UI intent remains explicitly unavailable.
        return requestFailure(
            FermentationApplicationRequestStatus::Unavailable);
    }
    const auto evidence = resolveRuntimeEvidence();
    if (runIdentity_ == nullptr) {
        return requestFailure(
            FermentationApplicationRequestStatus::NotInitialized);
    }
    const auto identity = runIdentity_->allocateForApplication();
    if (!identity.identity.has_value()) {
        return requestFailure(mapIdentityStatus(identity.status));
    }
    const auto envelope =
        FermentationUiCommandBridge::makeEnvelope(context, *identity.identity);
    if constexpr (std::is_same_v<Intent, FermentationUiAdjustRunIntent>) {
        RunAdjustmentCommandRequest request;
        request.envelope = envelope;
        request.targetTemperatureCelsius = intent.targetTemperatureCelsius;
        request.remainingDurationMinutes = intent.remainingDurationMinutes;
        return makePreparedRequest(request);
    } else if constexpr (std::is_same_v<
                             Intent,
                             FermentationUiRecoveryTimeCorrectionIntent>) {
        ApplyRecoveryTimeCorrectionRequest request;
        request.envelope = envelope;
        request.secondsDelta = intent.secondsDelta;
        return makePreparedRequest(request);
    } else if constexpr (std::is_same_v<
                             Intent, FermentationUiAcknowledgeMessageIntent>) {
        MessageCommandRequest request;
        request.envelope = envelope;
        request.messageId = intent.messageId;
        return makePreparedRequest(
            FermentationApplicationPreparedRequest::PreparedAcknowledgeMessage{
                request});
    } else if constexpr (std::is_same_v<Intent,
                                        FermentationUiMuteMessageIntent>) {
        MessageCommandRequest request;
        request.envelope = envelope;
        request.messageId = intent.messageId;
        return makePreparedRequest(
            FermentationApplicationPreparedRequest::PreparedMuteMessage{
                request});
    } else if constexpr (std::is_same_v<Intent,
                                        FermentationUiSensorSelectionIntent>) {
        SensorSelectionCommandRequest request;
        request.envelope = envelope;
        request.action = intent.action;
        return makePreparedRequest(request, evidence.plausibility);
    }
    return requestFailure(FermentationApplicationRequestStatus::InvalidInput);
}

FermentationApplicationRequestResult FermentationApplication::prepareEnvelope(
    const FermentationUiCommandContext& context,
    const FermentationUiEnvelopePayload& payload) {
    const auto guard = applicationCallSerializer_.enter();
    return std::visit(
        [this,
         &context](const auto& intent) -> FermentationApplicationRequestResult {
            using Intent = std::decay_t<decltype(intent)>;
            if constexpr (std::is_same_v<Intent,
                                         FermentationUiStartProgramIntent>) {
                return prepareStartProgram(context, intent);
            } else if constexpr (std::is_same_v<
                                     Intent,
                                     FermentationUiStartManualHoldingIntent>) {
                return prepareStartManualHolding(context, intent);
            } else if constexpr (std::is_same_v<
                                     Intent,
                                     FermentationUiStartManualTimedIntent>) {
                return prepareStartManualTimed(context, intent.values);
            } else if constexpr (std::is_same_v<Intent,
                                                FermentationUiStopRunIntent>) {
                return prepareStop(context, intent);
            } else if constexpr (std::is_same_v<
                                     Intent, FermentationUiCompleteRunIntent>) {
                return prepareCompletion(context, intent);
            } else {
                return prepareAdditionalEnvelope(context, intent);
            }
        },
        payload);
}

FermentationApplicationRequestResult FermentationApplication::confirmPrepared(
    const FermentationApplicationRequestResult& prepared) {
    const auto guard = applicationCallSerializer_.enter();
    if (prepared.status != FermentationApplicationRequestStatus::Prepared ||
        !prepared.request.has_value()) {
        return requestFailure(
            FermentationApplicationRequestStatus::Unavailable);
    }
    auto confirmed = prepared;
    if (!revalidatePreparedRequest(*confirmed.request)) {
        return requestFailure(
            FermentationApplicationRequestStatus::Unavailable);
    }
    confirmed.request->confirm();
    return confirmed;
}

FermentationUiCommandResult FermentationApplication::applyConfirmedPrepared(
    const FermentationApplicationPreparedRequest& confirmed) {
    const auto guard = applicationCallSerializer_.enter();
    if (!confirmed.commandEnvelope().confirmed) {
        return FermentationUiCommandBridge::fromCommandStatus(
            CommandStatus::NotConfirmed);
    }
    if (runtimeRunState_ == nullptr || runPersistenceCoordinator_ == nullptr) {
        // No owning path was entered. Keep this a decision-only result instead
        // of mislabelling an unavailable application as a persisted outcome.
        return FermentationUiCommandBridge::fromCommandStatus(
            CommandStatus::ContextMissing);
    }

    CommandDecision decision;
    auto decisionResult = FermentationUiCommandBridge::decidePrepared(
        *runtimeRunState_, confirmed, std::nullopt, &decision);
    if (!std::holds_alternative<CommandStatus>(decisionResult.detail) ||
        std::get<CommandStatus>(decisionResult.detail) !=
            CommandStatus::Proposed) {
        return decisionResult;
    }

    // Message acknowledgement/muting is an existing RAM-owned command path;
    // messages are deliberately outside the run-persistence wire projection.
    if (decision.kind == CommandKind::AcknowledgeMessage ||
        decision.kind == CommandKind::MuteMessage) {
        const auto applied = applyRunCommand(*runtimeRunState_, decision);
        return FermentationUiCommandBridge::fromOwningCommandApplyStatus(
            applied);
    }

    auto checkpointTime = currentCheckpointTime();
    checkpointTime.monotonicMillis =
        confirmed.commandEnvelope().monotonicMillis;
    RunPersistenceResult persisted;
    const auto* liveEvidence = &owningRuntimeEvidence_;
    if (decision.kind == CommandKind::StartProgram ||
        decision.kind == CommandKind::StartManualHolding) {
        FreshStartSnapshotProvenance provenance =
            FreshStartSnapshotProvenance::absent();
        if (configurationService_ != nullptr) {
            const auto runtime = configurationService_->acquireRuntime();
            if (runtime.status ==
                RuntimeConfigurationReadStatus::RuntimeLeaseGranted) {
                provenance = FreshStartSnapshotProvenance::fromRuntimeLease(
                    runtime.lease);
            }
        }
        persisted = runPersistenceCoordinator_->persistFreshStartCommand(
            *runtimeRunState_, decision, provenance, checkpointTime,
            liveEvidence);
    } else {
        persisted = runPersistenceCoordinator_->persistCommand(
            *runtimeRunState_, decision, checkpointTime, liveEvidence);
    }
    return FermentationUiCommandBridge::fromRunPersistenceResult(
        persisted.status);
}

FermentationUiCommandResult FermentationApplication::confirmProductInserted(
    const FermentationUiCommandContext& context) {
    const auto guard = applicationCallSerializer_.enter();
    if (runtimeRunState_ == nullptr || runPersistenceCoordinator_ == nullptr) {
        return FermentationUiCommandBridge::fromCommandStatus(
            CommandStatus::ContextMissing);
    }

    TransitionDecision decision;
    auto decided = FermentationUiCommandBridge::decideProductInsertedConfirmed(
        *runtimeRunState_,
        runtimeRunState_->processRunSnapshot.has_value()
            ? &*runtimeRunState_->processRunSnapshot
            : nullptr,
        context, ProcessSignals{}, context.monotonicMillis, &decision);
    if (!std::holds_alternative<DecisionStatus>(decided.detail) ||
        std::get<DecisionStatus>(decided.detail) != DecisionStatus::Proposed) {
        return decided;
    }

    auto checkpointTime = currentCheckpointTime();
    checkpointTime.monotonicMillis = context.monotonicMillis;
    const auto persisted = runPersistenceCoordinator_->persistTransition(
        *runtimeRunState_, decision, checkpointTime, &owningRuntimeEvidence_);
    return FermentationUiCommandBridge::fromRunPersistenceResult(
        persisted.status);
}

bool FermentationApplication::begin(
    device_platform::IPlatformServices& platformServices,
    const device_platform::IResetCauseSource* resetCauseSource) {
    if (!platformServices.ready()) {
        return false;
    }

    platformServices_ = &platformServices;
    timeSource_ = nullptr;
    stateStore_ = nullptr;
    storageEpoch_.reset();
    runIdentity_.reset();
    configurationRecoveryService_.reset();
    runPersistenceCoordinator_.reset();
    recoveryDisposition_.reset();
    localServiceLease_ = device_platform::ServiceSessionLease{};
    owningRuntimeEvidence_ = CrossRolePlausibilityContext{};
    uiRefreshTracker_ = FermentationUiRefreshRevisionTracker{};
    lifecycleState_ = ApplicationLifecycleState::Ready;
    presentationState_ = PresentationState{};
    presentationState_.resetCause = resetCauseSource == nullptr
                                        ? device_platform::ResetCause::Unknown
                                        : resetCauseSource->resetCause();
    return true;
}

bool FermentationApplication::begin(
    device_platform::IPlatformServices& platformServices,
    device_platform::IStateStore& store,
    const device_platform::ITimeZoneResolver& timeZoneResolver,
    const device_platform::IResetCauseSource* resetCauseSource) {
    return beginPersistent(platformServices, store, timeZoneResolver, nullptr,
                           resetCauseSource);
}

bool FermentationApplication::begin(
    device_platform::IPlatformServices& platformServices,
    device_platform::IStateStore& store,
    const device_platform::ITimeZoneResolver& timeZoneResolver,
    const device_platform::ITimeSource& timeSource,
    const device_platform::IResetCauseSource* resetCauseSource) {
    return beginPersistent(platformServices, store, timeZoneResolver,
                           &timeSource, resetCauseSource, nullptr, nullptr);
}

bool FermentationApplication::begin(
    device_platform::IPlatformServices& platformServices,
    device_platform::IStateStore& store,
    const device_platform::ITimeZoneResolver& timeZoneResolver,
    const device_platform::ITimeSource& timeSource,
    device_platform::INetworkLifecycle& networkLifecycle,
    device_platform::IHttpServerLifecycle& httpServerLifecycle,
    device_platform::ISecureRandomSource& randomSource,
    const device_platform::IResetCauseSource* resetCauseSource) {
    return beginPersistent(platformServices, store, timeZoneResolver,
                           &timeSource, resetCauseSource, &networkLifecycle,
                           &httpServerLifecycle, &randomSource, nullptr,
                           nullptr);
}

bool FermentationApplication::begin(
    device_platform::IPlatformServices& platformServices,
    device_platform::IStateStore& store,
    const device_platform::ITimeZoneResolver& timeZoneResolver,
    const device_platform::ITimeSource& timeSource,
    device_platform::INetworkLifecycle& networkLifecycle,
    device_platform::IHttpServerLifecycle& httpServerLifecycle,
    device_platform::ISecureRandomSource& randomSource,
    IAuthenticationKdf& authenticationKdf,
    const device_platform::IResetCauseSource* resetCauseSource) {
    return beginPersistent(platformServices, store, timeZoneResolver,
                           &timeSource, resetCauseSource, &networkLifecycle,
                           &httpServerLifecycle, &randomSource,
                           &authenticationKdf, nullptr);
}

bool FermentationApplication::begin(
    device_platform::IPlatformServices& platformServices,
    device_platform::IStateStore& store,
    const device_platform::ITimeZoneResolver& timeZoneResolver,
    const device_platform::ITimeSource& timeSource,
    device_platform::INetworkLifecycle& networkLifecycle,
    device_platform::IHttpServerLifecycle& httpServerLifecycle,
    device_platform::ISecureRandomSource& randomSource,
    IAuthenticationKdf& authenticationKdf,
    device_platform::IReplayDigest& replayDigest,
    const device_platform::IResetCauseSource* resetCauseSource) {
    return beginPersistent(platformServices, store, timeZoneResolver,
                           &timeSource, resetCauseSource, &networkLifecycle,
                           &httpServerLifecycle, &randomSource,
                           &authenticationKdf, &replayDigest);
}

bool FermentationApplication::initializeNetwork(
    device_platform::IStateStore& store,
    device_platform::StorageEpoch storageEpoch,
    device_platform::NetworkMode selectedMode,
    const std::string& canonicalDeviceName,
    device_platform::ISecureRandomSource* randomSource,
    device_platform::IReplayDigest* replayDigest) {
    if (networkLifecycle_ == nullptr || httpServerLifecycle_ == nullptr) {
        return true;
    }
    if (randomSource == nullptr) {
        return false;
    }

    connectivityCredentialStore_ = std::unique_ptr<ConnectivityCredentialStore>{
        new (std::nothrow) ConnectivityCredentialStore(store)};
    if (connectivityCredentialStore_ == nullptr) {
        return false;
    }
    networkConfigurationService_ = std::unique_ptr<NetworkConfigurationService>{
        new (std::nothrow) NetworkConfigurationService(
            *connectivityCredentialStore_, *networkLifecycle_, *randomSource)};
    if (networkConfigurationService_ == nullptr) {
        return false;
    }
    networkSetupRoutes_ = std::unique_ptr<NetworkSetupRoutes>{
        new (std::nothrow) NetworkSetupRoutes(*networkConfigurationService_)};
    if (networkSetupRoutes_ == nullptr) {
        return false;
    }
    if (timeSource_ == nullptr) {
        return false;
    }
    if (replayDigest == nullptr) {
        webSessionManager_ = std::unique_ptr<WebSessionManager>{
            new (std::nothrow) WebSessionManager(
                *randomSource, fermentationWebServicePolicy())};
    } else {
        webSessionManager_ = std::unique_ptr<WebSessionManager>{
            new (std::nothrow) WebSessionManager(
                *randomSource, *replayDigest, fermentationWebServicePolicy())};
    }
    if (webSessionManager_ == nullptr) {
        return false;
    }
    webRouteDispatcher_ = std::unique_ptr<WebRouteDispatcher>{
        new (std::nothrow) WebRouteDispatcher(
            *networkSetupRoutes_, *this, *webSessionManager_, *timeSource_)};
    if (webRouteDispatcher_ == nullptr) {
        return false;
    }
    const auto hostname = networkHostnameFromDeviceName(canonicalDeviceName);
    if (!hostname.has_value() ||
        networkLifecycle_->setHostname(*hostname).status !=
            device_platform::NetworkOperationStatus::Applied) {
        return false;
    }
    const auto networkStart = networkConfigurationService_->start(
        selectedMode, storageEpoch, canonicalDeviceName);
    if (networkStart.status == NetworkConfigurationStatus::Applied &&
        !httpServerLifecycle_->start(*webRouteDispatcher_)) {
        static_cast<void>(networkLifecycle_->stop());
        static_cast<void>(httpServerLifecycle_->stop());
        return false;
    }
    if (networkStart.status != NetworkConfigurationStatus::Applied &&
        networkStart.status != NetworkConfigurationStatus::SelectionRequired) {
        static_cast<void>(networkLifecycle_->stop());
        return false;
    }
    return true;
}

NetworkConfigurationResult FermentationApplication::applyNetworkMode(
    device_platform::NetworkMode selectedMode) {
    const auto guard = applicationCallSerializer_.enter();
    if (configurationService_ == nullptr ||
        networkConfigurationService_ == nullptr || stateStore_ == nullptr ||
        !storageEpoch_.has_value() || storageEpoch_->value() == 0U ||
        !device_platform::isSelectableNetworkMode(selectedMode)) {
        return {NetworkConfigurationStatus::InvalidMode};
    }
    const auto previousMode = networkLifecycle_->status().selectedMode;
    auto build = configurationService_->beginPreview();
    if (build.status != ConfigurationPreviewStatus::Success ||
        !build.lease.valid()) {
        return {NetworkConfigurationStatus::PersistenceFailure};
    }
    build.lease.userConfiguration().networkMode = selectedMode;
    const auto installed = configurationService_->installPreview(
        std::move(build.lease), {ChangeOriginKind::LocalDisplay, 2U},
        {ChangeOperationKind::NormalEdit, 1U});
    if (installed.status != ConfigurationPreviewStatus::Success ||
        !installed.preview.has_value()) {
        return {NetworkConfigurationStatus::PersistenceFailure};
    }
    const auto committed =
        configurationService_->confirmPreview(installed.preview->handle);
    if (committed.status != ConfigurationCommitStatus::Activated &&
        committed.status != ConfigurationCommitStatus::NoChange) {
        return {
            committed.status ==
                    ConfigurationCommitStatus::ConfigurationCommitIndeterminate
                ? NetworkConfigurationStatus::CommitIndeterminate
                : NetworkConfigurationStatus::PersistenceFailure};
    }
    const auto runtime = configurationService_->acquireRuntime();
    if (runtime.status != RuntimeConfigurationReadStatus::RuntimeLeaseGranted) {
        return {NetworkConfigurationStatus::PersistenceFailure};
    }
    const auto hostname = networkHostnameFromDeviceName(
        runtime.lease.get().userConfiguration().deviceName);
    if (!hostname.has_value() ||
        networkLifecycle_->setHostname(*hostname).status !=
            device_platform::NetworkOperationStatus::Applied) {
        static_cast<void>(networkLifecycle_->stop());
        if (httpServerLifecycle_ != nullptr) {
            static_cast<void>(httpServerLifecycle_->stop());
        }
        return {NetworkConfigurationStatus::TransportFailure};
    }
    const auto applied = networkConfigurationService_->start(
        selectedMode, runtime.lease.get().storageEpoch(),
        runtime.lease.get().userConfiguration().deviceName);
    if (applied.status != NetworkConfigurationStatus::Applied) {
        static_cast<void>(networkLifecycle_->stop());
        if (httpServerLifecycle_ != nullptr) {
            static_cast<void>(httpServerLifecycle_->stop());
        }
        return applied;
    }
    if (webSessionManager_ != nullptr && previousMode != selectedMode) {
        // The network boundary has changed successfully. Existing browser
        // sessions must not cross into the new transport boundary.
        revokeWebSessionsAtTrustBoundary();
    }
    if (httpServerLifecycle_ != nullptr && !httpServerLifecycle_->running() &&
        webRouteDispatcher_ != nullptr &&
        !httpServerLifecycle_->start(*webRouteDispatcher_)) {
        static_cast<void>(networkLifecycle_->stop());
        return {NetworkConfigurationStatus::TransportFailure};
    }
    storageEpoch_ = runtime.lease.get().storageEpoch();
    return {NetworkConfigurationStatus::Applied};
}

ApplicationConfigurationChangeResult
FermentationApplication::applyDisplayLanguage(
    const std::string& languageId,
    const std::optional<UserConfigurationRevision>& expectedRevision) {
    const auto guard = applicationCallSerializer_.enter();
    using Preview = ConfigurationPreviewStatus;
    using Commit = ConfigurationCommitStatus;
    if (configurationService_ == nullptr) {
        return {Preview::ConfigurationRuntimeUnavailable,
                Commit::ConfigurationRuntimeFailure};
    }
    // Only languages included in this build are selectable. An unknown id is
    // an invalid candidate; no preview slot is taken for it.
    const auto catalog = makeFermentationR1DeviceUiBuildCatalog();
    const bool known = std::any_of(
        catalog.includedLocales.begin(), catalog.includedLocales.end(),
        [&languageId](const device_platform::LocaleId& locale) {
            return locale.value() == languageId;
        });
    if (!known) {
        return {Preview::InvalidCandidate,
                Commit::ConfigurationValidationFailure};
    }
    // Without a decidable revision the change cannot be checked for staleness.
    if (!expectedRevision.has_value()) {
        return {Preview::StateChanged, Commit::ConfigurationConflictFailure};
    }
    return commitUserConfigurationChange(
        *configurationService_, *expectedRevision,
        [&languageId](UserConfiguration& configuration) {
            configuration.displayLanguageId = languageId;
        });
}

ApplicationConfigurationChangeResult FermentationApplication::applyUserSettings(
    const FermentationUiUserSettingsChange& change,
    const std::optional<UserConfigurationRevision>& expectedRevision) {
    const auto guard = applicationCallSerializer_.enter();
    using Preview = ConfigurationPreviewStatus;
    using Commit = ConfigurationCommitStatus;
    // Without the run state "no active run" cannot be proven (O4), and
    // without a service no configuration exists: fail closed.
    if (configurationService_ == nullptr || runtimeRunState_ == nullptr) {
        return {Preview::ConfigurationRuntimeUnavailable,
                Commit::ConfigurationRuntimeFailure};
    }
    if (!change.deviceName.has_value()) {
        return {Preview::InvalidCandidate,
                Commit::ConfigurationValidationFailure};
    }
    // The same active-run predicate the run persistence uses: a program or a
    // manual run exists. Checked before any preview slot is taken.
    if (runtimeRunState_->activeProgramRun.has_value() ||
        runtimeRunState_->activeManualRun.has_value()) {
        return {Preview::NotAllowed, Commit::ConfigurationRuntimeFailure};
    }
    if (validateVisibleName(*change.deviceName) !=
        ConfigurationTextStatus::Success) {
        return {Preview::InvalidCandidate,
                Commit::ConfigurationValidationFailure};
    }
    // Without a decidable revision the change cannot be checked for staleness.
    if (!expectedRevision.has_value()) {
        return {Preview::StateChanged, Commit::ConfigurationConflictFailure};
    }
    return commitUserConfigurationChange(
        *configurationService_, *expectedRevision,
        [&change](UserConfiguration& configuration) {
            configuration.deviceName = *change.deviceName;
        });
}

std::optional<FermentationSensorCommissioningSnapshot>
FermentationApplication::sensorCommissioning() const {
    const auto guard = applicationCallSerializer_.enter();
    if (configurationService_ == nullptr) {
        return std::nullopt;
    }
    const auto runtime = configurationService_->acquireRuntime();
    if (runtime.status != RuntimeConfigurationReadStatus::RuntimeLeaseGranted) {
        return std::nullopt;
    }
    FermentationSensorCommissioningSnapshot snapshot;
    snapshot.revision = runtime.lease.get().serviceConfigurationRevision();
    snapshot.record =
        runtime.lease.get().serviceConfiguration().sensorCommissioning;
    return snapshot;
}

ApplicationConfigurationChangeResult
FermentationApplication::applySensorCommissioning(
    const std::optional<SensorCommissioningRecord>& record,
    const std::optional<ServiceConfigurationRevision>& expectedRevision) {
    const auto guard = applicationCallSerializer_.enter();
    using Preview = ConfigurationPreviewStatus;
    using Commit = ConfigurationCommitStatus;
    // Without the run state "no active run" cannot be proven and without a
    // service no configuration exists: fail closed.
    if (configurationService_ == nullptr || runtimeRunState_ == nullptr) {
        return {Preview::ConfigurationRuntimeUnavailable,
                Commit::ConfigurationRuntimeFailure};
    }
    // A binding or offset change must not reach a running control: refused
    // before any preview slot is taken (same predicate as applyUserSettings).
    if (runtimeRunState_->activeProgramRun.has_value() ||
        runtimeRunState_->activeManualRun.has_value()) {
        return {Preview::NotAllowed, Commit::ConfigurationRuntimeFailure};
    }
    if (record.has_value() && validateSensorCommissioning(*record) !=
                                  SensorCommissioningStatus::Success) {
        return {Preview::InvalidCandidate,
                Commit::ConfigurationValidationFailure};
    }
    if (!expectedRevision.has_value()) {
        return {Preview::StateChanged, Commit::ConfigurationConflictFailure};
    }
    UserConfigurationRevision userRevision{0U};
    {
        const auto runtime = configurationService_->acquireRuntime();
        if (runtime.status !=
            RuntimeConfigurationReadStatus::RuntimeLeaseGranted) {
            return {Preview::ConfigurationRuntimeUnavailable,
                    Commit::ConfigurationRuntimeFailure};
        }
        if (runtime.lease.get().serviceConfigurationRevision() !=
            *expectedRevision) {
            return {Preview::StateChanged,
                    Commit::ConfigurationConflictFailure};
        }
        userRevision = runtime.lease.get().userConfigurationRevision();
    }
    return commitConfigurationChange(
        *configurationService_, userRevision,
        {ChangeOriginKind::InternalSystem, 1U},
        {ChangeOperationKind::NormalEdit, 1U},
        [&record](ConfigurationPreviewBuildLease& lease) {
            lease.serviceConfiguration().sensorCommissioning = record;
        });
}

ApplicationConfigurationChangeResult FermentationApplication::applyProgramEdit(
    const FermentationUiProgramEditRequest& request,
    const std::optional<ProgramCatalogRevision>& expectedProgramCatalogRevision,
    const std::optional<UserConfigurationRevision>&
        expectedUserConfigurationRevision) {
    const auto guard = applicationCallSerializer_.enter();
    using Preview = ConfigurationPreviewStatus;
    using Commit = ConfigurationCommitStatus;
    // Without the run state the usage of a program cannot be proven, and
    // without a service no catalog exists: fail closed.
    if (configurationService_ == nullptr || runtimeRunState_ == nullptr) {
        return {Preview::ConfigurationRuntimeUnavailable,
                Commit::ConfigurationRuntimeFailure};
    }
    // Without both decidable revisions the change cannot be checked for
    // staleness.
    if (!expectedProgramCatalogRevision.has_value() ||
        !expectedUserConfigurationRevision.has_value()) {
        return {Preview::StateChanged, Commit::ConfigurationConflictFailure};
    }
    const auto installed = applyProgramEditPreview(
        *configurationService_, *expectedProgramCatalogRevision, request,
        makeFermentationUiProgramUsageEvidence(*runtimeRunState_));
    if (installed.status != Preview::Success ||
        !installed.preview.has_value()) {
        return {installed.status == Preview::Success ? Preview::InvalidCandidate
                                                     : installed.status,
                Commit::ConfigurationRuntimeFailure};
    }
    // Every exit without an activated change releases the one visible
    // preview slot.
    const auto handle = installed.preview->handle;
    const auto validation =
        configurationService_->validatePreviewForConfirmation(
            handle, *expectedUserConfigurationRevision);
    if (validation.status != Commit::ReadyForConfirmation) {
        static_cast<void>(configurationService_->cancelPreview(handle));
        return {Preview::Success, validation.status};
    }
    const auto committed = configurationService_->confirmPreview(handle);
    if (committed.status != Commit::Activated &&
        committed.status != Commit::NoChange) {
        static_cast<void>(configurationService_->cancelPreview(handle));
    }
    return {Preview::Success, committed.status};
}

NetworkConfigurationResult
FermentationApplication::beginHomeWifiReconfiguration() {
    const auto guard = applicationCallSerializer_.enter();
    if (networkConfigurationService_ == nullptr ||
        configurationService_ == nullptr) {
        return {NetworkConfigurationStatus::NotInitialized};
    }
    const auto runtime = configurationService_->acquireRuntime();
    if (runtime.status != RuntimeConfigurationReadStatus::RuntimeLeaseGranted) {
        return {NetworkConfigurationStatus::PersistenceFailure};
    }
    const auto hostname = networkHostnameFromDeviceName(
        runtime.lease.get().userConfiguration().deviceName);
    if (!hostname.has_value() ||
        networkLifecycle_->setHostname(*hostname).status !=
            device_platform::NetworkOperationStatus::Applied) {
        static_cast<void>(networkLifecycle_->stop());
        if (httpServerLifecycle_ != nullptr) {
            static_cast<void>(httpServerLifecycle_->stop());
        }
        return {NetworkConfigurationStatus::TransportFailure};
    }
    const auto reconfigured =
        networkConfigurationService_->beginHomeWifiReconfiguration(
            runtime.lease.get().userConfiguration().deviceName);
    if (reconfigured.status != NetworkConfigurationStatus::Applied) {
        static_cast<void>(networkLifecycle_->stop());
        if (httpServerLifecycle_ != nullptr) {
            static_cast<void>(httpServerLifecycle_->stop());
        }
        return reconfigured;
    }
    if (webSessionManager_ != nullptr) {
        // Entering HOME_WIFI setup is a trust-boundary transition even
        // before a candidate credential is committed.
        revokeWebSessionsAtTrustBoundary();
    }
    if (httpServerLifecycle_ != nullptr && webRouteDispatcher_ != nullptr &&
        (httpServerLifecycle_->running() ||
         httpServerLifecycle_->start(*webRouteDispatcher_))) {
        return reconfigured;
    }
    static_cast<void>(networkLifecycle_->stop());
    if (httpServerLifecycle_ != nullptr) {
        static_cast<void>(httpServerLifecycle_->stop());
    }
    return {NetworkConfigurationStatus::TransportFailure};
}

std::optional<device_platform::NetworkAccessPointInfo>
FermentationApplication::networkAccessPointInfo() const {
    const auto guard = applicationCallSerializer_.enter();
    if (networkConfigurationService_ == nullptr) {
        return std::nullopt;
    }
    return networkConfigurationService_->accessPointInfo();
}

bool FermentationApplication::networkSetupFlowActive() const noexcept {
    const auto guard = applicationCallSerializer_.enter();
    return networkConfigurationService_ != nullptr &&
           networkConfigurationService_->setupFlowActive();
}

std::uint64_t FermentationApplication::networkAccessPointRevision()
    const noexcept {
    const auto guard = applicationCallSerializer_.enter();
    return networkConfigurationService_ == nullptr
               ? 0U
               : networkConfigurationService_->accessPointInfoRevision();
}

device_platform::NetworkMode FermentationApplication::networkMode()
    const noexcept {
    const auto guard = applicationCallSerializer_.enter();
    if (networkConfigurationService_ == nullptr) {
        return device_platform::NetworkMode::UNSELECTED;
    }
    return networkConfigurationService_->selectedMode();
}

bool FermentationApplication::validSensor(
    const device_platform::SensorQualitySnapshot& snapshot) noexcept {
    return snapshot.quality == device_platform::SensorQuality::Valid;
}

bool FermentationApplication::applicationReadiness() const {
    if (lifecycleState_ != ApplicationLifecycleState::Ready ||
        configurationService_ == nullptr ||
        runPersistenceCoordinator_ == nullptr || runtimeRunState_ == nullptr ||
        !storageEpoch_.has_value() || !persistenceLoadStatus_.has_value()) {
        return false;
    }

    const auto runtime = configurationService_->acquireRuntime();
    if (runtime.status != RuntimeConfigurationReadStatus::RuntimeLeaseGranted ||
        runtime.lease.get().storageEpoch() != *storageEpoch_) {
        return false;
    }

    switch (*persistenceLoadStatus_) {
        case RunPersistenceLoadStatus::NoPersistedRun:
        case RunPersistenceLoadStatus::NoActiveRun:
        case RunPersistenceLoadStatus::Current:
        case RunPersistenceLoadStatus::FallbackRecovered:
            break;
        default:
            return false;
    }
    if (loadDisposition_ == RunLoadDisposition::SafeBoot) {
        return false;
    }

    switch (runPersistenceCoordinator_->state()) {
        case RunPersistenceCoordinatorState::ReadyEmpty:
        case RunPersistenceCoordinatorState::Ready:
        case RunPersistenceCoordinatorState::LoadedActiveRun:
            break;
        default:
            return false;
    }
    return !runtimeRunState_->criticalSafetyEventPending;
}

FermentationApplication::ApplicationRuntimeEvidence
FermentationApplication::resolveRuntimeEvidence() const {
    ApplicationRuntimeEvidence evidence;
    evidence.plausibility = owningRuntimeEvidence_;
    evidence.safetyAllowsStart = applicationReadiness();
    evidence.safetyAllowsCooling = evidence.safetyAllowsStart;
    evidence.airSensorValid = validSensor(evidence.plausibility.air);
    evidence.coolingSensorValid = validSensor(evidence.plausibility.cooling);
    evidence.productSensorValid = validSensor(evidence.plausibility.product);
    return evidence;
}

bool FermentationApplication::revalidatePreparedRequest(
    FermentationApplicationPreparedRequest& request) {
    const auto evidence = resolveRuntimeEvidence();
    bool valid = true;
    const auto productEvidenceRegressed = [&request, &evidence] {
        return request.owningPlausibility_.has_value() &&
               request.owningPlausibility_->product.quality ==
                   device_platform::SensorQuality::Valid &&
               evidence.plausibility.product.quality !=
                   device_platform::SensorQuality::Valid;
    };
    std::visit(
        [&request, &evidence, &valid,
         &productEvidenceRegressed](auto& prepared) {
            using Request = std::decay_t<decltype(prepared)>;
            if constexpr (std::is_same_v<Request, ProgramStartRequest>) {
                prepared.safetyAllowsStart = evidence.safetyAllowsStart;
                prepared.airSensorValid = evidence.airSensorValid;
                prepared.coolingSensorValid = evidence.coolingSensorValid;
                prepared.productSensorValid = evidence.productSensorValid;
                valid = prepared.safetyAllowsStart && prepared.airSensorValid &&
                        prepared.coolingSensorValid &&
                        !(prepared.sensorMode == RunSensorMode::Product &&
                          productEvidenceRegressed());
            } else if constexpr (std::is_same_v<Request, ManualStartRequest>) {
                prepared.safetyAllowsStart = evidence.safetyAllowsStart;
                prepared.airSensorValid = evidence.airSensorValid;
                prepared.coolingSensorValid = evidence.coolingSensorValid;
                prepared.productSensorValid = evidence.productSensorValid;
                valid = prepared.safetyAllowsStart && prepared.airSensorValid &&
                        prepared.coolingSensorValid &&
                        !(prepared.plan.sensorMode == RunSensorMode::Product &&
                          productEvidenceRegressed());
            } else if constexpr (std::is_same_v<Request, StopRequest>) {
                if (prepared.option == StopOption::AbortAndCool) {
                    prepared.safetyAllowsCooling = evidence.safetyAllowsCooling;
                    prepared.airSensorValid = evidence.airSensorValid;
                    prepared.coolingSensorValid = evidence.coolingSensorValid;
                    valid = prepared.safetyAllowsCooling &&
                            prepared.airSensorValid &&
                            prepared.coolingSensorValid;
                }
            } else if constexpr (std::is_same_v<Request, CompletionRequest>) {
                if (prepared.startCooling) {
                    prepared.safetyAllowsCooling = evidence.safetyAllowsCooling;
                    prepared.airSensorValid = evidence.airSensorValid;
                    prepared.coolingSensorValid = evidence.coolingSensorValid;
                    valid = prepared.safetyAllowsCooling &&
                            prepared.airSensorValid &&
                            prepared.coolingSensorValid;
                }
            } else if constexpr (std::is_same_v<
                                     Request, SensorSelectionCommandRequest>) {
                const auto previous = request.owningPlausibility_;
                request.owningPlausibility_ = evidence.plausibility;
                if (!previous.has_value()) {
                    valid = false;
                    return;
                }
                const auto regressed = [](const auto& before, const auto& now) {
                    return before.quality ==
                               device_platform::SensorQuality::Valid &&
                           now.quality != device_platform::SensorQuality::Valid;
                };
                valid = !regressed(previous->air, evidence.plausibility.air) &&
                        !regressed(previous->product,
                                   evidence.plausibility.product) &&
                        !regressed(previous->cooling,
                                   evidence.plausibility.cooling);
            }
        },
        request.storage_);
    return valid;
}

void FermentationApplication::publishOwningRuntimeEvidence(
    const CrossRolePlausibilityContext& evidence) {
    const auto guard = applicationCallSerializer_.enter();
    owningRuntimeEvidence_ = evidence;
}

FermentationUiSnapshot FermentationApplication::uiSnapshot() const {
    FermentationUiSnapshot snapshot;
    refreshUiSnapshot(snapshot);
    return snapshot;
}

void FermentationApplication::refreshUiSnapshot(
    FermentationUiSnapshot& snapshot) const {
    const auto guard = applicationCallSerializer_.enter();
    // The projection input is a reused member: its temperature buffer keeps
    // its capacity, so repeated snapshots do not allocate once warmed up.
    auto& input = uiProjectionInput_;
    input.runState = runtimeRunState_.get();
    input.revisions = FermentationUiExpectedRevisions{};
    if (runtimeRunState_ != nullptr) {
        input.revisions.expectedStateSequence =
            runtimeRunState_->processState.transitionSequence;
        input.revisions.expectedRunRevision = runtimeRunState_->runRevision;
        input.revisions.expectedMessageRevision =
            runtimeRunState_->messageRevision;
        input.revisions.expectedFaultRevision = runtimeRunState_->faultRevision;
        input.revisions.expectedRecoveryEpisodeRevision =
            runtimeRunState_->recoveryEpisodeRevision;
    }
    if (configurationService_ != nullptr) {
        const auto runtime = configurationService_->acquireRuntime();
        if (runtime.status ==
            RuntimeConfigurationReadStatus::RuntimeLeaseGranted) {
            input.revisions.expectedUserConfigurationRevision =
                runtime.lease.get().userConfigurationRevision();
            input.revisions.expectedProgramCatalogRevision =
                runtime.lease.get().programCatalogRevision();
        }
    }

    const auto valueOf = [](const device_platform::SensorQualitySnapshot& value)
        -> std::optional<double> {
        if (value.filteredCelsius.has_value()) {
            return value.filteredCelsius;
        }
        if (value.correctedCelsius.has_value()) {
            return value.correctedCelsius;
        }
        return value.rawCelsius;
    };
    const auto evidence = resolveRuntimeEvidence();
    input.temperatures.clear();
    input.temperatures.push_back({FermentationTemperatureRole::CabinetAir,
                                  valueOf(evidence.plausibility.air),
                                  evidence.plausibility.air});
    input.temperatures.push_back({FermentationTemperatureRole::Product,
                                  valueOf(evidence.plausibility.product),
                                  evidence.plausibility.product});
    input.temperatures.push_back({FermentationTemperatureRole::Cooling,
                                  valueOf(evidence.plausibility.cooling),
                                  evidence.plausibility.cooling});
    input.recoveryDisposition = recoveryDisposition_;
    input.persistenceLoadStatus = persistenceLoadStatus_;
    input.coordinatorState.reset();
    if (runPersistenceCoordinator_ != nullptr) {
        input.coordinatorState = runPersistenceCoordinator_->state();
    }
    input.application.lifecycleState = lifecycleState_;
    input.application.presentation = presentationState_;
    input.network.currentMode = networkMode();
    input.webAccess = webAccessState();
    // Only the lease plus the single entry predicate open the service area;
    // the UI falls back to its generic locked reason (no per-tick text key).
    input.service.available = localServiceAvailableUnlocked();
    input.factoryReset = factoryResetView(
        timeSource_ != nullptr ? timeSource_->monotonicMillis() : 0U);
    input.refreshTracker = &uiRefreshTracker_;
    FermentationUiProjector::projectInto(snapshot, input);
}

std::optional<FermentationUiPresentationSource>
FermentationApplication::uiPresentationSource() const {
    const auto guard = applicationCallSerializer_.enter();
    if (configurationService_ == nullptr) {
        return std::nullopt;
    }
    const auto runtime = configurationService_->acquireRuntime();
    if (runtime.status != RuntimeConfigurationReadStatus::RuntimeLeaseGranted) {
        return std::nullopt;
    }
    FermentationUiPresentationSource source;
    const auto& userConfiguration = runtime.lease.get().userConfiguration();
    if (!userConfiguration.displayLanguageId.empty()) {
        source.displayLocale =
            device_platform::LocaleId{userConfiguration.displayLanguageId};
    }
    source.canonicalTimeZoneId = device_platform::TimeZoneId{
        runtime.lease.get().preparedTimeZone().canonicalIdentifier};
    source.timeZoneRule = runtime.lease.get().preparedTimeZone().rule;
    source.deviceName = userConfiguration.deviceName;
    source.programCatalog = runtime.lease.get().programCatalog();
    return source;
}

WebAuthenticationState FermentationApplication::webAuthenticationStateUnlocked()
    const {
    if (authenticationDomain_ == nullptr ||
        !authenticationContext_.has_value() ||
        authenticationResolutionStatus_ !=
            AuthenticationBootstrapResolutionStatus::Ready) {
        return WebAuthenticationState::Indeterminate;
    }
    authenticationBootstrapStatus_ =
        authenticationDomain_->inspect(*authenticationContext_);
    switch (authenticationBootstrapStatus_) {
        case AuthBootstrapStatus::BootstrapAllowed:
            return WebAuthenticationState::Unprovisioned;
        case AuthBootstrapStatus::AlreadyProvisioned: {
            const auto enabled = authenticationDomain_->webPasswordEnabled(
                *authenticationContext_);
            if (!enabled.has_value()) {
                return WebAuthenticationState::RecoveryRequired;
            }
            return *enabled ? WebAuthenticationState::PasswordProtected
                            : WebAuthenticationState::PasswordDisabled;
        }
        default:
            return WebAuthenticationState::RecoveryRequired;
    }
}

WebAuthenticationState FermentationApplication::webAuthenticationState() const {
    const auto guard = applicationCallSerializer_.enter();
    return webAuthenticationStateUnlocked();
}

WebAuthenticationResult FermentationApplication::authenticateWebPassword(
    const std::string& password, std::uint64_t nowMs) {
    AuthenticationDomain* domain = nullptr;
    std::optional<AuthenticationBootstrapContext> context;
    std::optional<AuthOperationGate::Token> token;
    std::uint64_t trustGeneration = 0U;
    {
        const auto guard = applicationCallSerializer_.enter();
        trustGeneration = webTrustGeneration_;
        if (webAuthenticationStateUnlocked() !=
                WebAuthenticationState::PasswordProtected ||
            authenticationDomain_ == nullptr ||
            !authenticationContext_.has_value()) {
            const auto state = webAuthenticationStateUnlocked();
            return {state == WebAuthenticationState::PasswordDisabled
                        ? WebAuthenticationResultStatus::Disabled
                        : WebAuthenticationResultStatus::RecoveryRequired,
                    0U, trustGeneration};
        }
        token = authOperationGate_.tryBegin();
        if (!token.has_value()) {
            return {WebAuthenticationResultStatus::RecoveryRequired, 0U,
                    trustGeneration};
        }
        domain = authenticationDomain_.get();
        context = authenticationContext_;
    }

    // The domain pointer and the context copy are valid only while the token
    // is active. The Application gate is released here, so the multi-second
    // PBKDF2 does not block other callers; reset and destruction wait for the
    // token instead.
    std::uint64_t retryAfterMs = 0U;
    const auto checked =
        domain->verifyWebPassword(*context, password, nowMs, retryAfterMs);
    token.reset();
    domain = nullptr;
    context.reset();

    switch (checked) {
        case AuthCheckStatus::Authenticated:
            return {WebAuthenticationResultStatus::Authenticated, 0U,
                    trustGeneration};
        case AuthCheckStatus::Disabled:
            return {WebAuthenticationResultStatus::Disabled, 0U,
                    trustGeneration};
        case AuthCheckStatus::Invalid:
            return retryAfterMs == 0U
                       ? WebAuthenticationResult{WebAuthenticationResultStatus::
                                                     Invalid,
                                                 0U, trustGeneration}
                       : WebAuthenticationResult{
                             WebAuthenticationResultStatus::LockedOut,
                             retryAfterMs, trustGeneration};
        case AuthCheckStatus::LockedOut:
            return {WebAuthenticationResultStatus::LockedOut, retryAfterMs,
                    trustGeneration};
        case AuthCheckStatus::RecoveryRequired:
            return {WebAuthenticationResultStatus::RecoveryRequired, 0U,
                    trustGeneration};
        case AuthCheckStatus::KdfUnavailable:
            return {WebAuthenticationResultStatus::KdfUnavailable, 0U,
                    trustGeneration};
    }
    return {WebAuthenticationResultStatus::RecoveryRequired, 0U,
            trustGeneration};
}

WebSessionIssueResult FermentationApplication::issueWebSession(
    const WebAuthenticationResult& authentication, std::uint64_t nowMs) {
    WebAuthenticationState expected = WebAuthenticationState::Indeterminate;
    if (authentication.status == WebAuthenticationResultStatus::Authenticated) {
        expected = WebAuthenticationState::PasswordProtected;
    } else if (authentication.status ==
               WebAuthenticationResultStatus::Disabled) {
        expected = WebAuthenticationState::PasswordDisabled;
    } else {
        return {WebSessionIssueStatus::TrustBoundaryChanged, {}};
    }

    // Recheck and create under the Application gate: every trust-boundary
    // revocation (network mode change, HOME_WIFI reconfiguration, factory
    // reset) runs under the same gate, so it either precedes this call and
    // makes the decision stale, or follows it and revokes the new session.
    const auto guard = applicationCallSerializer_.enter();
    if (webSessionManager_ == nullptr) {
        return {WebSessionIssueStatus::Unavailable, {}};
    }
    if (authentication.trustGeneration != webTrustGeneration_ ||
        webAuthenticationStateUnlocked() != expected) {
        return {WebSessionIssueStatus::TrustBoundaryChanged, {}};
    }
    auto created = webSessionManager_->create(nowMs);
    if (created.status != WebSessionStatus::Created ||
        !created.handle.has_value()) {
        return {WebSessionIssueStatus::Unavailable, {}};
    }
    return {WebSessionIssueStatus::Created, std::move(created)};
}

void FermentationApplication::revokeWebSessionsAtTrustBoundary() noexcept {
    ++webTrustGeneration_;
    // A trust boundary also ends the local release for the web setup.
    closeWebProvisioningWindow();
    if (webSessionManager_ != nullptr) {
        webSessionManager_->revokeAll();
    }
}

void FermentationApplication::closeWebProvisioningWindow() noexcept {
    webProvisioningWindowOpen_ = false;
    webProvisioningWindowOpenedAtMs_ = 0U;
}

bool FermentationApplication::webProvisioningWindowOpenUnlocked() noexcept {
    if (!webProvisioningWindowOpen_) {
        return false;
    }
    if (timeSource_ == nullptr) {
        closeWebProvisioningWindow();
        return false;
    }
    const auto nowMs = timeSource_->monotonicMillis();
    // Fail closed on a backward observation; otherwise compare elapsed time.
    if (nowMs < webProvisioningWindowOpenedAtMs_ ||
        nowMs - webProvisioningWindowOpenedAtMs_ >= kWebProvisioningWindowMs) {
        closeWebProvisioningWindow();
        return false;
    }
    return true;
}

bool FermentationApplication::webProvisioningWindowStillOpenUnlocked()
    const noexcept {
    if (!webProvisioningWindowOpen_ || timeSource_ == nullptr) {
        return false;
    }
    const auto nowMs = timeSource_->monotonicMillis();
    return nowMs >= webProvisioningWindowOpenedAtMs_ &&
           nowMs - webProvisioningWindowOpenedAtMs_ < kWebProvisioningWindowMs;
}

FermentationWebAccessState FermentationApplication::webAccessState() const {
    const auto guard = applicationCallSerializer_.enter();
    if (authenticationDomain_ == nullptr ||
        !authenticationContext_.has_value() ||
        authenticationResolutionStatus_ !=
            AuthenticationBootstrapResolutionStatus::Ready ||
        authenticationBootstrapStatus_ !=
            AuthBootstrapStatus::BootstrapAllowed) {
        return FermentationWebAccessState::NotApplicable;
    }
    return webProvisioningWindowStillOpenUnlocked()
               ? FermentationWebAccessState::WindowOpen
               : FermentationWebAccessState::Closed;
}

// --- Local service area (Issue #188 A) -------------------------------------

bool FermentationApplication::localServiceEntryAllowedUnlocked()
    const noexcept {
    // Validated STANDBY only: a ServiceRequired lifecycle, a run, an open
    // recovery decision or a missing configuration runtime never qualifies,
    // even while the run state still reports Standby.
    return lifecycleState_ == ApplicationLifecycleState::Ready &&
           runtimeRunState_ != nullptr &&
           runtimeRunState_->processState.state == ProcessState::Standby &&
           factoryResetRunGateOpenUnlocked() &&
           pendingRecoverySource_ == nullptr &&
           !recoveryDisposition_.has_value() &&
           (runPersistenceCoordinator_ == nullptr ||
            runPersistenceCoordinator_->state() !=
                RunPersistenceCoordinatorState::FallbackRecoveryPending) &&
           storageEpoch_.has_value();
}

bool FermentationApplication::localServiceAvailableUnlocked() const noexcept {
    return timeSource_ != nullptr && localServiceEntryAllowedUnlocked() &&
           localServiceLease_.activeAt(timeSource_->monotonicMillis());
}

LocalServicePinResult FermentationApplication::verifyLocalServicePin(
    const std::string& pin) {
    AuthenticationDomain* domain = nullptr;
    std::optional<AuthenticationBootstrapContext> context;
    std::optional<AuthOperationGate::Token> token;
    std::uint64_t trustGeneration = 0U;
    std::uint64_t nowMs = 0U;
    {
        const auto guard = applicationCallSerializer_.enter();
        if (timeSource_ == nullptr) {
            return {LocalServicePinStatus::Unavailable, 0U};
        }
        // Decided before any KDF run or authentication write.
        if (!localServiceEntryAllowedUnlocked()) {
            return {LocalServicePinStatus::NotAllowedInState, 0U};
        }
        switch (webAuthenticationStateUnlocked()) {
            case WebAuthenticationState::PasswordProtected:
            case WebAuthenticationState::PasswordDisabled:
                // Both modes keep the Service-PIN protection.
                break;
            case WebAuthenticationState::Unprovisioned:
                return {LocalServicePinStatus::NotProvisioned, 0U};
            case WebAuthenticationState::RecoveryRequired:
            case WebAuthenticationState::Indeterminate:
                return {LocalServicePinStatus::Unavailable, 0U};
        }
        if (authenticationDomain_ == nullptr ||
            !authenticationContext_.has_value()) {
            return {LocalServicePinStatus::Unavailable, 0U};
        }
        token = authOperationGate_.tryBegin();
        if (!token.has_value()) {
            return {LocalServicePinStatus::Unavailable, 0U};
        }
        domain = authenticationDomain_.get();
        context = authenticationContext_;
        trustGeneration = webTrustGeneration_;
        nowMs = timeSource_->monotonicMillis();
    }

    // The domain pointer and the context copy are valid only while the token
    // is active; the Application gate is released for the slow PBKDF2.
    std::uint64_t retryAfterMs = 0U;
    const auto checked =
        domain->verifyServicePin(*context, pin, nowMs, retryAfterMs);
    const auto checkedEpoch = context->storageEpoch();
    const auto checkedBootstrapSequence = context->bootstrapSequence();
    // Release the token before the Application gate is entered again: a
    // factory reset or authentication re-initialisation drains the gate
    // while holding the Application gate.
    token.reset();
    domain = nullptr;
    context.reset();

    switch (checked) {
        case AuthCheckStatus::Authenticated:
            break;
        case AuthCheckStatus::Invalid:
            return retryAfterMs == 0U
                       ? LocalServicePinResult{LocalServicePinStatus::Invalid,
                                               0U}
                       : LocalServicePinResult{LocalServicePinStatus::LockedOut,
                                               retryAfterMs};
        case AuthCheckStatus::LockedOut:
            return {LocalServicePinStatus::LockedOut, retryAfterMs};
        case AuthCheckStatus::Disabled:
        case AuthCheckStatus::RecoveryRequired:
        case AuthCheckStatus::KdfUnavailable:
            return {LocalServicePinStatus::Unavailable, 0U};
    }

    // Fail closed: the decision is only valid for the same entry state, the
    // same authentication context and the same trust generation it was taken
    // in (a domain replacement within one epoch bumps the generation).
    const auto guard = applicationCallSerializer_.enter();
    if (!localServiceEntryAllowedUnlocked()) {
        return {LocalServicePinStatus::NotAllowedInState, 0U};
    }
    if (timeSource_ == nullptr || authenticationDomain_ == nullptr ||
        !authenticationContext_.has_value() ||
        authenticationResolutionStatus_ !=
            AuthenticationBootstrapResolutionStatus::Ready ||
        authenticationContext_->storageEpoch() != checkedEpoch ||
        authenticationContext_->bootstrapSequence() !=
            checkedBootstrapSequence ||
        webTrustGeneration_ != trustGeneration) {
        return {LocalServicePinStatus::Unavailable, 0U};
    }
    const auto grantedAtMs = timeSource_->monotonicMillis();
    localServiceLease_ = device_platform::ServiceSessionLease(
        fermentationTouchServicePolicy(), grantedAtMs);
    if (!localServiceLease_.activeAt(grantedAtMs)) {
        return {LocalServicePinStatus::Unavailable, 0U};
    }
    return {LocalServicePinStatus::Authorized, 0U};
}

void FermentationApplication::endLocalServiceSession() {
    const auto guard = applicationCallSerializer_.enter();
    localServiceLease_ = device_platform::ServiceSessionLease{};
}

void FermentationApplication::noteLocalServiceActivity() {
    const auto guard = applicationCallSerializer_.enter();
    if (!localServiceAvailableUnlocked()) {
        return;
    }
    localServiceLease_.observe(
        device_platform::ServiceSessionEvent::RelevantUserActivity,
        timeSource_->monotonicMillis());
}

bool FermentationApplication::openWebProvisioningWindow() {
    const auto guard = applicationCallSerializer_.enter();
    if (timeSource_ == nullptr || authenticationDomain_ == nullptr ||
        !authenticationContext_.has_value() ||
        webAuthenticationStateUnlocked() !=
            WebAuthenticationState::Unprovisioned ||
        authenticationDomain_->inspect(*authenticationContext_) !=
            AuthBootstrapStatus::BootstrapAllowed) {
        closeWebProvisioningWindow();
        return false;
    }
    if (webProvisioningWindowOpenUnlocked()) {
        return false;
    }
    webProvisioningWindowOpen_ = true;
    webProvisioningWindowOpenedAtMs_ = timeSource_->monotonicMillis();
    return true;
}

WebProvisionStatus FermentationApplication::projectProvisionResult(
    AuthBootstrapStatus bootstrapResult,
    AuthBootstrapStatus reinspected) noexcept {
    switch (bootstrapResult) {
        case AuthBootstrapStatus::BootstrapAllowed:
            return WebProvisionStatus::Provisioned;
        case AuthBootstrapStatus::InvalidInput:
            return WebProvisionStatus::InvalidCredentials;
        case AuthBootstrapStatus::RecoveryRequired:
            // A concurrent request that completed first leaves the root
            // provisioned; that is a conflict, not a recovery condition.
            return reinspected == AuthBootstrapStatus::AlreadyProvisioned
                       ? WebProvisionStatus::AlreadyProvisioned
                       : WebProvisionStatus::RecoveryRequired;
        case AuthBootstrapStatus::KdfUnavailable:
        case AuthBootstrapStatus::PersistenceFailure:
            return WebProvisionStatus::Failed;
        case AuthBootstrapStatus::CommitOutcomeUnknown:
        case AuthBootstrapStatus::NotProvisioned:
        case AuthBootstrapStatus::AlreadyProvisioned:
        case AuthBootstrapStatus::AuthenticationDenied:
        case AuthBootstrapStatus::LockedOut:
        case AuthBootstrapStatus::AuthenticationDisabled:
            return WebProvisionStatus::RecoveryRequired;
    }
    return WebProvisionStatus::RecoveryRequired;
}

WebProvisionStatus FermentationApplication::provisionWebAccess(
    WebProvisionMode mode, const std::string& webPassword,
    const std::string& servicePin) {
    AuthenticationDomain* domain = nullptr;
    std::optional<AuthenticationBootstrapContext> context;
    std::optional<AuthOperationGate::Token> token;
    std::uint64_t trustGeneration = 0U;
    {
        const auto guard = applicationCallSerializer_.enter();
        const auto state = webAuthenticationStateUnlocked();
        if (state != WebAuthenticationState::Unprovisioned) {
            // Not Unprovisioned + BootstrapAllowed any more: the release ends.
            closeWebProvisioningWindow();
            return state == WebAuthenticationState::PasswordProtected ||
                           state == WebAuthenticationState::PasswordDisabled
                       ? WebProvisionStatus::AlreadyProvisioned
                       : WebProvisionStatus::RecoveryRequired;
        }
        if (authenticationDomain_ == nullptr ||
            !authenticationContext_.has_value() ||
            authenticationDomain_->inspect(*authenticationContext_) !=
                AuthBootstrapStatus::BootstrapAllowed) {
            closeWebProvisioningWindow();
            return WebProvisionStatus::RecoveryRequired;
        }
        if (!webProvisioningWindowOpenUnlocked()) {
            return WebProvisionStatus::NotAllowed;
        }
        trustGeneration = webTrustGeneration_;
        token = authOperationGate_.tryBegin();
        if (!token.has_value()) {
            return WebProvisionStatus::RecoveryRequired;
        }
        domain = authenticationDomain_.get();
        context = authenticationContext_;
    }

    // Fail closed before the first use outside the serializer block.
    if (domain == nullptr || !context.has_value() || !token.has_value()) {
        return WebProvisionStatus::RecoveryRequired;
    }

    // The domain pointer and the context copy are valid only while the token
    // is active (see AuthOperationGate). The Application gate is not held.
    const auto passwordMode = mode == WebProvisionMode::Protect
                                  ? WebPasswordMode::Protected
                                  : WebPasswordMode::Disabled;
    const auto bootstrapResult =
        domain->bootstrap(*context, device_platform::UiSurface::WebInterface,
                          true, webPassword, servicePin, passwordMode);
    auto reinspected = AuthBootstrapStatus::RecoveryRequired;
    if (bootstrapResult == AuthBootstrapStatus::RecoveryRequired) {
        reinspected = domain->inspect(*context);
    }
    token.reset();
    domain = nullptr;
    context.reset();

    const auto projected = projectProvisionResult(bootstrapResult, reinspected);

    // Re-take the Application gate only after the token ended and conclude
    // the operation once against the current state (no old domain pointer).
    const auto guard = applicationCallSerializer_.enter();
    if (projected != WebProvisionStatus::Provisioned) {
        // Retryable failures (invalid input, persistence failure before the
        // first root write) leave Unprovisioned + BootstrapAllowed and keep a
        // still valid window; every recovery or unclear state ends it.
        if (webAuthenticationStateUnlocked() !=
                WebAuthenticationState::Unprovisioned ||
            authenticationDomain_ == nullptr ||
            !authenticationContext_.has_value() ||
            authenticationDomain_->inspect(*authenticationContext_) !=
                AuthBootstrapStatus::BootstrapAllowed) {
            closeWebProvisioningWindow();
        }
        return projected;
    }

    // A trust boundary (factory reset, network boundary) between bootstrap
    // and here makes the old success stale: it must not be reported for a new
    // epoch.
    const auto expected = mode == WebProvisionMode::Protect
                              ? WebAuthenticationState::PasswordProtected
                              : WebAuthenticationState::PasswordDisabled;
    if (trustGeneration != webTrustGeneration_ ||
        webAuthenticationStateUnlocked() != expected) {
        return WebProvisionStatus::RecoveryRequired;
    }
    // Consumes the window and revokes any existing session.
    revokeWebSessionsAtTrustBoundary();
    return WebProvisionStatus::Provisioned;
}

bool FermentationApplication::beginPersistent(
    device_platform::IPlatformServices& platformServices,
    device_platform::IStateStore& store,
    const device_platform::ITimeZoneResolver& timeZoneResolver,
    const device_platform::ITimeSource* timeSource,
    const device_platform::IResetCauseSource* resetCauseSource,
    device_platform::INetworkLifecycle* networkLifecycle,
    device_platform::IHttpServerLifecycle* httpServerLifecycle,
    device_platform::ISecureRandomSource* randomSource,
    IAuthenticationKdf* authenticationKdf,
    device_platform::IReplayDigest* replayDigest) {
    if (!platformServices.ready()) {
        return false;
    }

    platformServices_ = &platformServices;
    timeSource_ = timeSource;
    stateStore_ = &store;
    networkLifecycle_ = networkLifecycle;
    httpServerLifecycle_ = httpServerLifecycle;
    secureRandomSource_ = randomSource;
    authenticationKdf_ = authenticationKdf;
    storageEpoch_.reset();
    noRuntimeResetAdmitted_ = false;
    runIdentity_.reset();
    lifecycleState_ = ApplicationLifecycleState::Initializing;
    presentationState_ = PresentationState{};
    presentationState_.resetCause = resetCauseSource == nullptr
                                        ? device_platform::ResetCause::Unknown
                                        : resetCauseSource->resetCause();
    persistenceLoadStatus_.reset();
    loadDisposition_ = RunLoadDisposition::SafeBoot;
#if defined(APP_ISSUE_90_SLICE7_HARNESS)
    configurationRecoveryStatus_.reset();
#endif
    pendingResume_.reset();
    pendingFallbackResume_.reset();
    owningRecoveryEvidence_.reset();
    owningRuntimeEvidence_ = CrossRolePlausibilityContext{};
    uiRefreshTracker_ = FermentationUiRefreshRevisionTracker{};
    closeWebProvisioningWindow();
    pendingRecoverySource_.reset();
    recoveryDisposition_.reset();
    runtimeRunState_.reset();
    runPersistenceCoordinator_.reset();
    configurationRecoveryService_.reset();
    networkSetupRoutes_.reset();
    webRouteDispatcher_.reset();
    webSessionManager_.reset();
    networkConfigurationService_.reset();
    connectivityCredentialStore_.reset();
    authenticationContext_.reset();
    authenticationDomain_.reset();
    authenticationRecordStore_.reset();
    authenticationResolutionStatus_ =
        AuthenticationBootstrapResolutionStatus::RecoveryRequired;
    authenticationBootstrapStatus_ = AuthBootstrapStatus::RecoveryRequired;
    configurationService_.reset();
    graphStore_.reset();
    mutationCoordinator_.reset();
    bootstrapStore_.reset();

    bootstrapStore_ = std::unique_ptr<ConfigurationBootstrapStore>{
        new (std::nothrow) ConfigurationBootstrapStore(store)};
    if (bootstrapStore_ == nullptr) {
        requireService(FaultCode::None, true);
        return true;
    }

    mutationCoordinator_ = std::unique_ptr<ConfigurationMutationCoordinator>{
        new (std::nothrow) ConfigurationMutationCoordinator()};
    if (mutationCoordinator_ == nullptr) {
        requireService(FaultCode::None, true);
        return true;
    }

    graphStore_ = std::unique_ptr<ConfigurationGraphStore>{
        new (std::nothrow) ConfigurationGraphStore(store, timeZoneResolver)};
    if (graphStore_ == nullptr) {
        requireService(FaultCode::None, true);
        return true;
    }

    configurationService_ = std::unique_ptr<ConfigurationService>{
        new (std::nothrow) ConfigurationService(
            *mutationCoordinator_, *graphStore_, timeZoneResolver)};
    if (configurationService_ == nullptr) {
        requireService(FaultCode::None, true);
        return true;
    }

    auto recovery = ConfigurationRecoveryService::create(
        store, *bootstrapStore_, *graphStore_, *configurationService_,
        *mutationCoordinator_);
    if (recovery == nullptr) {
        requireService(FaultCode::ConfigurationUnavailable);
        return true;
    }

    configurationRecoveryService_ = std::move(recovery);
    const auto configurationResult = configurationRecoveryService_->boot();
#if defined(APP_ISSUE_90_SLICE7_HARNESS)
    configurationRecoveryStatus_ = configurationResult.status;
#endif
    auto authorizedRunEpochHandoff =
        configurationRecoveryService_->takeAuthorizedRunEpochHandoffProof();
    const auto runtime = configurationService_->acquireRuntime();
    if (runtime.status != RuntimeConfigurationReadStatus::RuntimeLeaseGranted) {
        // One-time admission for the no-runtime factory reset: the recovery
        // core latched ResetEligibleNoRuntime and no run-epoch handoff is
        // open. Evaluated here, never on a UI tick.
        noRuntimeResetAdmitted_ =
            configurationService_->mode() ==
                ConfigurationServiceMode::ResetEligibleNoRuntime &&
            noRuntimeResetBootstrapEpochUnlocked().has_value();
        requireService(configurationFault(configurationResult.status));
        return true;
    }
    const auto epoch = runtime.lease.get().storageEpoch();
    storageEpoch_ = epoch;

    initializeAuthentication(store);
    if (!initializeNetwork(store, epoch,
                           runtime.lease.get().userConfiguration().networkMode,
                           runtime.lease.get().userConfiguration().deviceName,
                           secureRandomSource_, replayDigest)) {
        // Network setup is optional for application readiness. The core
        // persistence and safety path continues independently.
    }

    runPersistenceCoordinator_ = std::unique_ptr<RunPersistenceCoordinator>{
        new (std::nothrow)
            RunPersistenceCoordinator(store, epoch, RunCheckpointSchedule{})};
    if (runPersistenceCoordinator_ == nullptr) {
        requireService(FaultCode::None, true);
        return true;
    }

    if (authorizedRunEpochHandoff.has_value() &&
        !completeAuthorizedEpochHandoff(*configurationRecoveryService_,
                                        *runPersistenceCoordinator_,
                                        *authorizedRunEpochHandoff)) {
        requireService(FaultCode::RunPersistenceUntrusted);
        return true;
    }

    auto loadResult = std::unique_ptr<RunPersistenceLoadResult>{
        new (std::nothrow) RunPersistenceLoadResult{}};
    if (loadResult == nullptr) {
        requireService(FaultCode::None, true);
        return true;
    }
    runPersistenceCoordinator_->loadAndInitializeInto(*loadResult);

    if (const auto highWater = runPersistenceCoordinator_->commandIdHighWater();
        highWater.has_value()) {
        auto identity = ApplicationRunIdentity::create(epoch, highWater);
        if (!identity.has_value()) {
            requireService(FaultCode::RunPersistenceUntrusted);
            return true;
        }
        runIdentity_ = std::unique_ptr<ApplicationRunIdentity>{
            new (std::nothrow) ApplicationRunIdentity(std::move(*identity))};
        if (runIdentity_ == nullptr) {
            requireService(FaultCode::None, true);
            return true;
        }
    }

    persistenceLoadStatus_ = loadResult->status;
    const RunPersistenceSnapshot* snapshot =
        loadResult->snapshot.has_value() ? loadResult->snapshot.operator->()
                                         : nullptr;
    loadDisposition_ =
        boot_classification::classifyRunLoad(loadResult->status, snapshot);
    const auto classification =
        boot_classification::classify(loadResult->status, snapshot);

    const RunCheckpointTime bootTime = currentCheckpointTime();
    if (!processBootClassification(classification, snapshot, bootTime)) {
        return true;
    }

    lifecycleState_ = ApplicationLifecycleState::Ready;
    return true;
}

void FermentationApplication::initializeAuthentication(
    device_platform::IStateStore& store) {
    // resetAuthenticationState() closes and drains the auth gate; it is
    // reopened on every exit once the new domain (or its absence) is final.
    struct ReopenOnExit {
        explicit ReopenOnExit(AuthOperationGate& authGate) : gate(authGate) {}
        ReopenOnExit(const ReopenOnExit&) = delete;
        ReopenOnExit& operator=(const ReopenOnExit&) = delete;
        ReopenOnExit(ReopenOnExit&&) = delete;
        ReopenOnExit& operator=(ReopenOnExit&&) = delete;
        ~ReopenOnExit() { gate.reopen(); }
        AuthOperationGate& gate;
    };
    const ReopenOnExit reopenOnExit(authOperationGate_);
    resetAuthenticationState();

    if (authenticationKdf_ == nullptr || secureRandomSource_ == nullptr) {
        return;
    }
    authenticationRecordStore_ = std::unique_ptr<AuthenticationRecordStore>{
        new (std::nothrow) AuthenticationRecordStore(store)};
    if (authenticationRecordStore_ == nullptr) {
        return;
    }
    authenticationDomain_ = std::unique_ptr<AuthenticationDomain>{
        new (std::nothrow)
            AuthenticationDomain(*authenticationRecordStore_,
                                 *authenticationKdf_, *secureRandomSource_)};
    if (authenticationDomain_ == nullptr) {
        return;
    }
    const auto resolved =
        configurationRecoveryService_->resolveAuthenticationBootstrap(
            *authenticationRecordStore_);
    authenticationResolutionStatus_ = resolved.status;
    if (resolved.status != AuthenticationBootstrapResolutionStatus::Ready ||
        !resolved.context.has_value()) {
        return;
    }
    authenticationContext_ = *resolved.context;
    authenticationBootstrapStatus_ =
        authenticationDomain_->inspect(*authenticationContext_);
}

void FermentationApplication::resetAuthenticationState() noexcept {
    // No authentication operation may use the domain or record store while
    // they are destroyed. Boot-time callers have no active operation, so the
    // drain returns immediately; the runtime reset closed the gate already.
    authOperationGate_.closeAndDrain();
    // Any replacement of the authentication domain also ends the validity of
    // pending authentication decisions.
    ++webTrustGeneration_;
    closeWebProvisioningWindow();
    localServiceLease_ = device_platform::ServiceSessionLease{};
    authenticationContext_.reset();
    authenticationDomain_.reset();
    authenticationRecordStore_.reset();
    authenticationResolutionStatus_ =
        AuthenticationBootstrapResolutionStatus::RecoveryRequired;
    authenticationBootstrapStatus_ = AuthBootstrapStatus::RecoveryRequired;
}

bool FermentationApplication::processBootClassification(
    BootClassification classification, const RunPersistenceSnapshot* snapshot,
    const RunCheckpointTime& bootTime) {
    switch (classification) {
        case BootClassification::NoRun:
            return publishStandby();
        case BootClassification::ResumeOffer:
            return prepareResumeOffer(snapshot);
        case BootClassification::RecoveryEvaluation:
            return evaluateCurrentRecovery(snapshot, bootTime);
        case BootClassification::FallbackSelectionRequired:
            return prepareFallbackSelection(snapshot);
        case BootClassification::DiscardableRun:
        case BootClassification::CompletedRun:
        case BootClassification::TerminalRunFault:
            return processTerminalClassification(classification, snapshot,
                                                 bootTime);
        case BootClassification::SafeBoot:
        case BootClassification::Unresolved:
            requireService(FaultCode::RunPersistenceUntrusted);
            return false;
    }
    requireService(FaultCode::RunPersistenceUntrusted);
    return false;
}

bool FermentationApplication::prepareResumeOffer(
    const RunPersistenceSnapshot* snapshot) {
    if (snapshot == nullptr) {
        requireService(FaultCode::RunPersistenceUntrusted);
        return false;
    }
    pendingResume_ =
        std::unique_ptr<RunCommandState>{new (std::nothrow) RunCommandState{}};
    if (pendingResume_ == nullptr) {
        requireService(FaultCode::None, true);
        return false;
    }
    if (!restoreRunPersistenceSnapshotInto(*snapshot, *pendingResume_)) {
        pendingResume_.reset();
        requireService(FaultCode::RunPersistenceUntrusted);
        return false;
    }
    return true;
}

bool FermentationApplication::prepareFallbackSelection(
    const RunPersistenceSnapshot* snapshot) {
    if (snapshot == nullptr) {
        requireService(FaultCode::RunPersistenceUntrusted);
        return false;
    }
    pendingFallbackResume_ =
        std::unique_ptr<RunCommandState>{new (std::nothrow) RunCommandState{}};
    if (pendingFallbackResume_ == nullptr ||
        !restoreRunPersistenceSnapshotInto(*snapshot,
                                           *pendingFallbackResume_)) {
        pendingFallbackResume_.reset();
        requireService(FaultCode::RunPersistenceUntrusted);
        return false;
    }
    // The unresolved fallback is retained for an explicit user choice.  It
    // is never published as an active runtime state and therefore cannot
    // satisfy the actuator interlock before a successful persistence apply.
    return true;
}

bool FermentationApplication::evaluateCurrentRecovery(
    const RunPersistenceSnapshot* snapshot, const RunCheckpointTime& bootTime) {
    if (snapshot == nullptr) {
        requireService(FaultCode::RunPersistenceUntrusted);
        return false;
    }
    pendingRecoverySource_ =
        std::unique_ptr<RunCommandState>{new (std::nothrow) RunCommandState{}};
    if (pendingRecoverySource_ == nullptr) {
        requireService(FaultCode::None, true);
        return false;
    }
    if (!restoreRunPersistenceSnapshotInto(*snapshot,
                                           *pendingRecoverySource_)) {
        pendingRecoverySource_.reset();
        requireService(FaultCode::RunPersistenceUntrusted);
        return false;
    }

    const auto evaluation =
        runPersistenceCoordinator_->evaluateCurrentFermentingRecovery(
            *pendingRecoverySource_, bootTime);
    recoveryDisposition_ = evaluation.disposition;
    if (evaluation.disposition == RecoveryDisposition::WaitingForTrustedTime) {
        return enterRecoveryEvaluationRamState(*pendingRecoverySource_);
    }
    if (evaluation.disposition == RecoveryDisposition::CurrentRunRecoverable) {
        runtimeRunState_ = std::move(pendingRecoverySource_);
        return true;
    }
    return enterRecoveryEvaluationRamState(*pendingRecoverySource_);
}

bool FermentationApplication::processTerminalClassification(
    BootClassification classification, const RunPersistenceSnapshot* snapshot,
    const RunCheckpointTime& bootTime) {
    if (snapshot == nullptr) {
        requireService(FaultCode::RunPersistenceUntrusted);
        return false;
    }
    auto target =
        std::unique_ptr<RunCommandState>{new (std::nothrow) RunCommandState{}};
    if (target == nullptr) {
        requireService(FaultCode::None, true);
        return false;
    }
    if (!restoreRunPersistenceSnapshotInto(*snapshot, *target)) {
        requireService(FaultCode::RunPersistenceUntrusted);
        return false;
    }

    const auto persisted =
        classification == BootClassification::DiscardableRun
            ? runPersistenceCoordinator_->discardAsNoActiveRun(*target,
                                                               bootTime)
            : runPersistenceCoordinator_->activateR1EligibleRun(
                  *target, bootTime, nullptr);
    if (persisted.status != RunPersistenceResultStatus::Applied) {
        requireService(FaultCode::RunPersistenceUntrusted);
        return false;
    }
    runtimeRunState_ = std::move(target);
    return true;
}

void FermentationApplication::update() {
    const auto guard = applicationCallSerializer_.enter();
    if (networkLifecycle_ != nullptr) {
        networkLifecycle_->poll();
    }
    reevaluateWaitingForTrustedTime();
    // Leaving the validated STANDBY ends the local service lease for good
    // (SafetyStateInvalidated); a later return to STANDBY needs the PIN again.
    if (!localServiceEntryAllowedUnlocked()) {
        localServiceLease_ = device_platform::ServiceSessionLease{};
    }
}

void FermentationApplication::publishOwningRecoveryEvidence(
    const CrossRolePlausibilityContext& evidence) {
    const auto guard = applicationCallSerializer_.enter();
    owningRecoveryEvidence_ = evidence;
}

RunPersistenceResult FermentationApplication::resumeFallback(
    const FermentationUiResumeFallbackCommand& command) {
    const auto guard = applicationCallSerializer_.enter();
    if (pendingFallbackResume_ == nullptr ||
        runPersistenceCoordinator_ == nullptr) {
        RunPersistenceResult unavailable;
        unavailable.status = RunPersistenceResultStatus::NotInitialized;
        return unavailable;
    }
    const auto& state = *pendingFallbackResume_;
    if (command.expected.expectedStateSequence !=
            state.processState.transitionSequence ||
        (command.expected.expectedRunRevision.has_value() &&
         *command.expected.expectedRunRevision != state.runRevision) ||
        (command.expected.expectedMessageRevision.has_value() &&
         *command.expected.expectedMessageRevision != state.messageRevision) ||
        (command.expected.expectedFaultRevision.has_value() &&
         *command.expected.expectedFaultRevision != state.faultRevision) ||
        (command.expected.expectedRecoveryEpisodeRevision.has_value() &&
         *command.expected.expectedRecoveryEpisodeRevision !=
             state.recoveryEpisodeRevision)) {
        RunPersistenceResult stale;
        stale.status = RunPersistenceResultStatus::StaleDecision;
        stale.coordinatorState =
            RunPersistenceCoordinatorState::FallbackRecoveryPending;
        return stale;
    }
    if (!command.confirmed) {
        RunPersistenceResult pending;
        pending.status = RunPersistenceResultStatus::RecoveryPending;
        pending.coordinatorState =
            RunPersistenceCoordinatorState::FallbackRecoveryPending;
        return pending;
    }
    if (!owningRecoveryEvidence_.has_value()) {
        RunPersistenceResult unavailable;
        unavailable.status = RunPersistenceResultStatus::RecoveryPending;
        unavailable.coordinatorState =
            RunPersistenceCoordinatorState::FallbackRecoveryPending;
        return unavailable;
    }
    // Evidence is a point-in-time owning observation.  Consume it before the
    // mutating coordinator attempt so neither a failed write nor a pending
    // trusted-time result can replay the same sensor/plausibility snapshot.
    const auto evidence = *owningRecoveryEvidence_;
    owningRecoveryEvidence_.reset();
    const auto outcome =
        runPersistenceCoordinator_->activateFallbackRecoveredRun(
            *pendingFallbackResume_, currentCheckpointTime(), evidence);
    if (outcome.persistenceResult.status ==
        RunPersistenceResultStatus::Applied) {
        // The coordinator's resultingState is the exact candidate that was
        // durably committed.  Adopt that value, rather than the pre-commit
        // retained fallback copy, so RAM/FSM cannot diverge from storage.
        runtimeRunState_ = std::unique_ptr<RunCommandState>{
            new (std::nothrow) RunCommandState(outcome.resultingState)};
        if (runtimeRunState_ == nullptr) {
            requireService(FaultCode::None, true);
            RunPersistenceResult failed;
            failed.status =
                RunPersistenceResultStatus::PersistenceCommittedApplyFailed;
            failed.step = RunPersistenceStep::RamApply;
            failed.technicalReason =
                RunPersistenceTechnicalReason::InvalidProjection;
            failed.durability = RunPersistenceDurability::MayHaveChanged;
            failed.coordinatorState =
                RunPersistenceCoordinatorState::PersistenceCommittedApplyFailed;
            return failed;
        }
        pendingFallbackResume_.reset();
        if (outcome.resultingState.activeProgramRun.has_value() ||
            outcome.resultingState.activeManualRun.has_value()) {
            loadDisposition_ = RunLoadDisposition::ResumeOffer;
            persistenceLoadStatus_ = RunPersistenceLoadStatus::Current;
        } else {
            loadDisposition_ = RunLoadDisposition::NoActiveRun;
            persistenceLoadStatus_ = RunPersistenceLoadStatus::NoActiveRun;
            recoveryDisposition_.reset();
        }
    }
    return outcome.persistenceResult;
}

std::optional<device_platform::StorageEpoch>
FermentationApplication::factoryResetPreviousEpochUnlocked() const {
    // The old epoch comes from the loaded runtime or, without one, only from
    // the verified bootstrap of a configuration the recovery core admitted as
    // `ResetEligibleNoRuntime` (Issue #19 S1). The core re-proves eligibility
    // on every call; nothing else is accepted.
    if (configurationRecoveryService_ == nullptr ||
        configurationService_ == nullptr || stateStore_ == nullptr) {
        return std::nullopt;
    }
    if (storageEpoch_.has_value()) {
        return storageEpoch_;
    }
    if (!factoryResetRecoveryEntryUnlocked()) {
        return std::nullopt;
    }
    return noRuntimeResetBootstrapEpochUnlocked();
}

std::optional<device_platform::StorageEpoch>
FermentationApplication::noRuntimeResetBootstrapEpochUnlocked() const {
    if (bootstrapStore_ == nullptr) {
        return std::nullopt;
    }
    const auto bootstrap = bootstrapStore_->scan();
    if (bootstrap.status != ConfigurationBootstrapScanStatus::Available ||
        !bootstrap.loaded.has_value() ||
        bootstrap.loaded->record.state !=
            ConfigurationBootstrapState::Initialized ||
        bootstrap.loaded->record.handoff == RunEpochHandoffState::Pending ||
        bootstrap.loaded->record.handoff == RunEpochHandoffState::Committed) {
        return std::nullopt;
    }
    return bootstrap.loaded->record.storageEpoch;
}

ConfigurationRecoveryResult
FermentationApplication::beginAuthorizedFactoryReset() {
    const auto guard = applicationCallSerializer_.enter();
    ConfigurationRecoveryResult unavailable{
        ConfigurationRecoveryStatus::ConfigurationUnavailable, {}};
    const auto knownEpoch = factoryResetPreviousEpochUnlocked();
    if (!knownEpoch.has_value()) {
        return unavailable;
    }
    const auto previousEpoch = *knownEpoch;
    // Wait for running authentication operations before the destructive,
    // epoch-changing reset; new operations fail closed meanwhile.
    authOperationGate_.closeAndDrain();
    const auto reset =
        configurationRecoveryService_->beginAuthorizedFactoryReset();
#if defined(APP_ISSUE_90_SLICE7_HARNESS)
    configurationRecoveryStatus_ = reset.status;
#endif
    if (reset.status != ConfigurationRecoveryStatus::FactoryResetCompleted) {
        // Domain and context are unchanged and remain usable.
        authOperationGate_.reopen();
        return reset;
    }
    if (webSessionManager_ != nullptr) {
        // Revoke at the irreversible reset boundary. Later run-epoch handoff
        // failures must not preserve pre-reset browser authority.
        revokeWebSessionsAtTrustBoundary();
    }
    // The reset has advanced the configuration/storage epoch. Invalidate the
    // old domain now; a later bootstrap failure must remain fail-closed.
    resetAuthenticationState();

    auto authorizedRunEpochHandoff =
        configurationRecoveryService_->takeAuthorizedRunEpochHandoffProof();

    const auto runtime = configurationService_->acquireRuntime();
    if (runtime.status != RuntimeConfigurationReadStatus::RuntimeLeaseGranted) {
        requireService(FaultCode::ConfigurationUnavailable);
        return {ConfigurationRecoveryStatus::RunPersistenceHandoffUnavailable,
                reset.diagnostics};
    }
    const auto currentEpoch = runtime.lease.get().storageEpoch();
    if (currentEpoch.value() == 0U || currentEpoch == previousEpoch) {
        requireService(FaultCode::RunPersistenceUntrusted);
        return {ConfigurationRecoveryStatus::RunPersistenceHandoffUnavailable,
                reset.diagnostics};
    }

    auto coordinator = std::unique_ptr<RunPersistenceCoordinator>{
        new (std::nothrow) RunPersistenceCoordinator(*stateStore_, currentEpoch,
                                                     RunCheckpointSchedule{})};
    if (coordinator == nullptr) {
        requireService(FaultCode::None, true);
        return {ConfigurationRecoveryStatus::RunPersistenceHandoffUnavailable,
                reset.diagnostics};
    }
    if (!authorizedRunEpochHandoff.has_value() ||
        authorizedRunEpochHandoff->previousEpoch() != previousEpoch ||
        authorizedRunEpochHandoff->currentEpoch() != currentEpoch) {
        requireService(FaultCode::RunPersistenceUntrusted);
        return {ConfigurationRecoveryStatus::RunPersistenceHandoffUnavailable,
                reset.diagnostics};
    }
    if (authorizedRunEpochHandoff->phase() !=
        AuthorizedRunEpochHandoffPhase::Pending) {
        requireService(FaultCode::RunPersistenceUntrusted);
        return {ConfigurationRecoveryStatus::RunPersistenceHandoffUnavailable,
                reset.diagnostics};
    }
    const auto prepared =
        coordinator->prepareAuthorizedEpochHandoff(*authorizedRunEpochHandoff);
    if (prepared.persistenceResult.status !=
            RunPersistenceResultStatus::Applied ||
        !prepared.evidence.has_value()) {
        requireService(FaultCode::RunPersistenceUntrusted);
        return {ConfigurationRecoveryStatus::RunPersistenceHandoffUnavailable,
                reset.diagnostics};
    }
    const auto committed =
        configurationRecoveryService_->commitAuthorizedRunEpochHandoff(
            *authorizedRunEpochHandoff, *prepared.evidence);
    if (committed.status != ConfigurationRecoveryStatus::RuntimeReady) {
        requireService(FaultCode::RunPersistenceUntrusted);
        return {ConfigurationRecoveryStatus::RunPersistenceHandoffUnavailable,
                reset.diagnostics};
    }
    const auto finalized =
        coordinator->finalizeAuthorizedEpochHandoff(*authorizedRunEpochHandoff);
    if (finalized.persistenceResult.status !=
            RunPersistenceResultStatus::Applied ||
        !finalized.evidence.has_value()) {
        requireService(FaultCode::RunPersistenceUntrusted);
        return {ConfigurationRecoveryStatus::RunPersistenceHandoffUnavailable,
                reset.diagnostics};
    }
    const auto consumed =
        configurationRecoveryService_->consumeAuthorizedRunEpochHandoff(
            *authorizedRunEpochHandoff, *finalized.evidence);
    if (consumed.status != ConfigurationRecoveryStatus::RuntimeReady) {
        requireService(FaultCode::RunPersistenceUntrusted);
        return {ConfigurationRecoveryStatus::RunPersistenceHandoffUnavailable,
                reset.diagnostics};
    }
    // The handoff is consumed now, so resolve the new epoch-bound
    // authentication domain once more. The first resolution above deliberately
    // remained fail-closed while the reset handoff was still pending.
    initializeAuthentication(*stateStore_);
    runPersistenceCoordinator_ = std::move(coordinator);
    storageEpoch_ = currentEpoch;
    runIdentity_.reset();
    pendingResume_.reset();
    pendingFallbackResume_.reset();
    pendingRecoverySource_.reset();
    owningRecoveryEvidence_.reset();
    recoveryDisposition_.reset();
    runtimeRunState_.reset();

    auto loadResult = std::unique_ptr<RunPersistenceLoadResult>{
        new (std::nothrow) RunPersistenceLoadResult{}};
    if (loadResult == nullptr) {
        requireService(FaultCode::None, true);
        return {ConfigurationRecoveryStatus::RunPersistenceHandoffUnavailable,
                reset.diagnostics};
    }
    runPersistenceCoordinator_->loadAndInitializeInto(*loadResult);
    persistenceLoadStatus_ = loadResult->status;
    if (const auto highWater = runPersistenceCoordinator_->commandIdHighWater();
        highWater.has_value()) {
        auto identity = ApplicationRunIdentity::create(currentEpoch, highWater);
        if (!identity.has_value()) {
            requireService(FaultCode::RunPersistenceUntrusted);
            return {
                ConfigurationRecoveryStatus::RunPersistenceHandoffUnavailable,
                reset.diagnostics};
        }
        runIdentity_ = std::unique_ptr<ApplicationRunIdentity>{
            new (std::nothrow) ApplicationRunIdentity(std::move(*identity))};
        if (runIdentity_ == nullptr) {
            requireService(FaultCode::None, true);
            return {
                ConfigurationRecoveryStatus::RunPersistenceHandoffUnavailable,
                reset.diagnostics};
        }
    }
    const RunPersistenceSnapshot* snapshot =
        loadResult->snapshot.has_value() ? loadResult->snapshot.operator->()
                                         : nullptr;
    loadDisposition_ =
        boot_classification::classifyRunLoad(loadResult->status, snapshot);
    const auto classification =
        boot_classification::classify(loadResult->status, snapshot);
    if (!processBootClassification(classification, snapshot,
                                   currentCheckpointTime())) {
        requireService(FaultCode::RunPersistenceUntrusted);
        return {ConfigurationRecoveryStatus::RunPersistenceHandoffUnavailable,
                reset.diagnostics};
    }
    lifecycleState_ = ApplicationLifecycleState::Ready;
    return reset;
}

RunCheckpointTime FermentationApplication::currentCheckpointTime()
    const noexcept {
    if (timeSource_ == nullptr) {
        return RunCheckpointTime{};
    }
    return RunCheckpointTime{timeSource_->monotonicMillis(),
                             timeSource_->unixTimeSeconds()};
}

bool FermentationApplication::enterRecoveryEvaluationRamState(
    const RunCommandState& source) {
    auto target =
        std::unique_ptr<RunCommandState>{new (std::nothrow) RunCommandState{}};
    if (target == nullptr) {
        requireService(FaultCode::None, true);
        return false;
    }
    *target = source;
    const auto now = currentCheckpointTime().monotonicMillis;
    if (runPersistenceCoordinator_ == nullptr ||
        !runPersistenceCoordinator_->prepareRecoveryEvaluationState(*target,
                                                                    now)) {
        requireService(FaultCode::RunPersistenceUntrusted);
        return false;
    }
    runtimeRunState_ = std::move(target);
    loadDisposition_ = RunLoadDisposition::RecoveryEvaluation;
    return true;
}

void FermentationApplication::reevaluateWaitingForTrustedTime() {
    if (!recoveryDisposition_.has_value() ||
        *recoveryDisposition_ != RecoveryDisposition::WaitingForTrustedTime ||
        pendingRecoverySource_ == nullptr ||
        runPersistenceCoordinator_ == nullptr || timeSource_ == nullptr) {
        return;
    }

    const auto evaluation =
        runPersistenceCoordinator_->evaluateCurrentFermentingRecovery(
            *pendingRecoverySource_, currentCheckpointTime());
    recoveryDisposition_ = evaluation.disposition;
    if (evaluation.disposition == RecoveryDisposition::WaitingForTrustedTime) {
        return;
    }
    if (evaluation.disposition == RecoveryDisposition::CurrentRunRecoverable) {
        runtimeRunState_ = std::move(pendingRecoverySource_);
        return;
    }
    static_cast<void>(enterRecoveryEvaluationRamState(*pendingRecoverySource_));
}

bool FermentationApplication::ready() const {
    return lifecycleState_ == ApplicationLifecycleState::Ready;
}

std::optional<ProcessRuntimeState>
FermentationApplication::publishedProcessState() const {
    if (runtimeRunState_ == nullptr) {
        return std::nullopt;
    }
    return runtimeRunState_->processState;
}

void FermentationApplication::requireService(
    FaultCode faultCode, bool applicationAllocationFailure) noexcept {
    lifecycleState_ = ApplicationLifecycleState::ServiceRequired;
    localServiceLease_ = device_platform::ServiceSessionLease{};
    presentationState_.faultCode = faultCode;
    presentationState_.applicationAllocationFailure =
        applicationAllocationFailure;
}

bool FermentationApplication::publishStandby() {
    auto target =
        std::unique_ptr<RunCommandState>{new (std::nothrow) RunCommandState{}};
    if (target == nullptr) {
        requireService(FaultCode::None, true);
        return false;
    }

    if (!establishBootCompletedStandby(target->processState, 0U)) {
        requireService(FaultCode::None);
        return false;
    }
    runtimeRunState_ = std::move(target);
    return true;
}

// --- Local factory reset flow (Issue #19, plan section 4) -----------------

bool FermentationApplication::factoryResetRunGateOpenUnlocked() const noexcept {
    // Blocks only while a process is actually running (published active run).
    // In SAFE_BOOT no run is published and the actuator interlock denies every
    // output, so the recovery reset stays reachable there.
    return runtimeRunState_ == nullptr ||
           (!runtimeRunState_->activeProgramRun.has_value() &&
            !runtimeRunState_->activeManualRun.has_value());
}

bool FermentationApplication::factoryResetRecoveryEntryUnlocked()
    const noexcept {
    // Only the state the recovery core itself latched as reset-eligible: a
    // plain `NoRuntime`, a global scan blocker, an identity collision or an
    // unknown bootstrap never qualifies.
    return noRuntimeResetAdmitted_ && !storageEpoch_.has_value() &&
           bootstrapStore_ != nullptr && configurationService_ != nullptr &&
           configurationService_->mode() ==
               ConfigurationServiceMode::ResetEligibleNoRuntime;
}

bool FermentationApplication::factoryResetAvailableUnlocked() const noexcept {
    // The core needs a loaded configuration runtime (storageEpoch_); a reset
    // from a configuration without runtime is not offered (R0 stop finding S1).
    return factoryResetFlow_.configured() &&
           configurationRecoveryService_ != nullptr &&
           configurationService_ != nullptr && stateStore_ != nullptr &&
           (storageEpoch_.has_value() || factoryResetRecoveryEntryUnlocked()) &&
           factoryResetRunGateOpenUnlocked();
}

void FermentationApplication::setFactoryResetHoldMillis(
    std::optional<std::uint32_t> holdMillis) {
    const auto guard = applicationCallSerializer_.enter();
    if (factoryResetFlow_.active()) {
        return;
    }
    factoryResetFlow_ = FactoryResetFlow(holdMillis);
}

FermentationFactoryResetView FermentationApplication::factoryResetView(
    std::uint64_t nowMs) const {
    const auto guard = applicationCallSerializer_.enter();
    FermentationFactoryResetView view;
    view.stage = factoryResetFlow_.stage();
    view.kind = factoryResetFlow_.kind();
    view.outcome = factoryResetFlow_.outcome();
    view.available =
        !factoryResetFlow_.active() && factoryResetAvailableUnlocked();
    view.recoveryEntry = factoryResetRecoveryEntryUnlocked();
    view.holdRequiredMillis = factoryResetFlow_.holdMillis().value_or(0U);
    if (view.holdRequiredMillis != 0U) {
        const auto held =
            static_cast<std::uint64_t>(factoryResetFlow_.heldMillis(nowMs));
        const auto tenths = held * 10U / view.holdRequiredMillis;
        view.holdProgressTenths =
            static_cast<std::uint8_t>(tenths > 10U ? 10U : tenths);
    }
    return view;
}

bool FermentationApplication::beginFactoryReset(FactoryResetKind kind) {
    const auto guard = applicationCallSerializer_.enter();
    // Variant A (PIN protected) is not offered until a local PIN verification
    // exists (owner decision O-R2).
    if (kind != FactoryResetKind::PinIndependent ||
        !factoryResetAvailableUnlocked()) {
        return false;
    }
    return factoryResetFlow_.begin(kind);
}

bool FermentationApplication::acknowledgeFactoryReset() {
    const auto guard = applicationCallSerializer_.enter();
    return factoryResetFlow_.acknowledge();
}

void FermentationApplication::cancelFactoryReset() {
    const auto guard = applicationCallSerializer_.enter();
    factoryResetFlow_.cancel();
}

void FermentationApplication::dismissFactoryReset() {
    const auto guard = applicationCallSerializer_.enter();
    factoryResetFlow_.dismiss();
}

bool FermentationApplication::endNetworkAfterFactoryReset() {
    // Network first (disconnects the clients), then the HTTP server. Both are
    // always attempted. `httpd_stop()` blocks until the server task ended and
    // handlers may wait for the Application gate, so neither call may run
    // under it. `running()` is not evidence of completion; the return values
    // are.
    bool confirmed = true;
    if (networkLifecycle_ != nullptr) {
        confirmed = networkLifecycle_->stop().status ==
                        device_platform::NetworkOperationStatus::Applied &&
                    confirmed;
    }
    if (httpServerLifecycle_ != nullptr) {
        confirmed = httpServerLifecycle_->stop() && confirmed;
    }
    {
        const auto guard = applicationCallSerializer_.enter();
        if (networkConfigurationService_ != nullptr) {
            networkConfigurationService_->discardAfterFactoryReset();
        }
    }
    return confirmed;
}

void FermentationApplication::updateFactoryResetHold(bool held,
                                                     std::uint64_t nowMs) {
    FactoryResetCoreResult core = FactoryResetCoreResult::Unavailable;
    bool restartRequired = false;
    {
        const auto guard = applicationCallSerializer_.enter();
        if (!factoryResetFlow_.updateHold(held, nowMs)) {
            return;
        }
        // The flow is Executing. The binding precondition is re-checked here,
        // under the same gate as the core call and as a run start.
        if (!factoryResetAvailableUnlocked()) {
            factoryResetFlow_.finish(FactoryResetOutcome::Rejected);
            return;
        }
        // Without a loaded runtime the boot-time subsystems were never
        // composed.
        const bool withoutRuntime = !storageEpoch_.has_value();
        switch (beginAuthorizedFactoryReset().status) {
            case ConfigurationRecoveryStatus::FactoryResetCompleted:
                core = FactoryResetCoreResult::Completed;
                restartRequired = withoutRuntime;
                break;
            case ConfigurationRecoveryStatus::RunPersistenceHandoffUnavailable:
                core = FactoryResetCoreResult::HandoffUnavailable;
                break;
            case ConfigurationRecoveryStatus::ConfigurationUnavailable:
                core = FactoryResetCoreResult::Unavailable;
                break;
            default:
                core = FactoryResetCoreResult::Failed;
                break;
        }
        if (withoutRuntime) {
            // Any attempt consumes the one-time admission; a retry needs the
            // restart (which re-evaluates it).
            noRuntimeResetAdmitted_ = false;
        }
        if (withoutRuntime && factoryResetBoundaryCrossed(core)) {
            // Operation stays blocked until the restart composes the normal
            // boot (including the network) on the new epoch; held under the
            // same gate so no run can start in between.
            requireService(FaultCode::ConfigurationUnavailable);
        }
    }
    // Outside the gate: ending network and HTTP may block.
    const bool networkEnded = factoryResetBoundaryCrossed(core)
                                  ? endNetworkAfterFactoryReset()
                                  : true;
    const auto guard = applicationCallSerializer_.enter();
    factoryResetFlow_.finish(
        factoryResetOutcomeFor(core, networkEnded, restartRequired));
}

}  // namespace fermentation
