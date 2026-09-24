#include "fermentation_application.hpp"

#include <cctype>
#include <new>
#include <type_traits>
#include <utility>

#include "configuration_bootstrap_store.hpp"
#include "configuration_graph_store.hpp"
#include "configuration_mutation_coordinator.hpp"
#include "configuration_recovery_service.hpp"
#include "configuration_service.hpp"
#include "authentication_records.hpp"
#include "connectivity_credentials.hpp"
#include "fermentation_ui_commands.hpp"
#include "fermentation_ui_projector.hpp"
#include "network_configuration_service.hpp"
#include "network_setup_routes.hpp"
#include "web_application_routes.hpp"
#include "web_session.hpp"
#include "run_persistence_coordinator.hpp"

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

bool hasNextRunOverride(
    const FermentationUiStartCandidate& candidate) noexcept {
    return candidate.targetTemperatureCelsius.has_value() ||
           candidate.fermentationDurationMinutes.has_value() ||
           candidate.preheatEnabled.has_value() ||
           candidate.completionMode.has_value() ||
           candidate.coolingTargetCelsius.has_value() ||
           candidate.holdDurationMinutes.has_value();
}

void applyNextRunOverrides(ProgramDocument& program,
                           const FermentationUiStartCandidate& candidate) {
    auto& definition = program.program;
    if (candidate.targetTemperatureCelsius.has_value() &&
        !definition.fermentationStages.empty()) {
        definition.fermentationStages.front().targetTemperatureCelsius =
            candidate.targetTemperatureCelsius;
    }
    if (candidate.fermentationDurationMinutes.has_value() &&
        !definition.fermentationStages.empty()) {
        definition.fermentationStages.front().durationMinutes =
            candidate.fermentationDurationMinutes;
    }
    if (candidate.preheatEnabled.has_value()) {
        definition.preheat = *candidate.preheatEnabled;
    }
    if (candidate.completionMode.has_value()) {
        definition.completion.mode = *candidate.completionMode;
        if (*candidate.completionMode == CompletionMode::FinishWithoutCooling) {
            definition.completion.coolingTargetCelsius.reset();
            definition.completion.holdDurationMinutes.reset();
        } else if (*candidate.completionMode ==
                       CompletionMode::CoolThenFinish ||
                   *candidate.completionMode ==
                       CompletionMode::CoolAndHoldUntilManualStop) {
            definition.completion.holdDurationMinutes.reset();
        }
    }
    if (candidate.coolingTargetCelsius.has_value()) {
        definition.completion.coolingTargetCelsius =
            candidate.coolingTargetCelsius;
    }
    if (candidate.holdDurationMinutes.has_value()) {
        definition.completion.holdDurationMinutes =
            candidate.holdDurationMinutes;
    }
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

FermentationApplication::FermentationApplication() = default;
FermentationApplication::~FermentationApplication() = default;

FermentationApplicationRequestResult
FermentationApplication::prepareStartProgram(
    const FermentationUiCommandContext& context,
    const FermentationUiStartProgramIntent& intent) {
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
    const bool nextRunOverride = hasNextRunOverride(candidate);
    const auto sensorMode = candidate.sensorMode.value_or(RunSensorMode::Air);
    applyNextRunOverrides(*program, candidate);
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
    request.sensorMode = sensorMode;
    request.safetyAllowsStart = evidence.safetyAllowsStart;
    request.airSensorValid = evidence.airSensorValid;
    request.coolingSensorValid = evidence.coolingSensorValid;
    request.productSensorValid = evidence.productSensorValid;
    return makePreparedRequest(std::move(request), evidence.plausibility);
}

FermentationApplicationRequestResult
FermentationApplication::prepareStartManualHolding(
    const FermentationUiCommandContext& context,
    const FermentationUiStartManualHoldingIntent& intent) {
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
FermentationApplication::prepareStartManualTimed(
    const FermentationUiCommandContext& context,
    const ManualTimedRunValues& values) {
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

FermentationApplicationRequestResult FermentationApplication::prepareStop(
    const FermentationUiCommandContext& context,
    const FermentationUiStopRunIntent& intent) {
    const auto evidence = resolveRuntimeEvidence();
    if (runIdentity_ == nullptr) {
        return requestFailure(
            FermentationApplicationRequestStatus::NotInitialized);
    }
    if (intent.option == StopOption::AbortAndCool &&
        !intent.coolingPlan.has_value()) {
        return requestFailure(
            FermentationApplicationRequestStatus::InvalidInput);
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
    if (intent.option == StopOption::AbortAndCool) {
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

FermentationApplicationRequestResult FermentationApplication::prepareCompletion(
    const FermentationUiCommandContext& context,
    const FermentationUiCompleteRunIntent& intent) {
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

RunPersistenceResult FermentationApplication::applyPreparedRequest(
    const FermentationApplicationPreparedRequest& request) {
    RunPersistenceResult unavailable;
    unavailable.status = RunPersistenceResultStatus::NotInitialized;
    unavailable.coordinatorState =
        RunPersistenceCoordinatorState::Uninitialized;
    // This is the last application-owned mutation boundary.  Callers still
    // use prepare -> confirm -> apply, but the owner must not rely on a
    // transport preserving that order: several domain deciders intentionally
    // accept an unconfirmed request for decision-only evaluation.
    if (!request.commandEnvelope().confirmed) {
        unavailable.status = RunPersistenceResultStatus::InvalidDecision;
        return unavailable;
    }
    if (runtimeRunState_ == nullptr || runPersistenceCoordinator_ == nullptr ||
        timeSource_ == nullptr) {
        return unavailable;
    }
    const auto decision = FermentationUiCommandBridge::decidePreparedCommand(
        *runtimeRunState_, request);
    if (!decision.has_value()) {
        unavailable.status = RunPersistenceResultStatus::InvalidDecision;
        return unavailable;
    }
    if (!decision->proposed()) {
        unavailable.status = decision->status == CommandStatus::StaleState
                                 ? RunPersistenceResultStatus::StaleDecision
                                 : RunPersistenceResultStatus::InvalidDecision;
        return unavailable;
    }
    const auto& owningPlausibility = request.owningPlausibility();
    const auto* plausibility =
        owningPlausibility.has_value() ? &owningPlausibility.value() : nullptr;
    const auto time = currentCheckpointTime();
    if (decision->kind == CommandKind::StartProgram ||
        decision->kind == CommandKind::StartManualHolding) {
        if (configurationService_ == nullptr) {
            unavailable.status = RunPersistenceResultStatus::NotInitialized;
            return unavailable;
        }
        const auto runtime = configurationService_->acquireRuntime();
        if (runtime.status !=
            RuntimeConfigurationReadStatus::RuntimeLeaseGranted) {
            unavailable.status = RunPersistenceResultStatus::Blocked;
            return unavailable;
        }
        const auto provenance =
            FreshStartSnapshotProvenance::fromRuntimeLease(runtime.lease);
        return runPersistenceCoordinator_->persistFreshStartCommand(
            *runtimeRunState_, *decision, provenance, time, plausibility);
    }
    return runPersistenceCoordinator_->persistCommand(
        *runtimeRunState_, *decision, time, plausibility);
}

std::optional<std::vector<FermentationUiProgramListEntry>>
FermentationApplication::uiProgramList() const {
    const std::lock_guard<std::recursive_mutex> lock(stateMutex_);
    if (configurationService_ == nullptr) {
        return std::nullopt;
    }
    auto runtime = configurationService_->acquireRuntime();
    if (runtime.status != RuntimeConfigurationReadStatus::RuntimeLeaseGranted) {
        return std::nullopt;
    }
    return makeFermentationUiProgramList(runtime.lease->programCatalog());
}

ConfigurationPreviewInstallResult FermentationApplication::prepareProgramEdit(
    ProgramCatalogRevision expectedRevision,
    FermentationUiProgramEditRequest request, ChangeOrigin origin) {
    const std::lock_guard<std::recursive_mutex> lock(stateMutex_);
    if (configurationService_ == nullptr || runtimeRunState_ == nullptr) {
        return {ConfigurationPreviewStatus::ConfigurationRuntimeUnavailable,
                std::nullopt};
    }
    auto runtime = configurationService_->acquireRuntime();
    if (runtime.status != RuntimeConfigurationReadStatus::RuntimeLeaseGranted) {
        return {ConfigurationPreviewStatus::ConfigurationRuntimeUnavailable,
                std::nullopt};
    }
    // The web DTO can rename only an existing canonical document. It cannot
    // author a full ProgramDocument or alter its protected attributes.
    if (request.operation == FermentationUiProgramEditOperation::Edit &&
        !request.candidate.has_value() && request.name.has_value()) {
        const auto session =
            openProgramEditSession(runtime.lease.get(), request.programId);
        if (!session.has_value()) {
            return {ConfigurationPreviewStatus::InvalidCandidate, std::nullopt};
        }
        request.candidate = session->candidate;
        request.candidate->program.name = *request.name;
    }
    return applyProgramEditPreview(
        *configurationService_, expectedRevision, request,
        makeFermentationUiProgramUsageEvidence(*runtimeRunState_), origin);
}

ConfigurationCommitResult FermentationApplication::confirmConfigurationPreview(
    const FermentationUiConfigurationCommitCommand& command) {
    const std::lock_guard<std::recursive_mutex> lock(stateMutex_);
    if (configurationService_ == nullptr) {
        return {ConfigurationCommitStatus::ConfigurationRuntimeFailure};
    }
    const auto result = FermentationUiCommandBridge::commitConfiguration(
        *configurationService_, command);
    if (const auto* status =
            std::get_if<ConfigurationCommitStatus>(&result.detail)) {
        return {*status};
    }
    return {ConfigurationCommitStatus::ConfigurationRuntimeFailure};
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
    const device_platform::IResetCauseSource* resetCauseSource,
    device_platform::ISecureRandomSource* randomSource,
    IAuthenticationKdf* authenticationKdf) {
    return beginPersistent(platformServices, store, timeZoneResolver,
                           &timeSource, resetCauseSource, &networkLifecycle,
                           &httpServerLifecycle, randomSource,
                           authenticationKdf);
}

// fail-closed composition sequence is intentionally kept as one ordered
// startup transaction; splitting it would change its recovery boundary.
// NOLINTNEXTLINE(readability-function-cognitive-complexity)
bool FermentationApplication::initializeNetwork(
    device_platform::IStateStore& store,
    device_platform::StorageEpoch storageEpoch,
    device_platform::NetworkMode selectedMode,
    const std::string& canonicalDeviceName,
    bool positiveAuthenticationEpochEvidence) {
    if (networkLifecycle_ == nullptr || httpServerLifecycle_ == nullptr) {
        return true;
    }

    connectivityCredentialStore_ = std::unique_ptr<ConnectivityCredentialStore>{
        new (std::nothrow) ConnectivityCredentialStore(store)};
    if (connectivityCredentialStore_ == nullptr) {
        requireService(FaultCode::None, true);
        return false;
    }
    networkConfigurationService_ = std::unique_ptr<NetworkConfigurationService>{
        new (std::nothrow) NetworkConfigurationService(
            *connectivityCredentialStore_, *networkLifecycle_)};
    if (networkConfigurationService_ == nullptr) {
        requireService(FaultCode::None, true);
        return false;
    }
    networkSetupRoutes_ = std::unique_ptr<NetworkSetupRoutes>{
        new (std::nothrow) NetworkSetupRoutes(*networkConfigurationService_)};
    if (networkSetupRoutes_ == nullptr) {
        requireService(FaultCode::None, true);
        return false;
    }
    if (randomSource_ != nullptr && authenticationKdf_ != nullptr &&
        timeSource_ != nullptr) {
        authenticationStore_ = std::unique_ptr<AuthenticationRecordStore>{
            new (std::nothrow) AuthenticationRecordStore(store)};
        authenticationDomain_ = std::unique_ptr<AuthenticationDomain>{
            authenticationStore_ == nullptr
                ? nullptr
                : new (std::nothrow) AuthenticationDomain(*authenticationStore_,
                                                          *authenticationKdf_,
                                                          *randomSource_)};
        webSessionManager_ = std::unique_ptr<WebSessionManager>{
            new (std::nothrow) WebSessionManager(
                *randomSource_, fermentationWebServicePolicy())};
        if (authenticationStore_ == nullptr ||
            authenticationDomain_ == nullptr || webSessionManager_ == nullptr) {
            requireService(FaultCode::None, true);
            return false;
        }
        if (mutationCoordinator_ == nullptr) {
            requireService(FaultCode::ConfigurationUnavailable);
            return false;
        }
        auto authMutation = mutationCoordinator_->tryAcquire();
        if (authMutation.status !=
            ConfigurationMutationAcquireStatus::Acquired) {
            requireService(FaultCode::ConfigurationUnavailable);
            return false;
        }
        auto bootstrap = bootstrapStore_->scan();
        const auto root = authenticationStore_->readRoot(storageEpoch);
        if (bootstrap.status != ConfigurationBootstrapScanStatus::Available ||
            !bootstrap.loaded.has_value()) {
            requireService(FaultCode::ConfigurationUnavailable);
            return false;
        }
        if (bootstrap.loaded->record.authDomainHandoff ==
            AuthDomainHandoffState::None) {
            const bool rootCanProveFreshEpoch =
                root.status == AuthenticationReadStatus::NotFound ||
                root.status == AuthenticationReadStatus::DifferentEpoch ||
                (root.status == AuthenticationReadStatus::Success &&
                 root.value.has_value() &&
                 root.value->state == AuthProvisioningState::Unprovisioned);
            if (!positiveAuthenticationEpochEvidence ||
                !rootCanProveFreshEpoch) {
                requireService(FaultCode::ConfigurationUnavailable);
                return false;
            }
            const auto migrated = bootstrapStore_->writeAuthDomainHandoff(
                *bootstrap.loaded, AuthDomainHandoffState::Unconsumed,
                authMutation.lease);
            if (migrated.status != ConfigurationBootstrapWriteStatus::Success ||
                !migrated.loaded.has_value()) {
                requireService(FaultCode::ConfigurationUnavailable);
                return false;
            }
            bootstrap.loaded = migrated.loaded;
        }
        const auto authHandoff = bootstrap.loaded->record.authDomainHandoff;
        if (authHandoff == AuthDomainHandoffState::Consumed) {
            if (root.status != AuthenticationReadStatus::Success ||
                !root.value.has_value() ||
                root.value->state != AuthProvisioningState::Provisioned) {
                requireService(FaultCode::ConfigurationUnavailable);
                return false;
            }
        } else if (authHandoff == AuthDomainHandoffState::Unconsumed) {
            const bool rootCanBeInitialized =
                root.status == AuthenticationReadStatus::NotFound ||
                root.status == AuthenticationReadStatus::DifferentEpoch ||
                (root.status == AuthenticationReadStatus::Success &&
                 root.value.has_value() &&
                 root.value->state == AuthProvisioningState::Unprovisioned);
            if (!rootCanBeInitialized) {
                requireService(FaultCode::ConfigurationUnavailable);
                return false;
            }
            const auto inProgress = bootstrapStore_->writeAuthDomainHandoff(
                *bootstrap.loaded, AuthDomainHandoffState::InProgress,
                authMutation.lease);
            if (inProgress.status !=
                    ConfigurationBootstrapWriteStatus::Success ||
                !inProgress.loaded.has_value() ||
                authenticationDomain_->initializeUnprovisioned(storageEpoch) !=
                    AuthBootstrapStatus::BootstrapAllowed) {
                if (inProgress.loaded.has_value()) {
                    static_cast<void>(bootstrapStore_->writeAuthDomainHandoff(
                        *inProgress.loaded,
                        AuthDomainHandoffState::Indeterminate,
                        authMutation.lease));
                }
                requireService(FaultCode::ConfigurationUnavailable);
                return false;
            }
            const auto consumed = bootstrapStore_->writeAuthDomainHandoff(
                *inProgress.loaded, AuthDomainHandoffState::Consumed,
                authMutation.lease);
            if (consumed.status != ConfigurationBootstrapWriteStatus::Success) {
                requireService(FaultCode::ConfigurationUnavailable);
                return false;
            }
        } else {
            requireService(FaultCode::ConfigurationUnavailable);
            return false;
        }
        webApplicationRoutes_ = std::unique_ptr<WebApplicationRoutes>{
            new (std::nothrow)
                WebApplicationRoutes(*this, *authenticationDomain_,
                                     *webSessionManager_, *timeSource_)};
        webRouteDispatcher_ = std::unique_ptr<WebRouteDispatcher>{
            webApplicationRoutes_ == nullptr
                ? nullptr
                : new (std::nothrow) WebRouteDispatcher(
                      *networkSetupRoutes_, *webApplicationRoutes_)};
        if (webApplicationRoutes_ == nullptr ||
            webRouteDispatcher_ == nullptr) {
            requireService(FaultCode::None, true);
            return false;
        }
    }
    const auto hostname = networkHostnameFromDeviceName(canonicalDeviceName);
    if (!hostname.has_value() ||
        networkLifecycle_->setHostname(*hostname).status !=
            device_platform::NetworkOperationStatus::Applied) {
        requireService(FaultCode::ConfigurationUnavailable);
        return false;
    }
    const auto networkStart =
        networkConfigurationService_->start(selectedMode, storageEpoch);
    if (networkStart.status == NetworkConfigurationStatus::Applied &&
        !httpServerLifecycle_->start(
            webRouteDispatcher_ != nullptr
                ? static_cast<device_platform::IHttpRouteSink&>(
                      *webRouteDispatcher_)
                : static_cast<device_platform::IHttpRouteSink&>(
                      *networkSetupRoutes_))) {
        requireService(FaultCode::ConfigurationUnavailable);
        return false;
    }
    if (networkStart.status != NetworkConfigurationStatus::Applied &&
        networkStart.status != NetworkConfigurationStatus::SelectionRequired) {
        requireService(FaultCode::ConfigurationUnavailable);
        return false;
    }
    return true;
}

NetworkConfigurationResult FermentationApplication::applyNetworkMode(
    device_platform::NetworkMode selectedMode,
    std::optional<UserConfigurationRevision> expectedRevision,
    ChangeOriginKind origin) {
    const std::lock_guard<std::recursive_mutex> lock(stateMutex_);
    if (configurationService_ == nullptr ||
        networkConfigurationService_ == nullptr || stateStore_ == nullptr ||
        !storageEpoch_.has_value() || storageEpoch_->value() == 0U ||
        !device_platform::isSelectableNetworkMode(selectedMode)) {
        return {NetworkConfigurationStatus::InvalidMode};
    }
    if (expectedRevision.has_value()) {
        const auto current = configurationService_->userConfigurationRevision();
        if (!current.has_value() || *current != *expectedRevision) {
            return {NetworkConfigurationStatus::StateChanged};
        }
    }
    auto build = configurationService_->beginPreview();
    if (build.status != ConfigurationPreviewStatus::Success ||
        !build.lease.valid()) {
        return {NetworkConfigurationStatus::PersistenceFailure};
    }
    build.lease.userConfiguration().networkMode = selectedMode;
    const auto installed = configurationService_->installPreview(
        std::move(build.lease), {origin, 2U},
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
    const auto applied = networkConfigurationService_->start(
        selectedMode, runtime.lease.get().storageEpoch());
    if (applied.status != NetworkConfigurationStatus::Applied) {
        static_cast<void>(networkLifecycle_->stop());
        if (httpServerLifecycle_ != nullptr) {
            static_cast<void>(httpServerLifecycle_->stop());
        }
        requireService(FaultCode::ConfigurationUnavailable);
        return {NetworkConfigurationStatus::RecoveryRequired};
    }
    if (httpServerLifecycle_ != nullptr && !httpServerLifecycle_->running() &&
        networkSetupRoutes_ != nullptr &&
        !httpServerLifecycle_->start(*networkSetupRoutes_)) {
        requireService(FaultCode::ConfigurationUnavailable);
        return {NetworkConfigurationStatus::RecoveryRequired};
    }
    storageEpoch_ = runtime.lease.get().storageEpoch();
    return {NetworkConfigurationStatus::Applied};
}

AuthBootstrapStatus FermentationApplication::bootstrapAuthentication(
    const FermentationUiCommandContext& context,
    const FermentationUiBootstrapAuthenticationCommand& command) {
    const std::lock_guard<std::recursive_mutex> lock(stateMutex_);
    if (context.surface != device_platform::UiSurface::LocalDisplay ||
        !command.confirmed || authenticationDomain_ == nullptr ||
        !storageEpoch_.has_value()) {
        return AuthBootstrapStatus::InvalidInput;
    }
    return authenticationDomain_->bootstrap(*storageEpoch_, command.password,
                                            command.servicePin);
}

NetworkConfigurationResult
FermentationApplication::beginHomeWifiReconfiguration() {
    const std::lock_guard<std::recursive_mutex> lock(stateMutex_);
    if (networkConfigurationService_ == nullptr) {
        return {NetworkConfigurationStatus::NotInitialized};
    }
    return networkConfigurationService_->beginHomeWifiReconfiguration();
}

std::optional<device_platform::NetworkAccessPointInfo>
FermentationApplication::networkAccessPointInfo() const {
    const std::lock_guard<std::recursive_mutex> lock(stateMutex_);
    if (networkConfigurationService_ == nullptr) {
        return std::nullopt;
    }
    return networkConfigurationService_->accessPointInfo();
}

device_platform::NetworkMode FermentationApplication::networkMode()
    const noexcept {
    const std::lock_guard<std::recursive_mutex> lock(stateMutex_);
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
    owningRuntimeEvidence_ = evidence;
}

FermentationUiSnapshot FermentationApplication::uiSnapshot() const {
    const std::lock_guard<std::recursive_mutex> lock(stateMutex_);
    FermentationUiProjectionInput input;
    input.runState = runtimeRunState_.get();
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
    input.temperatures = {
        {FermentationTemperatureRole::CabinetAir,
         valueOf(evidence.plausibility.air), evidence.plausibility.air},
        {FermentationTemperatureRole::Product,
         valueOf(evidence.plausibility.product), evidence.plausibility.product},
        {FermentationTemperatureRole::Cooling,
         valueOf(evidence.plausibility.cooling),
         evidence.plausibility.cooling}};
    input.recoveryDisposition = recoveryDisposition_;
    input.persistenceLoadStatus = persistenceLoadStatus_;
    if (runPersistenceCoordinator_ != nullptr) {
        input.coordinatorState = runPersistenceCoordinator_->state();
    }
    input.application.lifecycleState = lifecycleState_;
    input.application.presentation = presentationState_;
    input.network.currentMode = networkMode();
    input.refreshTracker = &uiRefreshTracker_;
    return FermentationUiProjector::project(input);
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
    IAuthenticationKdf* authenticationKdf) {
    if (!platformServices.ready()) {
        return false;
    }

    platformServices_ = &platformServices;
    timeSource_ = timeSource;
    stateStore_ = &store;
    networkLifecycle_ = networkLifecycle;
    httpServerLifecycle_ = httpServerLifecycle;
    randomSource_ = randomSource;
    authenticationKdf_ = authenticationKdf;
    storageEpoch_.reset();
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
    pendingRecoverySource_.reset();
    recoveryDisposition_.reset();
    runtimeRunState_.reset();
    runPersistenceCoordinator_.reset();
    configurationRecoveryService_.reset();
    networkSetupRoutes_.reset();
    webRouteDispatcher_.reset();
    webApplicationRoutes_.reset();
    webSessionManager_.reset();
    authenticationDomain_.reset();
    authenticationStore_.reset();
    networkConfigurationService_.reset();
    connectivityCredentialStore_.reset();
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
        requireService(configurationFault(configurationResult.status));
        return true;
    }
    const auto epoch = runtime.lease.get().storageEpoch();
    storageEpoch_ = epoch;

    const bool positiveAuthenticationEpochEvidence =
        configurationResult.status ==
            ConfigurationRecoveryStatus::FactoryInitializationCompleted ||
        configurationResult.status ==
            ConfigurationRecoveryStatus::FactoryResetCompleted;
    if (!initializeNetwork(store, epoch,
                           runtime.lease.get().userConfiguration().networkMode,
                           runtime.lease.get().userConfiguration().deviceName,
                           positiveAuthenticationEpochEvidence)) {
        return true;
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
    const std::lock_guard<std::recursive_mutex> lock(stateMutex_);
    if (networkLifecycle_ != nullptr) {
        networkLifecycle_->poll();
    }
    reevaluateWaitingForTrustedTime();
}

void FermentationApplication::publishOwningRecoveryEvidence(
    const CrossRolePlausibilityContext& evidence) {
    owningRecoveryEvidence_ = evidence;
}

RunPersistenceResult FermentationApplication::resumeFallback(
    const FermentationUiResumeFallbackCommand& command) {
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

// factory-reset transaction preserves the ordered epoch/auth/run handoff.
ConfigurationRecoveryResult FermentationApplication::
    beginAuthorizedFactoryReset() {  // NOLINT(readability-function-cognitive-complexity)
    ConfigurationRecoveryResult unavailable{
        ConfigurationRecoveryStatus::ConfigurationUnavailable, {}};
    if (configurationRecoveryService_ == nullptr ||
        configurationService_ == nullptr || stateStore_ == nullptr ||
        !storageEpoch_.has_value()) {
        return unavailable;
    }

    const auto previousEpoch = *storageEpoch_;
    const auto reset =
        configurationRecoveryService_->beginAuthorizedFactoryReset();
#if defined(APP_ISSUE_90_SLICE7_HARNESS)
    configurationRecoveryStatus_ = reset.status;
#endif
    if (reset.status != ConfigurationRecoveryStatus::FactoryResetCompleted) {
        return reset;
    }

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
    runPersistenceCoordinator_ = std::move(coordinator);
    storageEpoch_ = currentEpoch;
    runIdentity_.reset();
    pendingResume_.reset();
    pendingFallbackResume_.reset();
    pendingRecoverySource_.reset();
    owningRecoveryEvidence_.reset();
    recoveryDisposition_.reset();
    runtimeRunState_.reset();

    // Factory reset creates a genuinely new authentication epoch. The old
    // root/credential records remain physically present in the shared store,
    // but their epoch binding makes them unreachable. Establish the new
    // unprovisioned root through the same persisted first-consumer handoff as
    // cold boot, and revoke all old sessions/service leases immediately.
    if (authenticationDomain_ != nullptr && authenticationStore_ != nullptr &&
        webSessionManager_ != nullptr && bootstrapStore_ != nullptr &&
        mutationCoordinator_ != nullptr) {
        auto authMutation = mutationCoordinator_->tryAcquire();
        auto bootstrap = bootstrapStore_->scan();
        const auto root = authenticationStore_->readRoot(currentEpoch);
        bool authenticationResetCompleted = false;
        if (authMutation.status ==
                ConfigurationMutationAcquireStatus::Acquired &&
            bootstrap.status == ConfigurationBootstrapScanStatus::Available &&
            bootstrap.loaded.has_value() &&
            (root.status == AuthenticationReadStatus::NotFound ||
             root.status == AuthenticationReadStatus::DifferentEpoch)) {
            if (bootstrap.loaded->record.authDomainHandoff ==
                AuthDomainHandoffState::None) {
                const auto unconsumed = bootstrapStore_->writeAuthDomainHandoff(
                    *bootstrap.loaded, AuthDomainHandoffState::Unconsumed,
                    authMutation.lease);
                if (unconsumed.status ==
                        ConfigurationBootstrapWriteStatus::Success &&
                    unconsumed.loaded.has_value()) {
                    bootstrap.loaded = unconsumed.loaded;
                }
            }
            if (bootstrap.loaded->record.authDomainHandoff ==
                AuthDomainHandoffState::Unconsumed) {
                const auto inProgress = bootstrapStore_->writeAuthDomainHandoff(
                    *bootstrap.loaded, AuthDomainHandoffState::InProgress,
                    authMutation.lease);
                if (inProgress.status ==
                        ConfigurationBootstrapWriteStatus::Success &&
                    inProgress.loaded.has_value() &&
                    authenticationDomain_->initializeUnprovisioned(
                        currentEpoch) ==
                        AuthBootstrapStatus::BootstrapAllowed) {
                    const auto consumed =
                        bootstrapStore_->writeAuthDomainHandoff(
                            *inProgress.loaded,
                            AuthDomainHandoffState::Consumed,
                            authMutation.lease);
                    authenticationResetCompleted =
                        consumed.status ==
                        ConfigurationBootstrapWriteStatus::Success;
                    if (!authenticationResetCompleted) {
                        static_cast<void>(
                            bootstrapStore_->writeAuthDomainHandoff(
                                *inProgress.loaded,
                                AuthDomainHandoffState::Indeterminate,
                                authMutation.lease));
                    }
                } else if (inProgress.loaded.has_value()) {
                    static_cast<void>(bootstrapStore_->writeAuthDomainHandoff(
                        *inProgress.loaded,
                        AuthDomainHandoffState::Indeterminate,
                        authMutation.lease));
                }
            }
        }
        if (!authenticationResetCompleted) {
            requireService(FaultCode::ConfigurationUnavailable);
            return {
                ConfigurationRecoveryStatus::RunPersistenceHandoffUnavailable,
                reset.diagnostics};
        }
        webSessionManager_->revokeAll();
    }

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
    const std::lock_guard<std::recursive_mutex> lock(stateMutex_);
    return lifecycleState_ == ApplicationLifecycleState::Ready;
}

std::optional<device_platform::StorageEpoch>
FermentationApplication::currentStorageEpoch() const noexcept {
    const std::lock_guard<std::recursive_mutex> lock(stateMutex_);
    return storageEpoch_;
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

}  // namespace fermentation
