#include "safety_fault_service.hpp"

#include <limits>

#include "actuator_planner.hpp"
#include "configuration_recovery_service.hpp"
#include "sensor_selection.hpp"

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

bool hasBlockingFaultExceptSafeBootTracking(const FaultCore& core) {
    const auto snapshot = core.snapshot();
    for (std::size_t index = 0U; index < snapshot.count; ++index) {
        const auto& fault = snapshot.records[index];
        if (fault.code != FaultCode::Y4_009 && isBlockingFault(fault)) {
            return true;
        }
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
    pendingAuthorizedSafeBootExitEvidenceId_.reset();
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
    const bool committedSoftwareEvidence =
        record_.restartEvidence.state == RestartEvidenceState::Committed &&
        record_.restartEvidence.cause == RestartCauseEvent::SoftwareRestart;
    result.restart = restartEpisode_.evaluateBoot(candidate, resetSnapshot);
    // RestartEpisodeCoordinator owns the neutral episode mechanics. The
    // application service owns the semantic binding to the current FaultCore
    // and record revision; a matching episode alone is not sufficient.
    if (committedSoftwareEvidence &&
        (result.restart.status == RestartBootStatus::AuthorizedReset ||
         result.restart.status ==
             RestartBootStatus::ControlledEvidenceConsumed) &&
        !restartEvidenceMatchesCurrentFault(record_.restartEvidence)) {
        candidate.safeBootRequired = true;
        result.restart.status = RestartBootStatus::EvidenceMismatch;
        result.restart.safeBootRequired = true;
    }
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
        result.restart.status == RestartBootStatus::AuthorizedReset &&
        candidate.restartEvidence.intent ==
            RestartIntentType::AuthorizedSafeBootExit &&
        safeBootBefore;
    if (authorizedExit) {
        const bool checksPass =
            !hasBlockingFaultExceptSafeBootTracking(stagedCore) &&
            configurationGateQualified_ && !candidate.capacityFailureLatched;
        if (!checksPass) {
            candidate.safeBootRequired = true;
            pendingAuthorizedSafeBootExitEvidenceId_ =
                candidate.restartEvidence.evidenceId;
        } else if (clearSafeBootTrackingFault(stagedCore)) {
            candidate.safeBootRequired = false;
            pendingAuthorizedSafeBootExitEvidenceId_.reset();
        } else {
            candidate.safeBootRequired = true;
            pendingAuthorizedSafeBootExitEvidenceId_ =
                candidate.restartEvidence.evidenceId;
        }
        needsCommit =
            needsCommit || candidate.safeBootRequired != safeBootBefore;
        result.restart.safeBootRequired = candidate.safeBootRequired;
    }

    if (!copyCoreToRecord(candidate, stagedCore)) {
        faultCore_ = stagedCore;
        retainRamFailClosed(0U, 0U);
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
            faultCore_ = stagedCore;
            retainRamFailClosed(0U, 0U);
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
    const FaultCoreSnapshot& before, std::uint32_t sourceKey,
    std::uint32_t correlationKey) {
    static_cast<void>(before);
    SafetyStateRecord candidate = record_;
    if (!copyCoreToRecord(candidate) || !increment(candidate.recordRevision)) {
        // The in-memory mutation is the diagnostic truth even when the write
        // failed. Retaining it together with the Y4-006 marker prevents a
        // later isolated write from looking like an automatic all-clear.
        retainRamFailClosed(sourceKey, correlationKey);
        return SafetyServiceStatus::PersistentWriteFailed;
    }
    const auto result = stateStore_.commit(candidate);
    if (result.status != SafetyRecordCommitStatus::Committed) {
        retainRamFailClosed(sourceKey, correlationKey);
        return SafetyServiceStatus::PersistentWriteFailed;
    }
    record_ = candidate;
    return SafetyServiceStatus::Ready;
}

void SafetyFaultService::retainRamFailClosed(std::uint32_t sourceKey,
                                             std::uint32_t correlationKey) {
    record_.safeBootRequired = true;
    record_.capacityFailureLatched = true;
    if (record_.capacityFailureRevision !=
        std::numeric_limits<std::uint32_t>::max()) {
        ++record_.capacityFailureRevision;
    }
    record_.capacityFailureSourceKey = sourceKey;
    record_.capacityFailureCorrelationKey = correlationKey;
    const auto snapshot = faultCore_.snapshot();
    record_.faultRevision = snapshot.revision;
    record_.faultInstanceSequence = snapshot.instanceSequenceHighWatermark;
    record_.latchCount = 0U;
    for (std::size_t index = 0U; index < snapshot.count; ++index) {
        const auto& fault = snapshot.records[index];
        if (!isLatchedFaultClass(fault.faultClass) ||
            fault.status == FaultStatus::Cleared ||
            record_.latchCount >= record_.latches.size()) {
            continue;
        }
        record_.latches[record_.latchCount++] = fault;
    }
    const auto* dominant = faultCore_.dominant();
    record_.dominantCode =
        dominant == nullptr ? FaultCode::Unknown : dominant->code;
}

bool SafetyFaultService::clearSafeBootTrackingFault(FaultCore& core) const {
    while (true) {
        const auto snapshot = core.snapshot();
        const FaultRecord* tracking = nullptr;
        for (std::size_t index = 0U; index < snapshot.count; ++index) {
            const auto& fault = snapshot.records[index];
            if (fault.code == FaultCode::Y4_009 &&
                fault.status != FaultStatus::Cleared) {
                tracking = &fault;
                break;
            }
        }
        if (tracking == nullptr) return true;
        if (!core.clearAfterAuthorizedSafeBootExit(tracking->instanceId,
                                                   tracking->faultRevision)) {
            return false;
        }
    }
}

SafetyServiceStatus SafetyFaultService::persistSafeBootLock() {
    SafetyStateRecord candidate = record_;
    candidate.safeBootRequired = true;
    if (!increment(candidate.recordRevision) ||
        stateStore_.commit(candidate).status !=
            SafetyRecordCommitStatus::Committed) {
        retainRamFailClosed(0U, 0U);
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
            retainRamFailClosed(request.sourceKey, request.correlationKey);
            return SafetyServiceStatus::PersistentWriteFailed;
        }
        candidate.capacityFailureSourceKey = request.sourceKey;
        candidate.capacityFailureCorrelationKey = request.correlationKey;
        if (!increment(candidate.recordRevision) ||
            stateStore_.commit(candidate).status !=
                SafetyRecordCommitStatus::Committed) {
            retainRamFailClosed(request.sourceKey, request.correlationKey);
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
        const auto status = persistCoreMutation(before, request.sourceKey,
                                                request.correlationKey);
        return status == SafetyServiceStatus::Ready
                   ? SafetyServiceStatus::FaultCapacityReached
                   : status;
    }
    if (raised.status == FaultRaiseStatus::InvalidInput) {
        return SafetyServiceStatus::InvalidFault;
    }
    const auto* fault = faultCore_.find(raised.instanceId);
    const auto status =
        persistCoreMutation(before, request.sourceKey, request.correlationKey);
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
        case ProcessMessage::FaultEntered:
            return SafetyServiceStatus::Ready;
        case ProcessMessage::RunCompleted:
        case ProcessMessage::RunAborted:
            // A later valid terminal process evaluation ends P1-001. The
            // process producer owns the signal; #24 owns only the
            // code-specific lifecycle projection.
            return resolveFaultCause(FaultCode::P1_001, sourceKey);
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
            // Product validity alone is not a #21 re-arm decision. Product
            // O2-001 is resolved only by consumeSensorSelectionEvidence().
            return role == SafetySensorRole::Product
                       ? SafetyServiceStatus::Ready
                       : resolveFaultCause(FaultCode::O2_002, sourceKey,
                                           correlationKey);
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

SafetyServiceStatus SafetyFaultService::consumeSensorSelectionEvidence(
    const SensorSelectionStateView& selection,
    const CrossRolePlausibilityContext& plausibility, std::uint32_t sourceKey,
    std::uint32_t correlationKey) {
    if (!started_) return SafetyServiceStatus::NotStarted;
    static_cast<void>(correlationKey);
    const bool airFallback =
        selection.runtime.phase == SensorSelectionPhase::AirFallbackActive &&
        selection.activeMode.has_value() &&
        *selection.activeMode == RunSensorMode::Air &&
        selection.runtime.permission == SensorPeltierPermission::Allowed &&
        plausibility.air.quality == device_platform::SensorQuality::Valid &&
        plausibility.cooling.quality == device_platform::SensorQuality::Valid;
    const bool returnedToProduct =
        (selection.runtime.phase == SensorSelectionPhase::NormalProduct ||
         selection.runtime.phase ==
             SensorSelectionPhase::ReturnValidationPending) &&
        selection.activeMode.has_value() &&
        *selection.activeMode == RunSensorMode::Product &&
        selection.runtime.permission == SensorPeltierPermission::Allowed &&
        plausibility.air.quality == device_platform::SensorQuality::Valid &&
        plausibility.product.quality == device_platform::SensorQuality::Valid &&
        plausibility.cooling.quality == device_platform::SensorQuality::Valid;
    if (!airFallback && !returnedToProduct) {
        return SafetyServiceStatus::SafetyRejected;
    }
    return resolveFaultCause(FaultCode::O2_001, sourceKey, correlationKey);
}

SafetyServiceStatus SafetyFaultService::resolveFaultCause(
    FaultCode code, std::uint32_t sourceKey,
    std::optional<std::uint32_t> correlationKey) {
    if (!started_) return SafetyServiceStatus::NotStarted;
    const auto before = faultCore_.snapshot();
    FaultCore staged = faultCore_;
    bool changed = false;
    for (std::size_t index = 0U; index < before.count; ++index) {
        const auto& fault = before.records[index];
        if (fault.code != code || fault.sourceKey != sourceKey ||
            (correlationKey.has_value() &&
             fault.correlationKey != *correlationKey) ||
            fault.status == FaultStatus::Cleared || !fault.causeActive) {
            continue;
        }
        changed =
            staged.markCauseCleared(fault.instanceId, fault.faultRevision) ||
            changed;
    }
    if (!changed) return SafetyServiceStatus::Ready;
    faultCore_ = staged;
    const auto status = persistCoreMutation(before, sourceKey, 0U);
    recordEvent(FaultEventType::FaultCauseCleared, faultCore_.dominant(),
                status == SafetyServiceStatus::Ready);
    return status;
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
            for (const auto code : {FaultCode::Y4_001, FaultCode::Y4_002,
                                    FaultCode::Y4_003, FaultCode::Y4_004}) {
                const auto resolved = resolveFaultCause(code, sourceKey);
                if (resolved != SafetyServiceStatus::Ready) {
                    configurationGateQualified_ = false;
                    return resolved;
                }
            }
            configurationGateQualified_ = true;
            return started_ ? finalizePendingSafeBootExit()
                            : SafetyServiceStatus::NotStarted;
        case ConfigurationSafetyStatus::ConfigurationRuntimeFailure:
            break;
        case ConfigurationSafetyStatus::ConfigurationCommitIndeterminate:
            break;
        case ConfigurationSafetyStatus::ConfigurationUnavailable:
            break;
        case ConfigurationSafetyStatus::ConfigurationIntegrityFailure:
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

FaultResetAuthorizationLevel SafetyFaultService::requiredAuthorizationFor(
    FaultCode code) {
    switch (code) {
        case FaultCode::O2_001:
        case FaultCode::O2_002:
            return FaultResetAuthorizationLevel::Operator;
        case FaultCode::S3_001:
        case FaultCode::S3_002:
        case FaultCode::S3_003:
        case FaultCode::S3_004:
        case FaultCode::S3_005:
        case FaultCode::S3_006:
        case FaultCode::S3_007:
            return FaultResetAuthorizationLevel::Service;
        case FaultCode::S3_008:
        case FaultCode::S3_009:
        case FaultCode::Y4_001:
        case FaultCode::Y4_002:
        case FaultCode::Y4_003:
        case FaultCode::Y4_004:
        case FaultCode::Y4_005:
        case FaultCode::Y4_006:
        case FaultCode::Y4_007:
        case FaultCode::Y4_008:
        case FaultCode::Y4_009:
            return FaultResetAuthorizationLevel::Technical;
        case FaultCode::P1_001:
        case FaultCode::Unknown:
            return FaultResetAuthorizationLevel::None;
    }
    return FaultResetAuthorizationLevel::None;
}

std::uint8_t SafetyFaultService::requiredResetCheckDomains(FaultCode code) {
    constexpr auto sensor =
        static_cast<std::uint8_t>(FaultResetCheckDomain::Sensor);
    constexpr auto actuator =
        static_cast<std::uint8_t>(FaultResetCheckDomain::Actuator);
    constexpr auto persistence =
        static_cast<std::uint8_t>(FaultResetCheckDomain::Persistence);
    constexpr auto integrity =
        static_cast<std::uint8_t>(FaultResetCheckDomain::Integrity);
    switch (code) {
        case FaultCode::S3_001:
            return sensor | integrity;
        case FaultCode::S3_002:
            return sensor | actuator | persistence | integrity;
        case FaultCode::S3_003:
        case FaultCode::S3_004:
        case FaultCode::S3_005:
        case FaultCode::S3_006:
        case FaultCode::S3_007:
            return sensor | actuator | persistence | integrity;
        case FaultCode::S3_008:
            return sensor | actuator | integrity;
        case FaultCode::S3_009:
            return actuator | integrity;
        case FaultCode::Y4_001:
        case FaultCode::Y4_002:
        case FaultCode::Y4_003:
        case FaultCode::Y4_004:
        case FaultCode::Y4_005:
        case FaultCode::Y4_007:
        case FaultCode::Y4_008:
        case FaultCode::Y4_009:
            return persistence | integrity;
        case FaultCode::P1_001:
        case FaultCode::O2_001:
        case FaultCode::O2_002:
        case FaultCode::Y4_006:
        case FaultCode::Unknown:
            return static_cast<std::uint8_t>(FaultResetCheckDomain::None);
    }
    return static_cast<std::uint8_t>(FaultResetCheckDomain::None);
}

bool SafetyFaultService::resetSafetyEvidenceMatches(
    const FaultResetSafetyEvidence& evidence, const FaultResetRequest& request,
    std::uint32_t targetRevision, std::uint8_t requiredDomains) const {
    if (evidence.targetFault != request.targetFault ||
        evidence.targetFaultRevision != targetRevision ||
        evidence.evidenceRevision != record_.recordRevision) {
        return false;
    }
    const auto passedAtCurrentRevision = [this](FaultResetCheckStatus status,
                                                std::uint32_t revision) {
        return status == FaultResetCheckStatus::Passed && revision != 0U &&
               revision == record_.recordRevision;
    };
    const auto hasDomain = [requiredDomains](FaultResetCheckDomain domain) {
        return (requiredDomains & static_cast<std::uint8_t>(domain)) != 0U;
    };
    return (!hasDomain(FaultResetCheckDomain::Sensor) ||
            passedAtCurrentRevision(evidence.sensor,
                                    evidence.sensorEvidenceRevision)) &&
           (!hasDomain(FaultResetCheckDomain::Actuator) ||
            passedAtCurrentRevision(evidence.actuator,
                                    evidence.actuatorEvidenceRevision)) &&
           (!hasDomain(FaultResetCheckDomain::Persistence) ||
            passedAtCurrentRevision(evidence.persistence,
                                    evidence.persistenceEvidenceRevision)) &&
           (!hasDomain(FaultResetCheckDomain::Integrity) ||
            passedAtCurrentRevision(evidence.integrity,
                                    evidence.integrityEvidenceRevision));
}

bool SafetyFaultService::authorizationIsCurrent(
    const FaultResetAuthorizationEvidence& authorization,
    FaultInstanceId targetFault, std::uint32_t targetRevision,
    FaultResetAuthorizationLevel required) const {
    const auto now = timeSource_.monotonicMillis();
    return authorization.evidenceId != 0U &&
           authorization.targetFault == targetFault &&
           authorization.targetFaultRevision == targetRevision &&
           static_cast<std::uint8_t>(authorization.level) >=
               static_cast<std::uint8_t>(required) &&
           authorization.issuedAtMonotonicMillis <= now &&
           now <= authorization.expiresAtMonotonicMillis;
}

bool SafetyFaultService::restartEvidenceMatchesCurrentFault(
    const PersistedRestartEvidence& evidence) const {
    if (evidence.state != RestartEvidenceState::Committed ||
        evidence.cause != RestartCauseEvent::SoftwareRestart ||
        evidence.evidenceId == 0U ||
        evidence.evidenceId != record_.restartEpisode.lastRestartEvidenceId ||
        evidence.episodeId == 0U ||
        evidence.episodeId != record_.restartEpisode.episodeId ||
        evidence.evidenceRevision == 0U ||
        evidence.evidenceRevision != record_.recordRevision) {
        return false;
    }
    switch (evidence.intent) {
        case RestartIntentType::AutomaticSafetyRecovery: {
            const auto* target = faultCore_.find(evidence.targetFault);
            return evidence.authorizationEvidenceId == 0U &&
                   target != nullptr && target->latched &&
                   target->causeActive &&
                   target->automaticRecoveryRestartUsed &&
                   allowsAutomaticRecoveryRestart(target->code) &&
                   target->faultRevision == evidence.targetFaultRevision;
        }
        case RestartIntentType::AuthorizedTechnicalRestart: {
            const auto* target = faultCore_.find(evidence.targetFault);
            return evidence.authorizationEvidenceId != 0U &&
                   target != nullptr && target->latched &&
                   target->causeActive &&
                   target->faultRevision == evidence.targetFaultRevision;
        }
        case RestartIntentType::AuthorizedSafeBootExit:
            return evidence.authorizationEvidenceId != 0U &&
                   !evidence.targetFault.valid() &&
                   evidence.targetFaultRevision == 0U;
        case RestartIntentType::None:
        case RestartIntentType::Unknown:
            return false;
    }
    return false;
}

FaultResetEvaluation SafetyFaultService::evaluateFaultReset(
    const FaultResetRequest& request,
    const FaultResetAuthorizationEvidence& authorization,
    const FaultResetSafetyEvidence& safetyEvidence) const {
    FaultResetEvaluation evaluation;
    const auto* target = faultCore_.find(request.targetFault);
    if (target == nullptr ||
        !request.envelope.expectedFaultRevision.has_value() ||
        target->faultRevision != *request.envelope.expectedFaultRevision) {
        evaluation.rejection = FaultResetRejection::StaleEvaluation;
        return evaluation;
    }
    evaluation.faultRevision = target->faultRevision;
    evaluation.causeStillActive = target->causeActive;
    evaluation.otherBlockingFaultActive =
        hasOtherBlockingFault(faultCore_, request.targetFault);
    evaluation.requiredAuthorization = requiredAuthorizationFor(target->code);
    evaluation.presentedAuthorization = authorization.level;
    evaluation.requiredCheckDomains = requiredResetCheckDomains(target->code);
    evaluation.codePolicyAllowsReset =
        target->latched && target->status == FaultStatus::CauseClearedLocked &&
        target->code != FaultCode::P1_001 && target->code != FaultCode::Y4_006;
    evaluation.safetyEvidenceTargetMatches =
        safetyEvidence.targetFault == request.targetFault &&
        safetyEvidence.targetFaultRevision == target->faultRevision;
    evaluation.safetyEvidenceCurrent =
        safetyEvidence.evidenceRevision == record_.recordRevision;
    evaluation.safetyEvidenceComplete = resetSafetyEvidenceMatches(
        safetyEvidence, request, target->faultRevision,
        evaluation.requiredCheckDomains);
    evaluation.safetyChecksPassed =
        evaluation.safetyEvidenceComplete && configurationGateQualified_;
    evaluation.authorizationSatisfied = authorizationIsCurrent(
        authorization, request.targetFault, target->faultRevision,
        evaluation.requiredAuthorization);

    RunCommandState commandState;
    projectTo(commandState);
    if (evaluation.causeStillActive) {
        evaluation.rejection = FaultResetRejection::CauseStillActive;
    } else if (evaluation.otherBlockingFaultActive) {
        evaluation.rejection = FaultResetRejection::OtherActiveFault;
    } else if (!evaluation.authorizationSatisfied) {
        evaluation.rejection = FaultResetRejection::AuthorizationMissing;
    } else if (!evaluation.codePolicyAllowsReset ||
               !evaluation.safetyChecksPassed) {
        evaluation.rejection = FaultResetRejection::SafetyChecksFailed;
    } else {
        const auto commandEvaluation = decideFaultReset(commandState, request);
        evaluation.allowed = commandEvaluation.proposed();
        evaluation.rejection = evaluation.allowed
                                   ? FaultResetRejection::None
                                   : FaultResetRejection::SafetyChecksFailed;
    }
    return evaluation;
}

SafetyResetResult SafetyFaultService::resetFault(
    const FaultResetRequest& request,
    const FaultResetAuthorizationEvidence& authorization,
    const FaultResetSafetyEvidence& safetyEvidence, ActuatorPlanner* planner) {
    SafetyResetResult result;
    result.targetFault = request.targetFault;
    if (!started_) {
        result.status = SafetyServiceStatus::NotStarted;
        return result;
    }
    const auto* target = faultCore_.find(request.targetFault);
    result.evaluation =
        evaluateFaultReset(request, authorization, safetyEvidence);
    if (!result.evaluation.allowed) {
        result.status =
            result.evaluation.rejection == FaultResetRejection::StaleEvaluation
                ? SafetyServiceStatus::StaleFault
                : SafetyServiceStatus::SafetyRejected;
        recordEvent(FaultEventType::FaultResetRejected, target, false);
        return result;
    }

    const auto targetCode = target->code;
    const auto targetRevision = target->faultRevision;
    FaultCore staged = faultCore_;
    if (!staged.clearAfterVerifiedReset(request.targetFault, targetRevision)) {
        result.status = SafetyServiceStatus::SafetyRejected;
        return result;
    }
    SafetyStateRecord candidate = record_;
    if (!copyCoreToRecord(candidate, staged) ||
        !increment(candidate.recordRevision) ||
        stateStore_.commit(candidate).status !=
            SafetyRecordCommitStatus::Committed) {
        retainRamFailClosed(request.envelope.id,
                            request.envelope.expectedStateSequence);
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
    recordEvent(FaultEventType::FaultResetCommitted, nullptr, true);
    return result;
}

SafetyServiceStatus SafetyFaultService::recoverSafetyStateMarker(
    const FaultResetAuthorizationEvidence& authorization,
    const SafetyMarkerRecoveryEvidence& evidence) {
    if (!started_) return SafetyServiceStatus::NotStarted;
    const auto now = timeSource_.monotonicMillis();
    const bool authorizationValid =
        authorization.evidenceId != 0U && !authorization.targetFault.valid() &&
        authorization.targetFaultRevision == 0U &&
        static_cast<std::uint8_t>(authorization.level) >=
            static_cast<std::uint8_t>(
                FaultResetAuthorizationLevel::Technical) &&
        authorization.issuedAtMonotonicMillis <= now &&
        now <= authorization.expiresAtMonotonicMillis;
    const bool evidenceCurrent =
        evidence.markerRevision == record_.capacityFailureRevision &&
        evidence.evidenceRevision == record_.recordRevision &&
        evidence.allPassed() &&
        evidence.readEvidenceRevision == record_.recordRevision &&
        evidence.writeEvidenceRevision == record_.recordRevision &&
        evidence.capacityEvidenceRevision == record_.recordRevision &&
        evidence.integrityEvidenceRevision == record_.recordRevision;
    if (!record_.capacityFailureLatched || !authorizationValid ||
        !evidenceCurrent || !configurationGateQualified_ ||
        faultCore_.hasBlockingFault()) {
        return SafetyServiceStatus::SafetyRejected;
    }

    // Read and validate the currently committed record before preparing the
    // recovery candidate. The candidate is not applied in RAM until the
    // single normal SafetyStateStore write and its readback are confirmed.
    const auto current = stateStore_.load();
    if (current.status != SafetyRecordLoadStatus::Loaded &&
        current.status != SafetyRecordLoadStatus::FactoryInitialized) {
        retainRamFailClosed(0U, authorization.evidenceId);
        return SafetyServiceStatus::PersistentReadFailed;
    }

    SafetyStateRecord candidate = record_;
    candidate.capacityFailureLatched = false;
    candidate.capacityFailureRevision = 0U;
    candidate.capacityFailureSourceKey = 0U;
    candidate.capacityFailureCorrelationKey = 0U;
    candidate.safeBootRequired = false;
    if (!copyCoreToRecord(candidate) || !increment(candidate.recordRevision) ||
        stateStore_.commit(candidate).status !=
            SafetyRecordCommitStatus::Committed) {
        retainRamFailClosed(0U, authorization.evidenceId);
        return SafetyServiceStatus::PersistentWriteFailed;
    }

    // SafetyStateStore::commit performs the single write and exact readback
    // comparison, including CommitOutcomeUnknown cut-points. Applying the
    // candidate only after Committed avoids a second persistence path.
    record_ = candidate;
    return SafetyServiceStatus::SafetyMarkerRecoveryCommitted;
}

SafetyServiceStatus SafetyFaultService::finalizeRestartRequestResult(
    device_platform::RestartRequestResult result) {
    if (result == device_platform::RestartRequestResult::Accepted) {
        return SafetyServiceStatus::Ready;
    }

    SafetyStateRecord terminal = record_;
    if (result == device_platform::RestartRequestResult::OutcomeUnknown) {
        // The request outcome is no longer retryable. Retaining the evidence
        // as terminally consumed prevents a later unrelated SoftwareRestart
        // from inheriting the old intent; SAFE_BOOT remains fail-closed.
        terminal.restartEvidence.state = RestartEvidenceState::Consumed;
        terminal.safeBootRequired = true;
    } else {
        // A definite rejection must not leave a committed intent that could
        // authorize a later independent SoftwareRestart.
        terminal.restartEvidence = PersistedRestartEvidence{};
    }
    if (!increment(terminal.recordRevision) ||
        stateStore_.commit(terminal).status !=
            SafetyRecordCommitStatus::Committed) {
        retainRamFailClosed(0U, terminal.restartEvidence.evidenceId);
        return SafetyServiceStatus::PersistentWriteFailed;
    }
    record_ = terminal;
    return result == device_platform::RestartRequestResult::Rejected
               ? SafetyServiceStatus::ResetBootRejected
               : SafetyServiceStatus::ResetBootOutcomeUnknown;
}

SafetyServiceStatus SafetyFaultService::requestControlledSafetyRestart(
    FaultInstanceId id, std::uint32_t expectedRevision) {
    if (!started_) return SafetyServiceStatus::NotStarted;
    const auto* target = faultCore_.find(id);
    if (target == nullptr || !target->latched || !target->causeActive ||
        target->faultRevision != expectedRevision ||
        target->automaticRecoveryRestartUsed || record_.safeBootRequired ||
        !allowsAutomaticRecoveryRestart(target->code)) {
        return SafetyServiceStatus::SafetyRejected;
    }
    FaultCore staged = faultCore_;
    if (!staged.markControlledRestartUsed(id, expectedRevision)) {
        return SafetyServiceStatus::SafetyRejected;
    }
    const auto* stagedTarget = staged.find(id);
    if (stagedTarget == nullptr) return SafetyServiceStatus::SafetyRejected;
    SafetyStateRecord candidate = record_;
    if (!restartEpisode_.prepareControlledRestart(
            candidate, id, stagedTarget->faultRevision) ||
        !copyCoreToRecord(candidate, staged) ||
        !increment(candidate.recordRevision) ||
        stateStore_.commit(candidate).status !=
            SafetyRecordCommitStatus::Committed) {
        retainRamFailClosed(23U, id.value);
        return SafetyServiceStatus::PersistentWriteFailed;
    }
    faultCore_ = staged;
    record_ = candidate;
    return finalizeRestartRequestResult(resetController_.requestRestart());
}

SafetyServiceStatus SafetyFaultService::finalizePendingSafeBootExit() {
    if (!pendingAuthorizedSafeBootExitEvidenceId_.has_value()) {
        return SafetyServiceStatus::Ready;
    }
    if (!configurationGateQualified_ ||
        hasBlockingFaultExceptSafeBootTracking(faultCore_) ||
        record_.capacityFailureLatched) {
        return SafetyServiceStatus::SafetyRejected;
    }
    SafetyStateRecord candidate = record_;
    FaultCore staged = faultCore_;
    if (!clearSafeBootTrackingFault(staged)) {
        return SafetyServiceStatus::SafetyRejected;
    }
    candidate.safeBootRequired = false;
    if (!copyCoreToRecord(candidate, staged) ||
        !increment(candidate.recordRevision) ||
        stateStore_.commit(candidate).status !=
            SafetyRecordCommitStatus::Committed) {
        retainRamFailClosed(0U, *pendingAuthorizedSafeBootExitEvidenceId_);
        return SafetyServiceStatus::PersistentWriteFailed;
    }
    faultCore_ = staged;
    record_ = candidate;
    const auto evidenceId = *pendingAuthorizedSafeBootExitEvidenceId_;
    pendingAuthorizedSafeBootExitEvidenceId_.reset();
    recordEvent(FaultEventType::SafeBootExitDecided, faultCore_.dominant(),
                true, record_.restartEpisode.episodeId, evidenceId);
    return SafetyServiceStatus::Ready;
}

SafetyServiceStatus SafetyFaultService::requestAuthorizedSafeBootExit(
    const FaultResetAuthorizationEvidence& authorization) {
    if (!started_) return SafetyServiceStatus::NotStarted;
    const auto now = timeSource_.monotonicMillis();
    if (!record_.safeBootRequired || authorization.evidenceId == 0U ||
        authorization.targetFault.valid() ||
        authorization.targetFaultRevision != 0U ||
        static_cast<std::uint8_t>(authorization.level) <
            static_cast<std::uint8_t>(
                FaultResetAuthorizationLevel::Technical) ||
        authorization.issuedAtMonotonicMillis > now ||
        now > authorization.expiresAtMonotonicMillis) {
        return SafetyServiceStatus::SafetyRejected;
    }
    SafetyStateRecord candidate = record_;
    if (!restartEpisode_.prepareRestartIntent(
            candidate, RestartIntentType::AuthorizedSafeBootExit, {},
            candidate.faultRevision)) {
        return SafetyServiceStatus::SafetyRejected;
    }
    candidate.restartEvidence.authorizationEvidenceId =
        authorization.evidenceId;
    if (!increment(candidate.recordRevision) ||
        stateStore_.commit(candidate).status !=
            SafetyRecordCommitStatus::Committed) {
        retainRamFailClosed(0U, authorization.evidenceId);
        return SafetyServiceStatus::PersistentWriteFailed;
    }
    record_ = candidate;
    return finalizeRestartRequestResult(resetController_.requestRestart());
}

SafetyServiceStatus SafetyFaultService::requestAuthorizedTechnicalRestart(
    const FaultResetAuthorizationEvidence& authorization,
    FaultInstanceId targetFault, std::uint32_t targetFaultRevision) {
    if (!started_ || !targetFault.valid() ||
        static_cast<std::uint8_t>(authorization.level) <
            static_cast<std::uint8_t>(
                FaultResetAuthorizationLevel::Technical) ||
        !authorizationIsCurrent(authorization, targetFault, targetFaultRevision,
                                FaultResetAuthorizationLevel::Technical)) {
        return SafetyServiceStatus::SafetyRejected;
    }
    const auto* target = faultCore_.find(targetFault);
    if (target == nullptr || !target->latched || !target->causeActive ||
        target->faultRevision != targetFaultRevision) {
        return SafetyServiceStatus::SafetyRejected;
    }
    SafetyStateRecord candidate = record_;
    if (!restartEpisode_.prepareRestartIntent(
            candidate, RestartIntentType::AuthorizedTechnicalRestart,
            targetFault, targetFaultRevision)) {
        return SafetyServiceStatus::SafetyRejected;
    }
    candidate.restartEvidence.authorizationEvidenceId =
        authorization.evidenceId;
    if (!increment(candidate.recordRevision) ||
        stateStore_.commit(candidate).status !=
            SafetyRecordCommitStatus::Committed) {
        retainRamFailClosed(0U, authorization.evidenceId);
        return SafetyServiceStatus::PersistentWriteFailed;
    }
    record_ = candidate;
    return finalizeRestartRequestResult(resetController_.requestRestart());
}

SafetyServiceStatus SafetyFaultService::advanceStableWindow() {
    if (!started_) return SafetyServiceStatus::NotStarted;
    SafetyStateRecord candidate = record_;
    const auto beforeRunning = candidate.restartEpisode.stableWindowRunning;
    const auto beforeStart =
        candidate.restartEpisode.stableWindowStartedAtMillis;
    const bool stable =
        !record_.safeBootRequired && !faultCore_.hasBlockingFault() &&
        configurationGateQualified_ && !record_.capacityFailureLatched;
    bool closed = false;
    if (stable) {
        closed = restartEpisode_.advanceStableWindow(
            candidate, timeSource_.monotonicMillis());
    } else {
        candidate.restartEpisode.stableWindowRunning = false;
        candidate.restartEpisode.stableWindowStartedAtMillis = 0U;
    }
    if (!closed &&
        beforeRunning == candidate.restartEpisode.stableWindowRunning &&
        beforeStart == candidate.restartEpisode.stableWindowStartedAtMillis) {
        return SafetyServiceStatus::Ready;
    }
    if (!increment(candidate.recordRevision) ||
        stateStore_.commit(candidate).status !=
            SafetyRecordCommitStatus::Committed) {
        retainRamFailClosed(0U, 0U);
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
    if (!started_ || record_.safeBootRequired || !configurationGateQualified_ ||
        record_.capacityFailureLatched)
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
    if (record_.safeBootRequired || !configurationGateQualified_ ||
        record_.capacityFailureLatched) {
        state.criticalSafetyEventPending = true;
    }
}

ActuatorSafetyGateInput SafetyFaultService::actuatorGateInput() const {
    if (!started_ || record_.safeBootRequired || !configurationGateQualified_ ||
        record_.capacityFailureLatched || faultCore_.hasBlockingFault()) {
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
