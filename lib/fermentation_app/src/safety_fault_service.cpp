#include "safety_fault_service.hpp"

#include <limits>

#include "actuator_planner.hpp"
#include "configuration_recovery_service.hpp"

namespace fermentation {
namespace {

bool increment(std::uint32_t& value) {
    if (value == std::numeric_limits<std::uint32_t>::max()) return false;
    ++value;
    return true;
}

}  // namespace

SafetyFaultService::SafetyFaultService(
    device_platform::IStateStore& store,
    device_platform::IResetController& resetController,
    device_platform::ITimeSource& timeSource,
    device_platform::IEventJournal* journal)
    : stateStore_(store),
      resetController_(resetController),
      timeSource_(timeSource),
      journal_(journal) {}

SafetyServiceStatus SafetyFaultService::begin(
    const FactoryNewSafetyProof& factoryProof) {
    const auto loaded = stateStore_.load(factoryProof);
    if (loaded.status ==
        SafetyRecordLoadStatus::NotFoundOutsideFactoryBootstrap) {
        return SafetyServiceStatus::FactoryBootstrapRequired;
    }
    if (loaded.status != SafetyRecordLoadStatus::Loaded &&
        loaded.status != SafetyRecordLoadStatus::FactoryInitialized) {
        return SafetyServiceStatus::PersistentReadFailed;
    }
    FaultCoreSnapshot snapshot;
    snapshot.count = loaded.record.latchCount;
    snapshot.revision = loaded.record.faultRevision;
    snapshot.instanceSequenceHighWatermark =
        loaded.record.faultInstanceSequence;
    for (std::size_t index = 0U; index < snapshot.count; ++index) {
        snapshot.records[index] = loaded.record.latches[index];
    }
    snapshot.criticalSafetyEventPending = loaded.record.safeBootRequired;
    if (!faultCore_.restoreSnapshot(snapshot)) {
        return SafetyServiceStatus::PersistentReadFailed;
    }
    record_ = loaded.record;
    configurationGateQualified_ = false;
    safetyRecoveryCapability_.reset();
    started_ = true;
    return SafetyServiceStatus::Ready;
}

SafetyBootResult SafetyFaultService::evaluateBoot() {
    SafetyBootResult result;
    if (!started_) {
        result.status = SafetyServiceStatus::NotStarted;
        return result;
    }
    const auto resetSnapshot = resetController_.observeBootReset();
    const bool sameObservation =
        resetSnapshot.valid && resetSnapshot.observationId != 0U &&
        resetSnapshot.observationId == record_.lastResetObservationId;
    SafetyStateRecord candidate = record_;
    FaultCore stagedCore = faultCore_;
    const bool safeBootBefore = record_.safeBootRequired;
    result.restart = restartEpisode_.evaluateBoot(candidate, resetSnapshot);
    bool recordNeedsCommit = result.restart.recordNeedsCommit;
    if (!resetSnapshot.valid || resetSnapshot.observationId == 0U) {
        candidate.safeBootRequired = true;
        recordNeedsCommit = true;
    }

    FaultCode bootFault = FaultCode::Unknown;
    if (result.restart.status == RestartBootStatus::UnknownFailClosed) {
        bootFault = FaultCode::Y4_006;
    } else if (result.restart.status == RestartBootStatus::EvidenceMismatch ||
               result.restart.status == RestartBootStatus::Overflow) {
        bootFault = FaultCode::Y4_011;
    } else if (result.restart.status == RestartBootStatus::SafeBootRequired) {
        bootFault = FaultCode::Y4_007;
    }
    FaultInstanceId bootFaultId;
    if (bootFault != FaultCode::Unknown &&
        (!sameObservation ||
         result.restart.status == RestartBootStatus::EvidenceMismatch)) {
        const auto raised = stagedCore.raise(
            {bootFault, 24U, candidate.restartEpisode.episodeId,
             timeSource_.monotonicMillis(), std::nullopt});
        bootFaultId = raised.instanceId;
        candidate.safeBootRequired = true;
        recordNeedsCommit = true;
    }
    if (!copyCoreToRecord(candidate, stagedCore)) {
        record_.safeBootRequired = true;
        result.status = SafetyServiceStatus::PersistentWriteFailed;
        result.safeBootRequired = true;
        return result;
    }
    recordNeedsCommit =
        recordNeedsCommit || candidate.safeBootRequired != safeBootBefore;

    bool safeBootExitRejected = false;
    const bool authorizedReset =
        !sameObservation &&
        result.restart.status == RestartBootStatus::AuthorizedReset;
    if (authorizedReset) {
        const bool fullyQualified =
            !stagedCore.hasBlockingFault() && configurationGateQualified_ &&
            candidate.restartEvidence.state != RestartEvidenceState::Pending &&
            !candidate.faultResetBootIntent.pending;
        if (fullyQualified) {
            candidate.safeBootRequired = false;
        } else {
            candidate.safeBootRequired = true;
            safeBootExitRejected = true;
        }
        result.restart.safeBootRequired = candidate.safeBootRequired;
        recordNeedsCommit =
            recordNeedsCommit || candidate.safeBootRequired != safeBootBefore;
    }

    if (recordNeedsCommit) {
        if (!increment(candidate.recordRevision)) {
            record_.safeBootRequired = true;
            result.status = SafetyServiceStatus::PersistentWriteFailed;
            result.safeBootRequired = true;
            return result;
        }
        if (stateStore_.commit(candidate).status !=
            SafetyRecordCommitStatus::Committed) {
            record_.safeBootRequired = true;
            result.status = SafetyServiceStatus::PersistentWriteFailed;
            result.safeBootRequired = true;
            return result;
        }
        faultCore_ = stagedCore;
        record_ = candidate;
    }

    result.safeBootRequired =
        record_.safeBootRequired || result.restart.safeBootRequired;
    result.status = result.safeBootRequired
                        ? SafetyServiceStatus::SafetyRejected
                        : SafetyServiceStatus::Ready;
    if (bootFaultId.valid()) {
        const auto* fault = faultCore_.find(bootFaultId);
        recordEvent(fault != nullptr &&
                            fault->status == FaultStatus::ActiveUnacknowledged
                        ? FaultEventType::FaultCreated
                        : FaultEventType::FaultEscalated,
                    fault, true);
    }
    if (!sameObservation && result.restart.evidenceId != 0U) {
        recordEvent(
            FaultEventType::RestartEpisodeAdvanced, faultCore_.dominant(), true,
            record_.restartEpisode.episodeId, result.restart.evidenceId);
    }
    if (!safeBootBefore && record_.safeBootRequired) {
        recordEvent(FaultEventType::SafeBootEntered, faultCore_.dominant(),
                    true);
    }
    if (authorizedReset) {
        recordEvent(safeBootExitRejected ? FaultEventType::SafeBootExitRejected
                                         : FaultEventType::SafeBootExitDecided,
                    faultCore_.dominant(), !safeBootExitRejected);
    }
    return result;
}

bool SafetyFaultService::copyCoreToRecord(SafetyStateRecord& candidate) const {
    return copyCoreToRecord(candidate, faultCore_);
}

bool SafetyFaultService::copyCoreToRecord(SafetyStateRecord& candidate,
                                          const FaultCore& core) const {
    const auto snapshot = core.snapshot();
    if (snapshot.count > candidate.latches.size()) return false;
    candidate.faultRevision = snapshot.revision;
    candidate.latchCount = 0U;
    candidate.dominantCode = FaultCode::Unknown;
    for (std::size_t index = 0U; index < snapshot.count; ++index) {
        const auto& fault = snapshot.records[index];
        const bool isResetIntentTarget =
            candidate.faultResetBootIntent.pending &&
            candidate.faultResetBootIntent.targetFault == fault.instanceId;
        if (!isLatchedFaultClass(fault.faultClass) ||
            (fault.status == FaultStatus::Cleared && !isResetIntentTarget)) {
            continue;
        }
        if (candidate.latchCount >= candidate.latches.size()) return false;
        candidate.latches[candidate.latchCount++] = fault;
    }
    // The sequence is a persistent high-watermark. It must not be rebuilt from
    // the currently retained latch set because cleared faults are intentionally
    // absent from that set.
    candidate.faultInstanceSequence = snapshot.instanceSequenceHighWatermark;
    if (const auto* dominant = core.dominant();
        dominant != nullptr && isLatchedFaultClass(dominant->faultClass) &&
        dominant->status != FaultStatus::Cleared) {
        candidate.dominantCode = dominant->code;
    }
    candidate.safeBootRequired =
        candidate.safeBootRequired ||
        (core.dominant() != nullptr &&
         core.dominant()->faultClass == FaultClass::LatchedSystemFault);
    return true;
}

SafetyServiceStatus SafetyFaultService::persistCoreMutation(
    const FaultCoreSnapshot& before) {
    SafetyStateRecord candidate = record_;
    if (!copyCoreToRecord(candidate) || !increment(candidate.recordRevision)) {
        static_cast<void>(faultCore_.restoreSnapshot(before));
        record_.safeBootRequired = true;
        return SafetyServiceStatus::PersistentWriteFailed;
    }
    const auto result = stateStore_.commit(candidate);
    if (result.status != SafetyRecordCommitStatus::Committed) {
        static_cast<void>(faultCore_.restoreSnapshot(before));
        record_.safeBootRequired = true;
        return SafetyServiceStatus::PersistentWriteFailed;
    }
    record_ = candidate;
    return SafetyServiceStatus::Ready;
}

SafetyServiceStatus SafetyFaultService::persistSafeBootLock() {
    SafetyStateRecord candidate = record_;
    candidate.safeBootRequired = true;
    if (!increment(candidate.recordRevision)) {
        return SafetyServiceStatus::PersistentWriteFailed;
    }
    const auto result = stateStore_.commit(candidate);
    if (result.status != SafetyRecordCommitStatus::Committed) {
        record_.safeBootRequired = true;
        return SafetyServiceStatus::PersistentWriteFailed;
    }
    record_ = candidate;
    return SafetyServiceStatus::Ready;
}

SafetyServiceStatus SafetyFaultService::raiseFault(
    const FaultRaiseRequest& request) {
    if (!started_) return SafetyServiceStatus::NotStarted;
    const FaultCoreSnapshot before = faultCore_.snapshot();
    const auto raised = faultCore_.raise(request);
    if (raised.status == FaultRaiseStatus::CapacityReached) {
        record_.safeBootRequired = true;
        const auto persistStatus = persistCoreMutation(before);
        return persistStatus == SafetyServiceStatus::Ready
                   ? SafetyServiceStatus::FaultCapacityReached
                   : persistStatus;
    }
    if (raised.status == FaultRaiseStatus::InvalidInput) {
        return SafetyServiceStatus::InvalidFault;
    }
    if (raised.status == FaultRaiseStatus::RevisionOverflow) {
        record_.safeBootRequired = true;
        const auto persistStatus = persistCoreMutation(before);
        return persistStatus == SafetyServiceStatus::Ready
                   ? SafetyServiceStatus::PersistentWriteFailed
                   : persistStatus;
    }
    const auto* fault = faultCore_.find(raised.instanceId);
    const auto status = persistCoreMutation(before);
    recordEvent(raised.status == FaultRaiseStatus::Created
                    ? FaultEventType::FaultCreated
                    : FaultEventType::FaultEscalated,
                fault, status == SafetyServiceStatus::Ready);
    return status;
}

SafetyServiceStatus SafetyFaultService::consumeWatchdogEvidence(
    const ActuatorWatchdogFaultEvidence& evidence) {
    if (!started_) return SafetyServiceStatus::NotStarted;
    return raiseFault(
        {FaultCode::S3_008, 23U,
         static_cast<std::uint32_t>(
             evidence.lastObservedSequenceHighWatermarkBeforeFault),
         evidence.detectedAtMonotonicMillis, std::nullopt,
         evidence.lastObservedSequenceHighWatermarkBeforeFault});
}

SafetyServiceStatus SafetyFaultService::consumeConfigurationStatus(
    ConfigurationSafetyStatus status, std::uint32_t sourceKey,
    std::uint32_t correlationKey) {
    FaultCode code = FaultCode::Unknown;
    switch (status) {
        case ConfigurationSafetyStatus::Operational:
            configurationGateQualified_ = true;
            return started_ ? SafetyServiceStatus::Ready
                            : SafetyServiceStatus::NotStarted;
        case ConfigurationSafetyStatus::ConfigurationRuntimeFailure:
            code = FaultCode::Y4_001;
            break;
        case ConfigurationSafetyStatus::ConfigurationCommitIndeterminate:
            code = FaultCode::Y4_002;
            break;
        case ConfigurationSafetyStatus::ConfigurationUnavailable:
            code = FaultCode::Y4_003;
            break;
        case ConfigurationSafetyStatus::ConfigurationIntegrityFailure:
            code = FaultCode::Y4_004;
            break;
        case ConfigurationSafetyStatus::Unknown:
            code = FaultCode::Y4_011;
            break;
    }
    configurationGateQualified_ = false;
    return raiseFault({code, sourceKey, correlationKey,
                       timeSource_.monotonicMillis(), std::nullopt});
}

SafetyServiceStatus SafetyFaultService::consumeConfigurationStatus(
    ConfigurationServiceMode status, std::uint32_t sourceKey,
    std::uint32_t correlationKey) {
    switch (status) {
        case ConfigurationServiceMode::Operational:
            configurationGateQualified_ = true;
            return started_ ? SafetyServiceStatus::Ready
                            : SafetyServiceStatus::NotStarted;
        case ConfigurationServiceMode::CommitIndeterminate:
            return consumeConfigurationStatus(
                ConfigurationSafetyStatus::ConfigurationCommitIndeterminate,
                sourceKey, correlationKey);
        case ConfigurationServiceMode::RuntimeFailure:
            return consumeConfigurationStatus(
                ConfigurationSafetyStatus::ConfigurationRuntimeFailure,
                sourceKey, correlationKey);
        case ConfigurationServiceMode::NoRuntime:
        case ConfigurationServiceMode::RecoveryPreparing:
        case ConfigurationServiceMode::ResetPreparing:
        case ConfigurationServiceMode::ResetEligibleNoRuntime:
        case ConfigurationServiceMode::EpochResetting:
        case ConfigurationServiceMode::BootstrapFinalizationPending:
        case ConfigurationServiceMode::CommitInProgress:
            return consumeConfigurationStatus(
                ConfigurationSafetyStatus::ConfigurationUnavailable, sourceKey,
                correlationKey);
    }
    return consumeConfigurationStatus(ConfigurationSafetyStatus::Unknown,
                                      sourceKey, correlationKey);
}

SafetyServiceStatus SafetyFaultService::consumeConfigurationStatus(
    ConfigurationCommitStatus status, std::uint32_t sourceKey,
    std::uint32_t correlationKey) {
    switch (status) {
        case ConfigurationCommitStatus::ConfigurationCommitIndeterminate:
            return consumeConfigurationStatus(
                ConfigurationSafetyStatus::ConfigurationCommitIndeterminate,
                sourceKey, correlationKey);
        case ConfigurationCommitStatus::ConfigurationRuntimeFailure:
            return consumeConfigurationStatus(
                ConfigurationSafetyStatus::ConfigurationRuntimeFailure,
                sourceKey, correlationKey);
        case ConfigurationCommitStatus::Activated:
        case ConfigurationCommitStatus::NoChange:
            configurationGateQualified_ = true;
            return started_ ? SafetyServiceStatus::Ready
                            : SafetyServiceStatus::NotStarted;
        case ConfigurationCommitStatus::PreviewNotFound:
        case ConfigurationCommitStatus::PreviewSuperseded:
        case ConfigurationCommitStatus::ConfigurationMutationBusy:
        case ConfigurationCommitStatus::ConfigurationConflictFailure:
        case ConfigurationCommitStatus::ConfigurationValidationFailure:
        case ConfigurationCommitStatus::PersistenceFailure:
        case ConfigurationCommitStatus::CapacityFailure:
            return consumeConfigurationStatus(
                ConfigurationSafetyStatus::ConfigurationUnavailable, sourceKey,
                correlationKey);
    }
    return consumeConfigurationStatus(ConfigurationSafetyStatus::Unknown,
                                      sourceKey, correlationKey);
}

SafetyServiceStatus SafetyFaultService::consumeConfigurationStatus(
    ConfigurationRecoveryStatus status, std::uint32_t sourceKey,
    std::uint32_t correlationKey) {
    switch (status) {
        case ConfigurationRecoveryStatus::RuntimeReady:
        case ConfigurationRecoveryStatus::FactoryInitializationCompleted:
        case ConfigurationRecoveryStatus::FactoryResetCompleted:
            configurationGateQualified_ = true;
            return started_ ? SafetyServiceStatus::Ready
                            : SafetyServiceStatus::NotStarted;
        case ConfigurationRecoveryStatus::ConfigurationIntegrityFailure:
        case ConfigurationRecoveryStatus::UnsupportedNewerConfigurationSchema:
            return consumeConfigurationStatus(
                ConfigurationSafetyStatus::ConfigurationIntegrityFailure,
                sourceKey, correlationKey);
        case ConfigurationRecoveryStatus::ConfigurationCommitIndeterminate:
        case ConfigurationRecoveryStatus::BootstrapCommitIndeterminate:
        case ConfigurationRecoveryStatus::
            ConfigurationRecordOutcomeIndeterminate:
            return consumeConfigurationStatus(
                ConfigurationSafetyStatus::ConfigurationCommitIndeterminate,
                sourceKey, correlationKey);
        case ConfigurationRecoveryStatus::ConfigurationUnavailable:
        case ConfigurationRecoveryStatus::PersistenceReadFailure:
        case ConfigurationRecoveryStatus::PersistenceCapacityFailure:
        case ConfigurationRecoveryStatus::PersistenceWriteFailure:
        case ConfigurationRecoveryStatus::RuntimePreparationFailure:
            return consumeConfigurationStatus(
                ConfigurationSafetyStatus::ConfigurationUnavailable, sourceKey,
                correlationKey);
        case ConfigurationRecoveryStatus::ConfigurationMutationBusy:
        case ConfigurationRecoveryStatus::ConfigurationModelBudgetBusy:
        case ConfigurationRecoveryStatus::StateTransitionRejected:
        case ConfigurationRecoveryStatus::CounterOverflow:
            configurationGateQualified_ = false;
            return consumeConfigurationStatus(
                ConfigurationSafetyStatus::Unknown, sourceKey, correlationKey);
    }
    return consumeConfigurationStatus(ConfigurationSafetyStatus::Unknown,
                                      sourceKey, correlationKey);
}

SafetyServiceStatus SafetyFaultService::consumeConfigurationRecoveryResult(
    const ConfigurationRecoveryResult& result, std::uint32_t sourceKey,
    std::uint32_t correlationKey) {
    // This is the producer-facing bridge: the real recovery result, including
    // its safety-producer classification, is mapped once into the canonical
    // #24 fault core. No parallel configuration fault domain is introduced.
    return consumeConfigurationStatus(result.status, sourceKey, correlationKey);
}

SafetyServiceStatus SafetyFaultService::acknowledgeFault(
    FaultInstanceId id, std::uint32_t expectedRevision) {
    if (!started_) return SafetyServiceStatus::NotStarted;
    const FaultCoreSnapshot before = faultCore_.snapshot();
    if (!faultCore_.acknowledge(id, expectedRevision)) {
        return SafetyServiceStatus::StaleFault;
    }
    const auto status = persistCoreMutation(before);
    recordEvent(FaultEventType::FaultAcknowledged, faultCore_.find(id),
                status == SafetyServiceStatus::Ready);
    return status;
}

SafetyServiceStatus SafetyFaultService::clearFaultCause(
    FaultInstanceId id, std::uint32_t expectedRevision) {
    if (!started_) return SafetyServiceStatus::NotStarted;
    const FaultCoreSnapshot before = faultCore_.snapshot();
    if (!faultCore_.markCauseCleared(id, expectedRevision)) {
        return SafetyServiceStatus::StaleFault;
    }
    const auto status = persistCoreMutation(before);
    recordEvent(FaultEventType::FaultCauseCleared, faultCore_.find(id),
                status == SafetyServiceStatus::Ready);
    return status;
}

std::optional<SafetyRecoveryRequest> SafetyFaultService::issueSafetyRecovery(
    const SafetyRecoveryQualification& qualification) {
    if (!started_) return std::nullopt;
    SafetyRecoveryRequest candidate(qualification, this);
    const auto* target = faultCore_.find(qualification.targetFault);
    bool otherBlockingFault = false;
    const auto snapshot = faultCore_.snapshot();
    for (std::size_t index = 0U; index < snapshot.count; ++index) {
        if (snapshot.records[index].instanceId != qualification.targetFault &&
            isBlockingFault(snapshot.records[index])) {
            otherBlockingFault = true;
            break;
        }
    }
    const bool targetEligible =
        target != nullptr && target->code == FaultCode::S3_004 &&
        target->latched && target->causeActive &&
        target->faultRevision == qualification.faultRevision;
    const bool accepted = candidate.structurallyValid() && targetEligible &&
                          !otherBlockingFault && !record_.safeBootRequired;
    recordEvent(FaultEventType::SafetyRecoveryAttempted, target, accepted);
    if (!accepted) {
        recordEvent(FaultEventType::SafetyRecoveryAborted, target, false);
        return std::nullopt;
    }
    safetyRecoveryCapability_ = candidate;
    return safetyRecoveryCapability_;
}

SafetyServiceStatus SafetyFaultService::completeSafetyRecovery(
    const SafetyRecoveryRequest& request, bool succeeded) {
    const auto* target =
        started_ ? faultCore_.find(request.targetFault()) : nullptr;
    const bool sameCapability =
        safetyRecoveryCapability_.has_value() && request.structurallyValid() &&
        request.targetFault() == safetyRecoveryCapability_->targetFault() &&
        request.faultRevision() == safetyRecoveryCapability_->faultRevision() &&
        request.sequence() == safetyRecoveryCapability_->sequence() &&
        request.issuer_ == this;
    if (!started_ || !sameCapability || target == nullptr ||
        target->code != FaultCode::S3_004 || !target->causeActive ||
        target->faultRevision != request.faultRevision()) {
        recordEvent(FaultEventType::SafetyRecoveryAborted, target, false);
        return started_ ? SafetyServiceStatus::SafetyRejected
                        : SafetyServiceStatus::NotStarted;
    }
    if (!succeeded) {
        safetyRecoveryCapability_.reset();
        recordEvent(FaultEventType::SafetyRecoveryAborted, target, false);
        return SafetyServiceStatus::SafetyRejected;
    }
    safetyRecoveryCapability_.reset();
    recordEvent(FaultEventType::SafetyRecoverySucceeded, target, true);
    return SafetyServiceStatus::Ready;
}

std::optional<FaultResetAuthorization>
SafetyFaultService::prepareFaultResetAuthorization(
    FaultInstanceId id, std::uint32_t expectedRevision) {
    if (!started_ || !id.valid() || expectedRevision == 0U) {
        return std::nullopt;
    }
    const auto* target = faultCore_.find(id);
    if (target == nullptr ||
        target->status != FaultStatus::CauseClearedLocked ||
        target->causeActive || !target->latched ||
        target->faultRevision != expectedRevision) {
        return std::nullopt;
    }
    const auto snapshot = faultCore_.snapshot();
    for (std::size_t index = 0U; index < snapshot.count; ++index) {
        const auto& other = snapshot.records[index];
        if (other.instanceId != id && isBlockingFault(other)) {
            return std::nullopt;
        }
    }
    if (nextAuthorityToken_ == 0U) return std::nullopt;
    const auto token = nextAuthorityToken_++;
    return FaultResetAuthorization{
        id, expectedRevision, snapshot.revision, true, true, true, true, token};
}

SafetyResetResult SafetyFaultService::resetFault(FaultInstanceId id,
                                                 std::uint32_t expectedRevision,
                                                 ActuatorPlanner* planner) {
    SafetyResetResult result;
    result.targetFault = id;
    if (!started_) {
        result.status = SafetyServiceStatus::NotStarted;
        return result;
    }
    const auto* target = faultCore_.find(id);
    if (target == nullptr || target->faultRevision != expectedRevision) {
        result.status = SafetyServiceStatus::StaleFault;
        recordEvent(FaultEventType::FaultResetRejected, target, false);
        return result;
    }
    const auto authorization =
        prepareFaultResetAuthorization(id, expectedRevision);
    if (!authorization.has_value()) {
        result.status = SafetyServiceStatus::SafetyRejected;
        recordEvent(FaultEventType::FaultResetRejected, target, false);
        return result;
    }
    const FaultCode targetCode = target->code;
    if (targetCode == FaultCode::S3_008 && planner == nullptr) {
        result.status = SafetyServiceStatus::SafetyRejected;
        recordEvent(FaultEventType::FaultResetRejected, target, false);
        return result;
    }

    RunCommandState commandState;
    projectTo(commandState);
    FaultResetRequest request;
    request.envelope.id =
        nextAuthorityToken_ == 0U ? 1U : nextAuthorityToken_++;
    request.envelope.monotonicMillis = timeSource_.monotonicMillis();
    request.envelope.expectedStateSequence =
        commandState.processState.transitionSequence;
    request.envelope.expectedFaultRevision = expectedRevision;
    request.envelope.confirmed = true;
    request.targetFault = id;
    const auto decision =
        decideFaultReset(commandState, request, *authorization);
    if (!decision.proposed()) {
        result.status = SafetyServiceStatus::SafetyRejected;
        recordEvent(FaultEventType::FaultResetRejected, target, false);
        return result;
    }

    FaultCore stagedCore = faultCore_;
    if (!stagedCore.clearAfterVerifiedReset(id, expectedRevision)) {
        result.status = SafetyServiceStatus::SafetyRejected;
        recordEvent(FaultEventType::FaultResetRejected, target, false);
        return result;
    }
    SafetyStateRecord candidate = record_;
    if (!restartEpisode_.prepareFaultResetBootIntent(candidate, id,
                                                     expectedRevision) ||
        !copyCoreToRecord(candidate, stagedCore) ||
        !increment(candidate.recordRevision)) {
        result.status = SafetyServiceStatus::PersistentWriteFailed;
        return result;
    }
    const auto commit = stateStore_.commit(candidate);
    if (commit.status != SafetyRecordCommitStatus::Committed) {
        result.status = SafetyServiceStatus::PersistentWriteFailed;
        return result;
    }
    // Write-before-apply: neither FaultCore nor the live record changes before
    // the complete reset intent and cleared target are committed.
    faultCore_ = stagedCore;
    record_ = candidate;
    recordEvent(FaultEventType::FaultResetCommitted, faultCore_.find(id), true);
    if (planner != nullptr && targetCode == FaultCode::S3_008) {
        planner->applyExternalWatchdogFaultReset(timeSource_.monotonicMillis());
    }
    const auto resetResult = resetController_.requestRestart(
        {device_platform::ControlledRestartPurpose::AuthorizedFaultReset});
    if (resetResult == device_platform::ControlledRestartResult::Rejected) {
        static_cast<void>(persistSafeBootLock());
        result.status = SafetyServiceStatus::ResetBootRejected;
        return result;
    }
    if (resetResult ==
        device_platform::ControlledRestartResult::OutcomeUnknown) {
        static_cast<void>(persistSafeBootLock());
        result.status = SafetyServiceStatus::ResetBootOutcomeUnknown;
        return result;
    }
    result.status = SafetyServiceStatus::ResetCommitted;
    return result;
}

SafetyResetResult SafetyFaultService::resetFault(
    FaultInstanceId id, std::uint32_t expectedRevision,
    bool /*authorizationSatisfied*/, ActuatorPlanner* planner) {
    // Legacy callers cannot turn a positive boolean into a reset authority.
    SafetyResetResult result;
    result.targetFault = id;
    result.status = started_ ? SafetyServiceStatus::SafetyRejected
                             : SafetyServiceStatus::NotStarted;
    recordEvent(FaultEventType::FaultResetRejected,
                started_ ? faultCore_.find(id) : nullptr, false);
    static_cast<void>(expectedRevision);
    static_cast<void>(planner);
    return result;
}

SafetyServiceStatus SafetyFaultService::requestControlledSafetyRestart(
    FaultInstanceId id, std::uint32_t expectedRevision) {
    if (!started_) return SafetyServiceStatus::NotStarted;
    const auto* target = faultCore_.find(id);
    if (target == nullptr || target->status == FaultStatus::Cleared ||
        !target->latched || target->faultRevision != expectedRevision ||
        target->controlledRestartUsed || record_.safeBootRequired) {
        return SafetyServiceStatus::SafetyRejected;
    }
    FaultCore stagedCore = faultCore_;
    if (!stagedCore.markControlledRestartUsed(id, expectedRevision)) {
        return SafetyServiceStatus::PersistentWriteFailed;
    }
    const auto stagedRevision = stagedCore.snapshot().revision;
    SafetyStateRecord candidate = record_;
    if (!copyCoreToRecord(candidate, stagedCore) ||
        !restartEpisode_.prepareControlledRestart(candidate, id,
                                                  stagedRevision) ||
        !increment(candidate.recordRevision)) {
        return SafetyServiceStatus::PersistentWriteFailed;
    }
    candidate.faultRevision = stagedRevision;
    const auto commit = stateStore_.commit(candidate);
    if (commit.status != SafetyRecordCommitStatus::Committed) {
        static_cast<void>(persistSafeBootLock());
        return SafetyServiceStatus::PersistentWriteFailed;
    }
    faultCore_ = stagedCore;
    record_ = candidate;
    const auto resetResult = resetController_.requestRestart(
        {device_platform::ControlledRestartPurpose::ControlledSafetyRestart});
    if (resetResult != device_platform::ControlledRestartResult::Accepted) {
        static_cast<void>(persistSafeBootLock());
        return resetResult == device_platform::ControlledRestartResult::Rejected
                   ? SafetyServiceStatus::ResetBootRejected
                   : SafetyServiceStatus::ResetBootOutcomeUnknown;
    }
    return SafetyServiceStatus::Ready;
}

SafetyServiceStatus SafetyFaultService::advanceStableWindow(bool stable) {
    if (!started_) return SafetyServiceStatus::NotStarted;
    SafetyStateRecord candidate = record_;
    const bool wasRunning = candidate.restartEpisode.stableWindowRunning;
    const auto wasStarted =
        candidate.restartEpisode.stableWindowStartedAtMillis;
    const bool closed = restartEpisode_.advanceStableWindow(
        candidate, timeSource_.monotonicMillis(), stable);
    const bool changed =
        closed || wasRunning != candidate.restartEpisode.stableWindowRunning ||
        wasStarted != candidate.restartEpisode.stableWindowStartedAtMillis;
    if (!changed) return SafetyServiceStatus::Ready;
    if (!increment(candidate.recordRevision)) {
        static_cast<void>(persistSafeBootLock());
        return SafetyServiceStatus::PersistentWriteFailed;
    }
    const auto commit = stateStore_.commit(candidate);
    if (commit.status != SafetyRecordCommitStatus::Committed) {
        static_cast<void>(persistSafeBootLock());
        return SafetyServiceStatus::PersistentWriteFailed;
    }
    record_ = candidate;
    if (closed) {
        recordEvent(FaultEventType::RestartEpisodeClosed, faultCore_.dominant(),
                    true, record_.restartEpisode.episodeId,
                    record_.restartEvidence.evidenceId);
    }
    return SafetyServiceStatus::Ready;
}

SafetyDisposition SafetyFaultService::disposition() const {
    if (!started_ || record_.safeBootRequired) {
        return SafetyDisposition::ImmediateStop;
    }
    return faultCore_.disposition();
}

bool SafetyFaultService::safeBootRequired() const {
    return !started_ || record_.safeBootRequired;
}

void SafetyFaultService::projectTo(RunCommandState& state) const {
    if (!started_) {
        state.criticalSafetyEventPending = true;
        return;
    }
    applyFaultCoreProjection(state, faultCore_);
    if (record_.safeBootRequired) state.criticalSafetyEventPending = true;
}

ActuatorSafetyGateInput SafetyFaultService::actuatorGateInput() const {
    ActuatorSafetyGateInput result;
    if (!started_) {
        result.status = ActuatorSafetyGateStatus::ImmediateStop;
        return result;
    }
    if (safetyRecoveryCapability_.has_value() &&
        safetyRecoveryCapability_->structurallyValid()) {
        const auto& recovery = *safetyRecoveryCapability_;
        const auto* target = faultCore_.find(recovery.targetFault());
        bool onlyRecoverableBlockingFault =
            !record_.safeBootRequired && target != nullptr &&
            target->code == FaultCode::S3_004 && target->latched &&
            target->causeActive &&
            target->faultRevision == recovery.faultRevision();
        const auto snapshot = faultCore_.snapshot();
        for (std::size_t index = 0U; index < snapshot.count; ++index) {
            if (snapshot.records[index].instanceId != recovery.targetFault() &&
                isBlockingFault(snapshot.records[index])) {
                onlyRecoverableBlockingFault = false;
            }
        }
        if (onlyRecoverableBlockingFault) {
            result.status = ActuatorSafetyGateStatus::SafetyRecovery;
            result.safetyRecovery = recovery;
            result.authority_ = this;
            return result;
        }
    }
    result.status = record_.safeBootRequired || faultCore_.hasBlockingFault()
                        ? ActuatorSafetyGateStatus::ImmediateStop
                        : ActuatorSafetyGateStatus::Allowed;
    return result;
}

ActuatorSafetyGateInput SafetyFaultService::actuatorGateInput(
    const std::optional<SafetyRecoveryRequest>&) const {
    // The argument is intentionally ignored. A caller may pass a copied or
    // lookalike value, but only the service-owned capability can open this
    // narrow gate.
    return actuatorGateInput();
}

void SafetyFaultService::recordEvent(FaultEventType type,
                                     const FaultRecord* fault, bool accepted,
                                     std::uint32_t episodeId,
                                     std::uint32_t restartEvidenceId) const {
    FaultEventProjection event;
    event.type = type;
    event.accepted = accepted;
    if (fault != nullptr) {
        event.code = fault->code;
        event.faultInstanceId = fault->instanceId;
        event.primaryFaultId = fault->primaryFaultId;
        event.faultRevision = fault->faultRevision;
        event.diagnosticSequenceHighWatermark =
            fault->diagnosticSequenceHighWatermark;
    }
    event.episodeId =
        episodeId == 0U ? record_.restartEpisode.episodeId : episodeId;
    event.restartEvidenceId = restartEvidenceId == 0U
                                  ? record_.restartEvidence.evidenceId
                                  : restartEvidenceId;
    static_cast<void>(
        recordFaultEvent(journal_, timeSource_.monotonicMillis(), event));
}

}  // namespace fermentation
