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

bool addOne(std::uint32_t value, std::uint32_t& result) {
    if (value == std::numeric_limits<std::uint32_t>::max()) return false;
    result = value + 1U;
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
    if (loaded.status == SafetyRecordLoadStatus::NotFoundOutsideFactoryBootstrap) {
        return SafetyServiceStatus::FactoryBootstrapRequired;
    }
    if (loaded.status != SafetyRecordLoadStatus::Loaded &&
        loaded.status != SafetyRecordLoadStatus::FactoryInitialized) {
        return SafetyServiceStatus::PersistentReadFailed;
    }
    FaultCoreSnapshot snapshot;
    snapshot.count = loaded.record.latchCount;
    snapshot.revision = loaded.record.faultRevision;
    for (std::size_t index = 0U; index < snapshot.count; ++index) {
        snapshot.records[index] = loaded.record.latches[index];
    }
    snapshot.criticalSafetyEventPending = loaded.record.safeBootRequired;
    if (!faultCore_.restoreSnapshot(snapshot)) {
        return SafetyServiceStatus::PersistentReadFailed;
    }
    record_ = loaded.record;
    started_ = true;
    return SafetyServiceStatus::Ready;
}

SafetyBootResult SafetyFaultService::evaluateBoot() {
    SafetyBootResult result;
    if (!started_) {
        result.status = SafetyServiceStatus::NotStarted;
        return result;
    }
    result.restart = restartEpisode_.evaluateBoot(
        record_, resetController_.observeBootReset());
    FaultCode bootFault = FaultCode::Unknown;
    if (result.restart.status == RestartBootStatus::UnknownFailClosed) {
        bootFault = FaultCode::Y4_006;
    } else if (result.restart.status == RestartBootStatus::EvidenceMismatch ||
               result.restart.status == RestartBootStatus::Overflow) {
        bootFault = FaultCode::Y4_011;
    } else if (result.restart.status == RestartBootStatus::SafeBootRequired) {
        bootFault = FaultCode::Y4_007;
    }
    if (bootFault != FaultCode::Unknown) {
        const auto raised = faultCore_.raise(
            {bootFault, 24U, record_.restartEpisode.episodeId,
             timeSource_.monotonicMillis(), std::nullopt});
        const auto* fault = faultCore_.find(raised.instanceId);
        record_.safeBootRequired = true;
        static_cast<void>(copyCoreToRecord(record_));
        recordEvent(raised.status == FaultRaiseStatus::Created
                        ? FaultEventType::FaultCreated
                        : FaultEventType::FaultEscalated,
                    fault, true);
        result.restart.recordNeedsCommit = true;
    }
    if (result.restart.recordNeedsCommit) {
        if (!increment(record_.recordRevision)) {
            record_.safeBootRequired = true;
            result.status = SafetyServiceStatus::PersistentWriteFailed;
            result.safeBootRequired = true;
            return result;
        }
        const auto status = stateStore_.commit(record_).status;
        if (status != SafetyRecordCommitStatus::Committed) {
            record_.safeBootRequired = true;
            result.status = SafetyServiceStatus::PersistentWriteFailed;
            result.safeBootRequired = true;
            return result;
        }
    }
    result.safeBootRequired = record_.safeBootRequired ||
                             result.restart.safeBootRequired;
    result.status = result.safeBootRequired ? SafetyServiceStatus::SafetyRejected
                                             : SafetyServiceStatus::Ready;
    if (result.safeBootRequired) {
        recordEvent(FaultEventType::SafeBootEntered, faultCore_.dominant(), true);
    }
    return result;
}

bool SafetyFaultService::copyCoreToRecord(SafetyStateRecord& candidate) const {
    const auto snapshot = faultCore_.snapshot();
    if (snapshot.count > candidate.latches.size()) return false;
    candidate.faultRevision = snapshot.revision;
    candidate.latchCount = 0U;
    candidate.dominantCode = FaultCode::Unknown;
    std::uint32_t maximumId = candidate.faultInstanceSequence;
    for (std::size_t index = 0U; index < snapshot.count; ++index) {
        const auto& fault = snapshot.records[index];
        if (!isLatchedFaultClass(fault.faultClass) ||
            fault.status == FaultStatus::Cleared) {
            continue;
        }
        if (candidate.latchCount >= candidate.latches.size()) return false;
        candidate.latches[candidate.latchCount++] = fault;
        if (fault.instanceId.value > maximumId) maximumId = fault.instanceId.value;
    }
    candidate.faultInstanceSequence = maximumId;
    if (const auto* dominant = faultCore_.dominant(); dominant != nullptr) {
        candidate.dominantCode = dominant->code;
    }
    candidate.safeBootRequired =
        candidate.safeBootRequired ||
        (faultCore_.dominant() != nullptr &&
         faultCore_.dominant()->faultClass == FaultClass::LatchedSystemFault);
    return true;
}

