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

std::size_t persistentLatchCount(const FaultCore& core) {
    const auto snapshot = core.snapshot();
    std::size_t count = 0U;
    for (std::size_t index = 0U; index < snapshot.count; ++index) {
        const auto& fault = snapshot.records[index];
        if (isLatchedFaultClass(fault.faultClass) &&
            fault.status != FaultStatus::Cleared) {
            ++count;
        }
    }
    return count;
}

bool hasOtherBlockingFault(const FaultCore& core, FaultInstanceId target) {
    const auto snapshot = core.snapshot();
    for (std::size_t index = 0U; index < snapshot.count; ++index) {
        const auto& fault = snapshot.records[index];
        if (fault.instanceId != target && isBlockingFault(fault)) return true;
    }
    return false;
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
    started_ = true;
    return SafetyServiceStatus::Ready;
}

SafetyBootResult SafetyFaultService::evaluateBoot() {
    SafetyBootResult result;
    if (!started_) return result;

    const auto resetSnapshot = resetController_.observeBootReset();
    const bool sameObservation =
        resetSnapshot.valid && resetSnapshot.observationId != 0U &&
        resetSnapshot.observationId == record_.lastResetObservationId;
    SafetyStateRecord candidate = record_;
    FaultCore stagedCore = faultCore_;
    const bool safeBootBefore = candidate.safeBootRequired;
    result.restart = restartEpisode_.evaluateBoot(candidate, resetSnapshot);
    bool needsCommit = result.restart.recordNeedsCommit;

    FaultCode bootFault = FaultCode::Unknown;
    if (result.restart.status == RestartBootStatus::UnknownFailClosed ||
        result.restart.status == RestartBootStatus::EvidenceMismatch ||
        result.restart.status == RestartBootStatus::Overflow) {
        bootFault = FaultCode::Y4_008;
    } else if (result.restart.status == RestartBootStatus::SafeBootRequired) {
        bootFault = FaultCode::Y4_009;
    }

    FaultInstanceId bootFaultId;
    if (bootFault != FaultCode::Unknown &&
        (!sameObservation ||
         result.restart.status == RestartBootStatus::EvidenceMismatch)) {
        const auto raised = stagedCore.raise(
            {bootFault, 24U, candidate.restartEpisode.episodeId,
             timeSource_.monotonicMillis(), std::nullopt});
        if (raised.status != FaultRaiseStatus::InvalidInput) {
            bootFaultId = raised.instanceId;
            candidate.safeBootRequired = true;
            needsCommit = true;
        }
    }

    const bool authorizedExit =
        !sameObservation &&
        result.restart.status == RestartBootStatus::AuthorizedReset;
    if (authorizedExit) {
        const bool checksPass = !stagedCore.hasBlockingFault() &&
                                configurationGateQualified_ &&
                                !candidate.capacityFailureLatched;
        if (!checksPass)
            candidate.safeBootRequired = true;
        else
            candidate.safeBootRequired = false;
        needsCommit =
            needsCommit || candidate.safeBootRequired != safeBootBefore;
        result.restart.safeBootRequired = candidate.safeBootRequired;
    }

    if (!copyCoreToRecord(candidate, stagedCore)) {
        record_.safeBootRequired = true;
        result.status = SafetyServiceStatus::PersistentWriteFailed;
        result.safeBootRequired = true;
        return result;
    }
    needsCommit = needsCommit || candidate.safeBootRequired != safeBootBefore;
    if (needsCommit) {
        if (!increment(candidate.recordRevision) ||
            stateStore_.commit(candidate).status !=
                SafetyRecordCommitStatus::Committed) {
            // A failed safety commit never grants an Allowed projection. The
            // RAM lock is deliberately retained even when persistence failed.
            record_.safeBootRequired = true;
            result.status = SafetyServiceStatus::PersistentWriteFailed;
            result.safeBootRequired = true;
            return result;
        }
        faultCore_ = stagedCore;
        record_ = candidate;
    }

    result.safeBootRequired = record_.safeBootRequired;
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
    if (authorizedExit) {
        recordEvent(record_.safeBootRequired
                        ? FaultEventType::SafeBootExitRejected
                        : FaultEventType::SafeBootExitDecided,
                    faultCore_.dominant(), !record_.safeBootRequired);
    }
    return result;
}

