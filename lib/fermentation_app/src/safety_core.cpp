#include "safety_core.hpp"

#include "actuator_planner.hpp"

namespace fermentation {
namespace {

bool isConfigurationUnavailable(ConfigurationRecoveryStatus status) {
    switch (status) {
        case ConfigurationRecoveryStatus::ConfigurationUnavailable:
        case ConfigurationRecoveryStatus::PersistenceReadFailure:
        case ConfigurationRecoveryStatus::PersistenceCapacityFailure:
        case ConfigurationRecoveryStatus::PersistenceWriteFailure:
        case ConfigurationRecoveryStatus::RuntimePreparationFailure:
        case ConfigurationRecoveryStatus::ConfigurationModelBudgetBusy:
        case ConfigurationRecoveryStatus::StateTransitionRejected:
        case ConfigurationRecoveryStatus::CounterOverflow:
            return true;
        default:
            return false;
    }
}

bool isConfigurationIntegrityFailure(ConfigurationRecoveryStatus status) {
    return status ==
               ConfigurationRecoveryStatus::ConfigurationIntegrityFailure ||
           status ==
               ConfigurationRecoveryStatus::UnsupportedNewerConfigurationSchema;
}

bool isConfigurationCommitIndeterminate(ConfigurationRecoveryStatus status) {
    return status ==
               ConfigurationRecoveryStatus::ConfigurationCommitIndeterminate ||
           status ==
               ConfigurationRecoveryStatus::BootstrapCommitIndeterminate ||
           status == ConfigurationRecoveryStatus::
                         ConfigurationRecordOutcomeIndeterminate;
}

bool isPersistenceSafeBoot(RunPersistenceLoadStatus status) {
    switch (status) {
        case RunPersistenceLoadStatus::NoPersistedRun:
        case RunPersistenceLoadStatus::Current:
        case RunPersistenceLoadStatus::NoActiveRun:
            return false;
        case RunPersistenceLoadStatus::FallbackRecovered:
        case RunPersistenceLoadStatus::PreparedInterrupted:
        case RunPersistenceLoadStatus::NotReconstructible:
        case RunPersistenceLoadStatus::NotReconstructibleOrphanedState:
        case RunPersistenceLoadStatus::ReadFailed:
        case RunPersistenceLoadStatus::CapacityExceeded:
        case RunPersistenceLoadStatus::UnsupportedSchema:
        case RunPersistenceLoadStatus::ForeignEpoch:
        case RunPersistenceLoadStatus::AlreadyInitialized:
            return true;
    }
    return true;
}

}  // namespace

void SafetyCore::beginBoot(device_platform::ResetCause resetCause) noexcept {
    resetCause_ = resetCause;
    activeFault_ = FaultCode::None;
    acknowledged_ = false;
    lastEvaluation_ = SafetyEvaluation{};
    lastEvaluation_.resetCause = resetCause;
}

SafetyEvaluation SafetyCore::evaluate(const SafetyCoreInput& input) {
    SafetyEvaluation result;
    result.resetCause = resetCause_;
    result.gate.status = ActuatorSafetyGateStatus::Unresolved;
    result.bootDisposition = SafetyBootDisposition::Unresolved;
    const auto finalize = [this](SafetyEvaluation evaluation) {
        lastEvaluation_ = evaluation;
        return evaluation;
    };

    const auto setResultFault = [&](FaultCode code,
                                    SafetyDisposition disposition) {
        setFault(code);
        result.faultCode = code;
        result.acknowledged = acknowledged_;
        result.disposition = disposition;
        result.bootDisposition = disposition == SafetyDisposition::SafeBoot
                                     ? SafetyBootDisposition::SafeBoot
                                     : SafetyBootDisposition::Unresolved;
    };

    if ((input.configurationServiceMode.has_value() &&
         !isKnown(*input.configurationServiceMode)) ||
        (input.configurationCommitStatus.has_value() &&
         !isKnown(*input.configurationCommitStatus)) ||
        (input.configurationRecoveryStatus.has_value() &&
         !isKnown(*input.configurationRecoveryStatus)) ||
        (input.configurationProducer.has_value() &&
         !isKnown(*input.configurationProducer)) ||
        (input.persistenceLoadStatus.has_value() &&
         !isKnown(*input.persistenceLoadStatus)) ||
        !isKnown(input.persistenceCoordinatorState)) {
        setResultFault(FaultCode::SystemProducerUnknown,
                       SafetyDisposition::SafeBoot);
        return finalize(result);
    }

    if (input.configurationProducer.has_value()) {
        switch (*input.configurationProducer) {
            case ConfigurationSafetyProducer::ConfigurationUnavailable:
                setResultFault(FaultCode::ConfigurationUnavailable,
                               SafetyDisposition::SafeBoot);
                return finalize(result);
            case ConfigurationSafetyProducer::ConfigurationIntegrityFailure:
                setResultFault(FaultCode::ConfigurationIntegrityFailure,
                               SafetyDisposition::SafeBoot);
                return finalize(result);
        }
    }
    if (input.configurationRecoveryStatus.has_value()) {
        const auto status = *input.configurationRecoveryStatus;
        if (isConfigurationCommitIndeterminate(status)) {
            setResultFault(FaultCode::ConfigurationCommitIndeterminate,
                           SafetyDisposition::SafeBoot);
            return finalize(result);
        }
        if (isConfigurationIntegrityFailure(status)) {
            setResultFault(FaultCode::ConfigurationIntegrityFailure,
                           SafetyDisposition::SafeBoot);
            return finalize(result);
        }
        if (isConfigurationUnavailable(status)) {
            setResultFault(FaultCode::ConfigurationUnavailable,
                           SafetyDisposition::SafeBoot);
            return finalize(result);
        }
    }
    if (input.configurationServiceMode.has_value()) {
        switch (*input.configurationServiceMode) {
            case ConfigurationServiceMode::CommitIndeterminate:
            case ConfigurationServiceMode::BootstrapFinalizationPending:
                setResultFault(FaultCode::ConfigurationCommitIndeterminate,
                               SafetyDisposition::SafeBoot);
                return finalize(result);
            case ConfigurationServiceMode::RuntimeFailure:
                setResultFault(FaultCode::ConfigurationRuntimeFailure,
                               SafetyDisposition::BlockedImmediateStop);
                return finalize(result);
            default:
                break;
        }
    }
    if (input.configurationCommitStatus.has_value()) {
        switch (*input.configurationCommitStatus) {
            case ConfigurationCommitStatus::ConfigurationCommitIndeterminate:
                setResultFault(FaultCode::ConfigurationCommitIndeterminate,
                               SafetyDisposition::SafeBoot);
                return finalize(result);
            case ConfigurationCommitStatus::ConfigurationRuntimeFailure:
                setResultFault(FaultCode::ConfigurationRuntimeFailure,
                               SafetyDisposition::BlockedImmediateStop);
                return finalize(result);
            case ConfigurationCommitStatus::Activated:
            case ConfigurationCommitStatus::NoChange:
                break;
            default:
                setResultFault(FaultCode::ConfigurationUnavailable,
                               SafetyDisposition::SafeBoot);
                return finalize(result);
        }
    }

    if (!input.configurationValidated) {
        setResultFault(FaultCode::SystemProducerUnknown,
                       SafetyDisposition::SafeBoot);
        return finalize(result);
    }

    if (input.persistenceLoadStatus.has_value() &&
        isPersistenceSafeBoot(*input.persistenceLoadStatus)) {
        setResultFault(FaultCode::RunPersistenceUntrusted,
                       SafetyDisposition::SafeBoot);
        return finalize(result);
    }
    switch (input.persistenceCoordinatorState) {
        case RunPersistenceCoordinatorState::BlockedIndeterminate:
        case RunPersistenceCoordinatorState::FallbackRecoveryPending:
        case RunPersistenceCoordinatorState::PersistenceCommittedApplyFailed:
        case RunPersistenceCoordinatorState::Busy:
        case RunPersistenceCoordinatorState::Uninitialized:
            setResultFault(FaultCode::RunPersistenceUntrusted,
                           SafetyDisposition::SafeBoot);
            return finalize(result);
        default:
            break;
    }
    if (!input.persistenceValidated) {
        setResultFault(FaultCode::RunPersistenceUntrusted,
                       SafetyDisposition::SafeBoot);
        return finalize(result);
    }

    if (input.explicitActivationRequested &&
        (!input.sensorEvidenceValidated || input.peltierSensor == nullptr ||
         input.peltierSensor->quality !=
             device_platform::SensorQuality::Valid ||
         input.sensorSelectionRuntime == nullptr ||
         input.sensorSelectionRuntime->permission !=
             SensorPeltierPermission::Allowed)) {
        setResultFault(FaultCode::SafetySensorUnavailable,
                       SafetyDisposition::BlockedImmediateStop);
        return finalize(result);
    }

    if (input.requestWatchdogTripped) {
        setResultFault(FaultCode::ActuatorRequestWatchdog,
                       SafetyDisposition::BlockedImmediateStop);
        return finalize(result);
    }

    clearFault();
    result.faultCode = FaultCode::None;
    result.acknowledged = false;
    result.disposition = SafetyDisposition::Information;
    if (!input.bootValidationComplete || !input.explicitActivationRequested ||
        !input.plannerEvidenceValidated) {
        result.bootDisposition =
            input.persistenceLoadStatus.has_value() &&
                    (*input.persistenceLoadStatus ==
                         RunPersistenceLoadStatus::NoPersistedRun ||
                     *input.persistenceLoadStatus ==
                         RunPersistenceLoadStatus::NoActiveRun)
                ? SafetyBootDisposition::Standby
                : SafetyBootDisposition::Unresolved;
        return finalize(result);
    }

    result.gate.status = ActuatorSafetyGateStatus::Allowed;
    result.bootDisposition = SafetyBootDisposition::ResumeOffer;
    return finalize(result);
}

void SafetyCore::acknowledge(FaultCode code) noexcept {
    if (code != FaultCode::None && code == activeFault_) acknowledged_ = true;
}

bool SafetyCore::resetRequestWatchdog(ActuatorPlanner& planner,
                                      std::uint64_t nowMonotonicMillis,
                                      bool freshSafetyEvidence) {
    if (!freshSafetyEvidence ||
        activeFault_ != FaultCode::ActuatorRequestWatchdog)
        return false;
    planner.applyExternalWatchdogFaultReset(nowMonotonicMillis);
    clearFault();
    acknowledged_ = false;
    return true;
}

bool SafetyCore::isR1ResumeEligible(
    const RunPersistenceSnapshot& snapshot) noexcept {
    if (snapshot.variant == RunCheckpointVariant::NoActiveRun ||
        snapshot.processState.state == ProcessState::Completed ||
        snapshot.processState.state == ProcessState::Fault ||
        snapshot.processState.state == ProcessState::RecoveryEvaluation ||
        snapshot.pendingRecoveryAnchor.has_value() ||
        snapshot.recoveryBootAnchorMonotonicMillis.has_value() ||
        snapshot.lastRecoveryEpisodeEvidence.has_value() ||
        snapshot.priorBootPhaseElapsed.has_value() ||
        snapshot.nominalRecoveryAdjustment.has_value() ||
        snapshot.runProgress.weightedProgress.has_value() ||
        snapshot.runProgress.basis == RunProgressBasis::PartialUnknownHistory) {
        return false;
    }
    switch (snapshot.processState.state) {
        case ProcessState::Preheating:
        case ProcessState::Cooling:
        case ProcessState::ManualHolding:
            return true;
        case ProcessState::WaitingForProduct:
        case ProcessState::ReachingTarget:
        case ProcessState::QualifyingTarget:
        case ProcessState::Fermenting:
        case ProcessState::CoolHolding:
        case ProcessState::Boot:
        case ProcessState::SafeBoot:
        case ProcessState::Standby:
        case ProcessState::Completed:
        case ProcessState::RecoveryEvaluation:
        case ProcessState::Fault:
        case ProcessState::ServiceMode:
            return false;
    }
    return false;
}

RunLoadDisposition SafetyCore::classifyRunLoad(
    RunPersistenceLoadStatus status,
    const RunPersistenceSnapshot* snapshot) noexcept {
    switch (status) {
        case RunPersistenceLoadStatus::NoPersistedRun:
        case RunPersistenceLoadStatus::NoActiveRun:
            return RunLoadDisposition::Standby;
        case RunPersistenceLoadStatus::Current:
            if (snapshot == nullptr) return RunLoadDisposition::SafeBoot;
            if (snapshot->processState.state == ProcessState::Completed)
                return RunLoadDisposition::Completed;
            if (snapshot->processState.state == ProcessState::Fault)
                return RunLoadDisposition::TerminalFault;
            return isR1ResumeEligible(*snapshot)
                       ? RunLoadDisposition::ResumeOffer
                       : RunLoadDisposition::NoActiveRun;
        case RunPersistenceLoadStatus::FallbackRecovered:
        case RunPersistenceLoadStatus::PreparedInterrupted:
        case RunPersistenceLoadStatus::NotReconstructible:
        case RunPersistenceLoadStatus::NotReconstructibleOrphanedState:
        case RunPersistenceLoadStatus::ReadFailed:
        case RunPersistenceLoadStatus::CapacityExceeded:
        case RunPersistenceLoadStatus::UnsupportedSchema:
        case RunPersistenceLoadStatus::ForeignEpoch:
        case RunPersistenceLoadStatus::AlreadyInitialized:
            return RunLoadDisposition::SafeBoot;
    }
    return RunLoadDisposition::SafeBoot;
}

bool SafetyCore::isKnown(ConfigurationRecoveryStatus status) noexcept {
    switch (status) {
        case ConfigurationRecoveryStatus::RuntimeReady:
        case ConfigurationRecoveryStatus::FactoryInitializationCompleted:
        case ConfigurationRecoveryStatus::FactoryResetCompleted:
        case ConfigurationRecoveryStatus::ConfigurationMutationBusy:
        case ConfigurationRecoveryStatus::ConfigurationModelBudgetBusy:
        case ConfigurationRecoveryStatus::StateTransitionRejected:
        case ConfigurationRecoveryStatus::ConfigurationUnavailable:
        case ConfigurationRecoveryStatus::ConfigurationIntegrityFailure:
        case ConfigurationRecoveryStatus::UnsupportedNewerConfigurationSchema:
        case ConfigurationRecoveryStatus::PersistenceReadFailure:
        case ConfigurationRecoveryStatus::PersistenceCapacityFailure:
        case ConfigurationRecoveryStatus::PersistenceWriteFailure:
        case ConfigurationRecoveryStatus::CounterOverflow:
        case ConfigurationRecoveryStatus::RuntimePreparationFailure:
        case ConfigurationRecoveryStatus::BootstrapCommitIndeterminate:
        case ConfigurationRecoveryStatus::
            ConfigurationRecordOutcomeIndeterminate:
        case ConfigurationRecoveryStatus::ConfigurationCommitIndeterminate:
            return true;
    }
    return false;
}

bool SafetyCore::isKnown(ConfigurationSafetyProducer producer) noexcept {
    switch (producer) {
        case ConfigurationSafetyProducer::ConfigurationUnavailable:
        case ConfigurationSafetyProducer::ConfigurationIntegrityFailure:
            return true;
    }
    return false;
}

bool SafetyCore::isKnown(ConfigurationServiceMode mode) noexcept {
    switch (mode) {
        case ConfigurationServiceMode::NoRuntime:
        case ConfigurationServiceMode::RecoveryPreparing:
        case ConfigurationServiceMode::Operational:
        case ConfigurationServiceMode::CommitInProgress:
        case ConfigurationServiceMode::CommitIndeterminate:
        case ConfigurationServiceMode::ResetPreparing:
        case ConfigurationServiceMode::ResetEligibleNoRuntime:
        case ConfigurationServiceMode::EpochResetting:
        case ConfigurationServiceMode::BootstrapFinalizationPending:
        case ConfigurationServiceMode::RuntimeFailure:
            return true;
    }
    return false;
}

bool SafetyCore::isKnown(ConfigurationCommitStatus status) noexcept {
    switch (status) {
        case ConfigurationCommitStatus::Activated:
        case ConfigurationCommitStatus::NoChange:
        case ConfigurationCommitStatus::PreviewNotFound:
        case ConfigurationCommitStatus::PreviewSuperseded:
        case ConfigurationCommitStatus::ConfigurationMutationBusy:
        case ConfigurationCommitStatus::ConfigurationConflictFailure:
        case ConfigurationCommitStatus::ConfigurationValidationFailure:
        case ConfigurationCommitStatus::PersistenceFailure:
        case ConfigurationCommitStatus::CapacityFailure:
        case ConfigurationCommitStatus::ConfigurationCommitIndeterminate:
        case ConfigurationCommitStatus::ConfigurationRuntimeFailure:
            return true;
    }
    return false;
}

bool SafetyCore::isKnown(RunPersistenceLoadStatus status) noexcept {
    switch (status) {
        case RunPersistenceLoadStatus::NoPersistedRun:
        case RunPersistenceLoadStatus::Current:
        case RunPersistenceLoadStatus::NoActiveRun:
        case RunPersistenceLoadStatus::FallbackRecovered:
        case RunPersistenceLoadStatus::PreparedInterrupted:
        case RunPersistenceLoadStatus::NotReconstructible:
        case RunPersistenceLoadStatus::NotReconstructibleOrphanedState:
        case RunPersistenceLoadStatus::ReadFailed:
        case RunPersistenceLoadStatus::CapacityExceeded:
        case RunPersistenceLoadStatus::UnsupportedSchema:
        case RunPersistenceLoadStatus::ForeignEpoch:
        case RunPersistenceLoadStatus::AlreadyInitialized:
            return true;
    }
    return false;
}

bool SafetyCore::isKnown(RunPersistenceCoordinatorState state) noexcept {
    switch (state) {
        case RunPersistenceCoordinatorState::Uninitialized:
        case RunPersistenceCoordinatorState::ReadyEmpty:
        case RunPersistenceCoordinatorState::LoadedActiveRun:
        case RunPersistenceCoordinatorState::Ready:
        case RunPersistenceCoordinatorState::Busy:
        case RunPersistenceCoordinatorState::BlockedIndeterminate:
        case RunPersistenceCoordinatorState::FallbackRecoveryPending:
        case RunPersistenceCoordinatorState::PersistenceCommittedApplyFailed:
            return true;
    }
    return false;
}

void SafetyCore::setFault(FaultCode code) noexcept {
    if (activeFault_ == code) return;
    activeFault_ = code;
    acknowledged_ = false;
}

void SafetyCore::clearFault() noexcept { activeFault_ = FaultCode::None; }

}  // namespace fermentation