SafetyServiceStatus SafetyFaultService::persistCoreMutation() {
    SafetyStateRecord candidate = record_;
    if (!copyCoreToRecord(candidate) || !increment(candidate.recordRevision)) {
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
    const auto raised = faultCore_.raise(request);
    if (raised.status == FaultRaiseStatus::CapacityReached) {
        record_.safeBootRequired = true;
        const auto persistStatus = persistCoreMutation();
        return persistStatus == SafetyServiceStatus::Ready
                   ? SafetyServiceStatus::FaultCapacityReached
                   : persistStatus;
    }
    if (raised.status == FaultRaiseStatus::InvalidInput) {
        return SafetyServiceStatus::InvalidFault;
    }
    if (raised.status == FaultRaiseStatus::RevisionOverflow) {
        record_.safeBootRequired = true;
        const auto persistStatus = persistCoreMutation();
        return persistStatus == SafetyServiceStatus::Ready
                   ? SafetyServiceStatus::PersistentWriteFailed
                   : persistStatus;
    }
    const auto* fault = faultCore_.find(raised.instanceId);
    const auto status = persistCoreMutation();
    recordEvent(raised.status == FaultRaiseStatus::Created
                    ? FaultEventType::FaultCreated
                    : FaultEventType::FaultEscalated,
                fault, status == SafetyServiceStatus::Ready);
    return status;
}

SafetyServiceStatus SafetyFaultService::consumeWatchdogEvidence(
    const ActuatorWatchdogFaultEvidence& evidence) {
    if (!started_) return SafetyServiceStatus::NotStarted;
    return raiseFault({FaultCode::S3_008, 23U,
                       static_cast<std::uint32_t>(
                           evidence.lastObservedSequenceHighWatermarkBeforeFault),
                       evidence.detectedAtMonotonicMillis, std::nullopt});
}

SafetyServiceStatus SafetyFaultService::consumeConfigurationStatus(
    ConfigurationSafetyStatus status, std::uint32_t sourceKey,
    std::uint32_t correlationKey) {
    FaultCode code = FaultCode::Unknown;
    switch (status) {
        case ConfigurationSafetyStatus::Operational:
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
    return raiseFault({code, sourceKey, correlationKey,
                       timeSource_.monotonicMillis(), std::nullopt});
}

SafetyServiceStatus SafetyFaultService::consumeConfigurationStatus(
    ConfigurationServiceMode status, std::uint32_t sourceKey,
    std::uint32_t correlationKey) {
    switch (status) {
        case ConfigurationServiceMode::Operational:
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
                ConfigurationSafetyStatus::ConfigurationUnavailable,
                sourceKey, correlationKey);
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
                ConfigurationSafetyStatus::ConfigurationUnavailable,
                sourceKey, correlationKey);
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
            return started_ ? SafetyServiceStatus::Ready
                            : SafetyServiceStatus::NotStarted;
        case ConfigurationRecoveryStatus::ConfigurationIntegrityFailure:
        case ConfigurationRecoveryStatus::UnsupportedNewerConfigurationSchema:
            return consumeConfigurationStatus(
                ConfigurationSafetyStatus::ConfigurationIntegrityFailure,
                sourceKey, correlationKey);
        case ConfigurationRecoveryStatus::ConfigurationCommitIndeterminate:
        case ConfigurationRecoveryStatus::BootstrapCommitIndeterminate:
        case ConfigurationRecoveryStatus::ConfigurationRecordOutcomeIndeterminate:
            return consumeConfigurationStatus(
                ConfigurationSafetyStatus::ConfigurationCommitIndeterminate,
                sourceKey, correlationKey);
        case ConfigurationRecoveryStatus::ConfigurationUnavailable:
        case ConfigurationRecoveryStatus::PersistenceReadFailure:
        case ConfigurationRecoveryStatus::PersistenceCapacityFailure:
        case ConfigurationRecoveryStatus::PersistenceWriteFailure:
        case ConfigurationRecoveryStatus::RuntimePreparationFailure:
            return consumeConfigurationStatus(
                ConfigurationSafetyStatus::ConfigurationUnavailable,
                sourceKey, correlationKey);
        case ConfigurationRecoveryStatus::ConfigurationMutationBusy:
        case ConfigurationRecoveryStatus::ConfigurationModelBudgetBusy:
        case ConfigurationRecoveryStatus::StateTransitionRejected:
        case ConfigurationRecoveryStatus::CounterOverflow:
            return consumeConfigurationStatus(ConfigurationSafetyStatus::Unknown,
                                              sourceKey, correlationKey);
    }
    return consumeConfigurationStatus(ConfigurationSafetyStatus::Unknown,
                                      sourceKey, correlationKey);
}