bool SafetyFaultService::copyCoreToRecord(SafetyStateRecord& candidate) const {
    return copyCoreToRecord(candidate, faultCore_);
}

bool SafetyFaultService::copyCoreToRecord(SafetyStateRecord& candidate,
                                          const FaultCore& core) const {
    const auto snapshot = core.snapshot();
    if (persistentLatchCount(core) > candidate.latches.size()) return false;
    candidate.faultRevision = snapshot.revision;
    candidate.latchCount = 0U;
    candidate.dominantCode = FaultCode::Unknown;
    for (std::size_t index = 0U; index < snapshot.count; ++index) {
        const auto& fault = snapshot.records[index];
        if (!isLatchedFaultClass(fault.faultClass) ||
            fault.status == FaultStatus::Cleared) {
            continue;
        }
        if (candidate.latchCount >= candidate.latches.size()) return false;
        candidate.latches[candidate.latchCount++] = fault;
    }
    candidate.faultInstanceSequence = snapshot.instanceSequenceHighWatermark;
    if (const auto* dominant = core.dominant();
        dominant != nullptr && isLatchedFaultClass(dominant->faultClass) &&
        dominant->status != FaultStatus::Cleared) {
        candidate.dominantCode = dominant->code;
    }
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
    if (!increment(candidate.recordRevision) ||
        stateStore_.commit(candidate).status !=
            SafetyRecordCommitStatus::Committed) {
        record_.safeBootRequired = true;
        return SafetyServiceStatus::PersistentWriteFailed;
    }
    record_ = candidate;
    return SafetyServiceStatus::Ready;
}

SafetyServiceStatus SafetyFaultService::raiseFault(
    const FaultRaiseRequest& request) {
    if (!started_) return SafetyServiceStatus::NotStarted;
    const auto normalized = normalizeFaultCode(request.code);
    const bool latch = isLatchedFaultClass(faultClassForCode(normalized));
    if (normalized == FaultCode::Y4_006 ||
        (latch &&
         persistentLatchCount(faultCore_) >= kMaximumPersistedLatches)) {
        SafetyStateRecord candidate = record_;
        candidate.safeBootRequired = true;
        candidate.capacityFailureLatched = true;
        if (!increment(candidate.capacityFailureRevision)) {
            record_.safeBootRequired = true;
            return SafetyServiceStatus::PersistentWriteFailed;
        }
        candidate.capacityFailureSourceKey = request.sourceKey;
        candidate.capacityFailureCorrelationKey = request.correlationKey;
        if (!increment(candidate.recordRevision) ||
            stateStore_.commit(candidate).status !=
                SafetyRecordCommitStatus::Committed) {
            record_.safeBootRequired = true;
            return SafetyServiceStatus::PersistentWriteFailed;
        }
        record_ = candidate;
        return SafetyServiceStatus::FaultCapacityReached;
    }
    const FaultCoreSnapshot before = faultCore_.snapshot();
    const auto raised = faultCore_.raise(request);
    if (raised.status == FaultRaiseStatus::CapacityReached ||
        raised.status == FaultRaiseStatus::RevisionOverflow) {
        record_.safeBootRequired = true;
        record_.capacityFailureLatched = true;
        record_.capacityFailureSourceKey = request.sourceKey;
        record_.capacityFailureCorrelationKey = request.correlationKey;
        const auto status = persistCoreMutation(before);
        return status == SafetyServiceStatus::Ready
                   ? SafetyServiceStatus::FaultCapacityReached
                   : status;
    }
    if (raised.status == FaultRaiseStatus::InvalidInput) {
        return SafetyServiceStatus::InvalidFault;
    }
    const auto* fault = faultCore_.find(raised.instanceId);
    const auto status = persistCoreMutation(before);
    recordEvent(raised.status == FaultRaiseStatus::Created
                    ? FaultEventType::FaultCreated
                    : FaultEventType::FaultEscalated,
                fault, status == SafetyServiceStatus::Ready);
    return status;
}

SafetyServiceStatus SafetyFaultService::consumeProcessMessage(
    ProcessMessage message, std::uint32_t sourceKey,
    std::uint32_t correlationKey) {
    if (!started_) return SafetyServiceStatus::NotStarted;
    switch (message) {
        case ProcessMessage::TargetReachTimeExceeded:
            return raiseFault({FaultCode::P1_001, sourceKey, correlationKey,
                               timeSource_.monotonicMillis(), std::nullopt});
        case ProcessMessage::ProductInsertionRequested:
        case ProcessMessage::RunCompleted:
        case ProcessMessage::RunAborted:
        case ProcessMessage::FaultEntered:
            return SafetyServiceStatus::Ready;
    }
    return raiseFault({FaultCode::Y4_008, sourceKey, correlationKey,
                       timeSource_.monotonicMillis(), std::nullopt});
}

SafetyServiceStatus SafetyFaultService::consumeSensorQuality(
    SafetySensorRole role,
    const device_platform::SensorQualitySnapshot& snapshot,
    std::uint32_t sourceKey, std::uint32_t correlationKey) {
    if (!started_) return SafetyServiceStatus::NotStarted;
    switch (snapshot.quality) {
        case device_platform::SensorQuality::Valid:
            return SafetyServiceStatus::Ready;
        case device_platform::SensorQuality::Stale: {
            const auto code = role == SafetySensorRole::Product
                                  ? FaultCode::O2_001
                                  : FaultCode::O2_002;
            return raiseFault({code, sourceKey, correlationKey,
                               timeSource_.monotonicMillis(), std::nullopt});
        }
        case device_platform::SensorQuality::Failed: {
            FaultCode code = FaultCode::Y4_008;
            if (role == SafetySensorRole::CabinetAir) code = FaultCode::S3_001;
            if (role == SafetySensorRole::Cooling) code = FaultCode::S3_002;
            if (role == SafetySensorRole::Product) code = FaultCode::O2_001;
            return raiseFault({code, sourceKey, correlationKey,
                               timeSource_.monotonicMillis(), std::nullopt});
        }
    }
    return raiseFault({FaultCode::Y4_008, sourceKey, correlationKey,
                       timeSource_.monotonicMillis(), std::nullopt});
}

SafetyServiceStatus SafetyFaultService::consumeWatchdogEvidence(
    const ActuatorWatchdogFaultEvidence& evidence) {
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
    switch (status) {
        case ConfigurationSafetyStatus::Operational:
            configurationGateQualified_ = true;
            return started_ ? SafetyServiceStatus::Ready
                            : SafetyServiceStatus::NotStarted;
        case ConfigurationSafetyStatus::ConfigurationRuntimeFailure:
            status = ConfigurationSafetyStatus::ConfigurationRuntimeFailure;
            break;
        case ConfigurationSafetyStatus::ConfigurationCommitIndeterminate:
            status =
                ConfigurationSafetyStatus::ConfigurationCommitIndeterminate;
            break;
        case ConfigurationSafetyStatus::ConfigurationUnavailable:
            status = ConfigurationSafetyStatus::ConfigurationUnavailable;
            break;
        case ConfigurationSafetyStatus::ConfigurationIntegrityFailure:
            status = ConfigurationSafetyStatus::ConfigurationIntegrityFailure;
            break;
        case ConfigurationSafetyStatus::Unknown:
            configurationGateQualified_ = false;
            return raiseFault({FaultCode::Y4_008, sourceKey, correlationKey,
                               timeSource_.monotonicMillis(), std::nullopt});
    }
    configurationGateQualified_ = false;
    FaultCode code = FaultCode::Y4_003;
    switch (status) {
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
        case ConfigurationSafetyStatus::Operational:
        case ConfigurationSafetyStatus::Unknown:
            break;
    }
    return raiseFault({code, sourceKey, correlationKey,
                       timeSource_.monotonicMillis(), std::nullopt});
}

SafetyServiceStatus SafetyFaultService::consumeConfigurationStatus(
    ConfigurationServiceMode status, std::uint32_t sourceKey,
    std::uint32_t correlationKey) {
    switch (status) {
        case ConfigurationServiceMode::Operational:
            return consumeConfigurationStatus(
                ConfigurationSafetyStatus::Operational, sourceKey,
                correlationKey);
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
                ConfigurationSafetyStatus::Unknown, sourceKey, correlationKey);
    }
    return consumeConfigurationStatus(ConfigurationSafetyStatus::Unknown,
                                      sourceKey, correlationKey);
}