SafetyServiceStatus SafetyFaultService::acknowledgeFault(
    FaultInstanceId id, std::uint32_t expectedRevision) {
    if (!started_) return SafetyServiceStatus::NotStarted;
    if (!faultCore_.acknowledge(id, expectedRevision)) {
        return SafetyServiceStatus::StaleFault;
    }
    const auto status = persistCoreMutation();
    recordEvent(FaultEventType::FaultAcknowledged, faultCore_.find(id),
                status == SafetyServiceStatus::Ready);
    return status;
}

SafetyServiceStatus SafetyFaultService::clearFaultCause(
    FaultInstanceId id, std::uint32_t expectedRevision) {
    if (!started_) return SafetyServiceStatus::NotStarted;
    if (!faultCore_.markCauseCleared(id, expectedRevision)) {
        return SafetyServiceStatus::StaleFault;
    }
    const auto status = persistCoreMutation();
    recordEvent(FaultEventType::FaultCauseCleared, faultCore_.find(id),
                status == SafetyServiceStatus::Ready);
    return status;
}

SafetyResetResult SafetyFaultService::resetFault(
    FaultInstanceId id, std::uint32_t expectedRevision,
    bool authorizationSatisfied, ActuatorPlanner* planner) {
    SafetyResetResult result;
    result.targetFault = id;
    if (!started_ || !authorizationSatisfied) {
        result.status = started_ ? SafetyServiceStatus::SafetyRejected
                                 : SafetyServiceStatus::NotStarted;
        return result;
    }
    const auto* target = faultCore_.find(id);
    if (target == nullptr || target->causeActive || !target->latched ||
        target->faultRevision != expectedRevision) {
        result.status = SafetyServiceStatus::StaleFault;
        return result;
    }
    const FaultCode targetCode = target->code;
    if (targetCode == FaultCode::S3_008 && planner == nullptr) {
        result.status = SafetyServiceStatus::SafetyRejected;
        return result;
    }
    const auto snapshot = faultCore_.snapshot();
    for (std::size_t index = 0U; index < snapshot.count; ++index) {
        const auto& other = snapshot.records[index];
        if (other.instanceId != id && isBlockingFault(other)) {
            result.status = SafetyServiceStatus::SafetyRejected;
            return result;
        }
    }
    SafetyStateRecord candidate = record_;
    std::uint32_t nextFaultRevision = 0U;
    if (!addOne(expectedRevision, nextFaultRevision)) {
        result.status = SafetyServiceStatus::PersistentWriteFailed;
        return result;
    }
    bool persistedTargetFound = false;
    for (std::size_t index = 0U; index < candidate.latchCount; ++index) {
        if (candidate.latches[index].instanceId == id) {
            persistedTargetFound = true;
            candidate.latches[index].status = FaultStatus::Cleared;
            candidate.latches[index].causeActive = false;
            candidate.latches[index].faultRevision = nextFaultRevision;
        }
    }
    if (!persistedTargetFound) {
        result.status = SafetyServiceStatus::PersistentReadFailed;
        return result;
    }
    candidate.dominantCode = FaultCode::Unknown;
    const FaultRecord* persistedDominant = nullptr;
    for (std::size_t index = 0U; index < candidate.latchCount; ++index) {
        const auto& persisted = candidate.latches[index];
        if (persisted.status == FaultStatus::Cleared) continue;
        if (persistedDominant == nullptr ||
            static_cast<std::uint8_t>(persisted.faultClass) >
                static_cast<std::uint8_t>(persistedDominant->faultClass) ||
            (persisted.faultClass == persistedDominant->faultClass &&
             (faultCodePriority(persisted.code) <
                  faultCodePriority(persistedDominant->code) ||
              (faultCodePriority(persisted.code) ==
                   faultCodePriority(persistedDominant->code) &&
               persisted.creationSequence <
                   persistedDominant->creationSequence)))) {
            persistedDominant = &persisted;
        }
    }
    if (persistedDominant != nullptr) {
        candidate.dominantCode = persistedDominant->code;
    }
    candidate.faultRevision = nextFaultRevision;
    if (!restartEpisode_.prepareFaultResetBootIntent(candidate, id,
                                                     expectedRevision) ||
        !increment(candidate.recordRevision)) {
        result.status = SafetyServiceStatus::PersistentWriteFailed;
        return result;
    }
    const auto commit = stateStore_.commit(candidate);
    if (commit.status != SafetyRecordCommitStatus::Committed) {
        result.status = SafetyServiceStatus::PersistentWriteFailed;
        return result;
    }
    record_ = candidate;
    if (!faultCore_.clearAfterVerifiedReset(id, expectedRevision)) {
        result.status = SafetyServiceStatus::PersistentWriteFailed;
        static_cast<void>(persistSafeBootLock());
        return result;
    }
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
    if (resetResult == device_platform::ControlledRestartResult::OutcomeUnknown) {
        static_cast<void>(persistSafeBootLock());
        result.status = SafetyServiceStatus::ResetBootOutcomeUnknown;
        return result;
    }
    result.status = SafetyServiceStatus::ResetCommitted;
    recordEvent(FaultEventType::FaultResetCommitted, target, true);
    return result;
}