SafetyServiceStatus SafetyFaultService::consumeConfigurationStatus(
    ConfigurationCommitStatus status, std::uint32_t sourceKey,
    std::uint32_t correlationKey) {
    switch (status) {
        case ConfigurationCommitStatus::Activated:
        case ConfigurationCommitStatus::NoChange:
            return consumeConfigurationStatus(
                ConfigurationSafetyStatus::Operational, sourceKey,
                correlationKey);
        case ConfigurationCommitStatus::ConfigurationCommitIndeterminate:
            return consumeConfigurationStatus(
                ConfigurationSafetyStatus::ConfigurationCommitIndeterminate,
                sourceKey, correlationKey);
        case ConfigurationCommitStatus::ConfigurationRuntimeFailure:
            return consumeConfigurationStatus(
                ConfigurationSafetyStatus::ConfigurationRuntimeFailure,
                sourceKey, correlationKey);
        case ConfigurationCommitStatus::PersistenceFailure:
        case ConfigurationCommitStatus::CapacityFailure:
            return consumeConfigurationStatus(
                ConfigurationSafetyStatus::ConfigurationUnavailable, sourceKey,
                correlationKey);
        default:
            return consumeConfigurationStatus(
                ConfigurationSafetyStatus::Unknown, sourceKey, correlationKey);
    }
}

SafetyServiceStatus SafetyFaultService::consumeConfigurationStatus(
    ConfigurationRecoveryStatus status, std::uint32_t sourceKey,
    std::uint32_t correlationKey) {
    switch (status) {
        case ConfigurationRecoveryStatus::RuntimeReady:
        case ConfigurationRecoveryStatus::FactoryInitializationCompleted:
        case ConfigurationRecoveryStatus::FactoryResetCompleted:
            return consumeConfigurationStatus(
                ConfigurationSafetyStatus::Operational, sourceKey,
                correlationKey);
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
        default:
            return consumeConfigurationStatus(
                ConfigurationSafetyStatus::Unknown, sourceKey, correlationKey);
    }
}

SafetyServiceStatus SafetyFaultService::consumeConfigurationRecoveryResult(
    const ConfigurationRecoveryResult& result, std::uint32_t sourceKey,
    std::uint32_t correlationKey) {
    return consumeConfigurationStatus(result.status, sourceKey, correlationKey);
}

SafetyServiceStatus SafetyFaultService::acknowledgeFault(
    FaultInstanceId id, std::uint32_t expectedRevision) {
    if (!started_) return SafetyServiceStatus::NotStarted;
    const auto before = faultCore_.snapshot();
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
    const auto before = faultCore_.snapshot();
    if (!faultCore_.markCauseCleared(id, expectedRevision)) {
        return SafetyServiceStatus::StaleFault;
    }
    const auto status = persistCoreMutation(before);
    recordEvent(FaultEventType::FaultCauseCleared, faultCore_.find(id),
                status == SafetyServiceStatus::Ready);
    return status;
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
    if (target->status != FaultStatus::CauseClearedLocked ||
        target->causeActive || hasOtherBlockingFault(faultCore_, id) ||
        (record_.capacityFailureLatched && target->code != FaultCode::Y4_006)) {
        result.status = SafetyServiceStatus::SafetyRejected;
        recordEvent(FaultEventType::FaultResetRejected, target, false);
        return result;
    }
    RunCommandState commandState;
    projectTo(commandState);
    FaultResetRequest request;
    request.envelope.id = 1U;
    request.envelope.monotonicMillis = timeSource_.monotonicMillis();
    request.envelope.expectedStateSequence =
        commandState.processState.transitionSequence;
    request.envelope.expectedFaultRevision = expectedRevision;
    request.envelope.confirmed = true;
    request.targetFault = id;
    if (!decideFaultReset(commandState, request).proposed()) {
        result.status = SafetyServiceStatus::SafetyRejected;
        recordEvent(FaultEventType::FaultResetRejected, target, false);
        return result;
    }
    const auto targetCode = target->code;
    FaultCore staged = faultCore_;
    if (!staged.clearAfterVerifiedReset(id, expectedRevision)) {
        result.status = SafetyServiceStatus::SafetyRejected;
        return result;
    }
    SafetyStateRecord candidate = record_;
    if (!copyCoreToRecord(candidate, staged) ||
        !increment(candidate.recordRevision) ||
        stateStore_.commit(candidate).status !=
            SafetyRecordCommitStatus::Committed) {
        record_.safeBootRequired = true;
        result.status = SafetyServiceStatus::PersistentWriteFailed;
        recordEvent(FaultEventType::FaultResetRejected, target, false);
        return result;
    }
    faultCore_ = staged;
    record_ = candidate;
    if (planner != nullptr && targetCode == FaultCode::S3_008) {
        planner->applyExternalWatchdogFaultReset(timeSource_.monotonicMillis());
    }
    result.status = SafetyServiceStatus::ResetCommitted;
    recordEvent(FaultEventType::FaultResetCommitted, faultCore_.find(id), true);
    return result;
}

SafetyServiceStatus SafetyFaultService::requestControlledSafetyRestart(
    FaultInstanceId id, std::uint32_t expectedRevision) {
    if (!started_) return SafetyServiceStatus::NotStarted;
    const auto* target = faultCore_.find(id);
    if (target == nullptr || !target->latched || !target->causeActive ||
        target->faultRevision != expectedRevision ||
        target->automaticRecoveryRestartUsed || record_.safeBootRequired) {
        return SafetyServiceStatus::SafetyRejected;
    }
    FaultCore staged = faultCore_;
    if (!staged.markControlledRestartUsed(id, expectedRevision)) {
        return SafetyServiceStatus::SafetyRejected;
    }
    SafetyStateRecord candidate = record_;
    if (!restartEpisode_.prepareControlledRestart(candidate, id,
                                                  staged.snapshot().revision) ||
        !copyCoreToRecord(candidate, staged) ||
        !increment(candidate.recordRevision) ||
        stateStore_.commit(candidate).status !=
            SafetyRecordCommitStatus::Committed) {
        record_.safeBootRequired = true;
        return SafetyServiceStatus::PersistentWriteFailed;
    }
    faultCore_ = staged;
    record_ = candidate;
    const auto restartResult = resetController_.requestRestart();
    if (restartResult != device_platform::RestartRequestResult::Accepted) {
        static_cast<void>(persistSafeBootLock());
        return restartResult == device_platform::RestartRequestResult::Rejected
                   ? SafetyServiceStatus::ResetBootRejected
                   : SafetyServiceStatus::ResetBootOutcomeUnknown;
    }
    return SafetyServiceStatus::Ready;
}

SafetyServiceStatus SafetyFaultService::advanceStableWindow(bool stable) {
    if (!started_) return SafetyServiceStatus::NotStarted;
    SafetyStateRecord candidate = record_;
    const auto beforeRunning = candidate.restartEpisode.stableWindowRunning;
    const auto beforeStart =
        candidate.restartEpisode.stableWindowStartedAtMillis;
    const bool closed = restartEpisode_.advanceStableWindow(
        candidate, timeSource_.monotonicMillis(), stable);
    if (!closed &&
        beforeRunning == candidate.restartEpisode.stableWindowRunning &&
        beforeStart == candidate.restartEpisode.stableWindowStartedAtMillis) {
        return SafetyServiceStatus::Ready;
    }
    if (!increment(candidate.recordRevision) ||
        stateStore_.commit(candidate).status !=
            SafetyRecordCommitStatus::Committed) {
        record_.safeBootRequired = true;
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
    if (!started_ || record_.safeBootRequired)
        return SafetyDisposition::ImmediateStop;
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
    if (!started_ || record_.safeBootRequired ||
        faultCore_.hasBlockingFault()) {
        return ActuatorSafetyGateInput{ActuatorSafetyGateStatus::ImmediateStop};
    }
    return ActuatorSafetyGateInput{ActuatorSafetyGateStatus::Allowed};
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