SafetyServiceStatus SafetyFaultService::requestControlledSafetyRestart(
    FaultInstanceId id, std::uint32_t expectedRevision) {
    if (!started_) return SafetyServiceStatus::NotStarted;
    const auto* target = faultCore_.find(id);
    if (target == nullptr || target->status == FaultStatus::Cleared ||
        !target->latched ||
        target->faultRevision != expectedRevision ||
        target->controlledRestartUsed || record_.safeBootRequired) {
        return SafetyServiceStatus::SafetyRejected;
    }
    std::uint32_t nextFaultRevision = 0U;
    if (!addOne(expectedRevision, nextFaultRevision)) {
        return SafetyServiceStatus::PersistentWriteFailed;
    }
    SafetyStateRecord candidate = record_;
    bool found = false;
    for (std::size_t index = 0U; index < candidate.latchCount; ++index) {
        auto& persisted = candidate.latches[index];
        if (persisted.instanceId == id) {
            persisted.controlledRestartUsed = true;
            persisted.faultRevision = nextFaultRevision;
            found = true;
            break;
        }
    }
    if (!found || !restartEpisode_.prepareControlledRestart(
                     candidate, id, nextFaultRevision) ||
        !increment(candidate.recordRevision)) {
        return SafetyServiceStatus::PersistentWriteFailed;
    }
    candidate.faultRevision = nextFaultRevision;
    const auto commit = stateStore_.commit(candidate);
    if (commit.status != SafetyRecordCommitStatus::Committed) {
        static_cast<void>(persistSafeBootLock());
        return SafetyServiceStatus::PersistentWriteFailed;
    }
    record_ = candidate;
    if (!faultCore_.markControlledRestartUsed(id, expectedRevision)) {
        static_cast<void>(persistSafeBootLock());
        return SafetyServiceStatus::PersistentWriteFailed;
    }
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
    const auto wasStarted = candidate.restartEpisode.stableWindowStartedAtMillis;
    const bool closed = restartEpisode_.advanceStableWindow(
        candidate, timeSource_.monotonicMillis(), stable);
    const bool changed = closed ||
                         wasRunning != candidate.restartEpisode.stableWindowRunning ||
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

ActuatorSafetyGateInput SafetyFaultService::actuatorGateInput(
    const std::optional<SafetyRecoveryRequest>& recovery) const {
    ActuatorSafetyGateInput result;
    if (!started_ || record_.safeBootRequired) {
        result.status = ActuatorSafetyGateStatus::ImmediateStop;
        return result;
    }
    if (recovery.has_value() && recovery->structurallyValid()) {
        const auto* target = faultCore_.find(recovery->targetFault);
        bool onlyRecoverableBlockingFault = target != nullptr &&
                                             target->code == FaultCode::S3_004 &&
                                             target->latched && target->causeActive &&
                                             target->faultRevision == recovery->faultRevision;
        const auto snapshot = faultCore_.snapshot();
        for (std::size_t index = 0U; index < snapshot.count; ++index) {
            if (snapshot.records[index].instanceId != recovery->targetFault &&
                isBlockingFault(snapshot.records[index])) {
                onlyRecoverableBlockingFault = false;
            }
        }
        if (onlyRecoverableBlockingFault) {
            result.status = ActuatorSafetyGateStatus::SafetyRecovery;
            result.safetyRecovery = recovery;
            return result;
        }
    }
    result.status = faultCore_.hasBlockingFault()
                        ? ActuatorSafetyGateStatus::ImmediateStop
                        : ActuatorSafetyGateStatus::Allowed;
    return result;
}

void SafetyFaultService::recordEvent(FaultEventType type,
                                     const FaultRecord* fault,
                                     bool accepted) const {
    FaultEventProjection event;
    event.type = type;
    event.accepted = accepted;
    if (fault != nullptr) {
        event.code = fault->code;
        event.faultInstanceId = fault->instanceId;
        event.primaryFaultId = fault->primaryFaultId;
        event.faultRevision = fault->faultRevision;
    }
    static_cast<void>(recordFaultEvent(journal_, timeSource_.monotonicMillis(),
                                       event));
}

}  // namespace fermentation
