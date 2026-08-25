#include "safety_core.hpp"

#include "actuator_planner.hpp"

namespace fermentation {
namespace {

bool isPersistenceSafeBoot(RunPersistenceLoadStatus status) {
    switch (status) {
        case RunPersistenceLoadStatus::NoPersistedRun:
        case RunPersistenceLoadStatus::Current:
        case RunPersistenceLoadStatus::NoActiveRun:
        case RunPersistenceLoadStatus::FallbackRecovered:
            return false;
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

bool isTrustedCoordinatorState(RunPersistenceCoordinatorState state) {
    return state == RunPersistenceCoordinatorState::ReadyEmpty ||
           state == RunPersistenceCoordinatorState::LoadedActiveRun ||
           state == RunPersistenceCoordinatorState::Ready;
}

bool isValidFallbackRecoveryEvidence(const SafetyCoreInput& input) {
    return input.persistenceLoadStatus.has_value() &&
           *input.persistenceLoadStatus ==
               RunPersistenceLoadStatus::FallbackRecovered &&
           input.persistenceValidated && input.persistenceSnapshot != nullptr &&
           input.persistenceCoordinatorState ==
               RunPersistenceCoordinatorState::FallbackRecoveryPending;
}

bool hasFreshConfigurationEvidence(const SafetyCoreInput& input) {
    if (!input.configurationValidated) return false;
    if (input.configurationRecoveryStatus.has_value()) {
        switch (*input.configurationRecoveryStatus) {
            case ConfigurationRecoveryStatus::RuntimeReady:
            case ConfigurationRecoveryStatus::FactoryInitializationCompleted:
            case ConfigurationRecoveryStatus::FactoryResetCompleted:
                return true;
            default:
                break;
        }
    }
    if (input.configurationServiceMode.has_value() &&
        *input.configurationServiceMode ==
            ConfigurationServiceMode::Operational)
        return true;
    if (input.configurationCommitStatus.has_value() &&
        (*input.configurationCommitStatus ==
             ConfigurationCommitStatus::Activated ||
         *input.configurationCommitStatus ==
             ConfigurationCommitStatus::NoChange))
        return true;
    return false;
}

bool hasFreshSensorEvidence(const SafetyCoreInput& input) {
    return input.sensorEvidenceValidated && input.peltierSensor != nullptr &&
           input.peltierSensor->quality ==
               device_platform::SensorQuality::Valid &&
           input.sensorSelectionRuntime != nullptr &&
           input.sensorSelectionRuntime->permission ==
               SensorPeltierPermission::Allowed;
}

constexpr std::array<FaultCode, SafetyCore::kFaultCount> kFaultPriority = {
    FaultCode::ConfigurationUnavailable,
    FaultCode::ConfigurationIntegrityFailure,
    FaultCode::ConfigurationCommitIndeterminate,
    FaultCode::SystemProducerUnknown,
    FaultCode::RunPersistenceUntrusted,
    FaultCode::ConfigurationRuntimeFailure,
    FaultCode::SafetySensorUnavailable,
    FaultCode::ActuatorRequestWatchdog,
};

bool hasFreshIntegrityEvidence(const SafetyCoreInput& input) {
    if (!input.configurationValidated) return false;
    if (input.configurationRecoveryStatus.has_value()) {
        switch (*input.configurationRecoveryStatus) {
            case ConfigurationRecoveryStatus::RuntimeReady:
            case ConfigurationRecoveryStatus::FactoryInitializationCompleted:
            case ConfigurationRecoveryStatus::FactoryResetCompleted:
                return true;
            default:
                break;
        }
    }
    return input.configurationServiceMode.has_value() &&
           *input.configurationServiceMode ==
               ConfigurationServiceMode::Operational;
}

bool hasResolvedCommitEvidence(const SafetyCoreInput& input) {
    if (input.configurationCommitStatus.has_value() &&
        (*input.configurationCommitStatus ==
             ConfigurationCommitStatus::Activated ||
         *input.configurationCommitStatus ==
             ConfigurationCommitStatus::NoChange)) {
        return true;
    }
    return hasFreshIntegrityEvidence(input);
}

}  // namespace

void SafetyCore::beginBoot(device_platform::ResetCause resetCause) noexcept {
    resetCause_ = resetCause;
    activeFaultMask_ = 0U;
    acknowledgedFaultMask_ = 0U;
    unknownProducerSources_ = 0U;
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

    FaultMask observedFaults = 0U;
    const auto observe = [&observedFaults](FaultCode code) {
        observedFaults =
            static_cast<FaultMask>(observedFaults | SafetyCore::faultBit(code));
    };

    const auto observeProducerKnownness = [this, &observe](
                                              UnknownProducerSource source,
                                              bool provided, bool known) {
        if (!provided) return;
        const auto bit = unknownProducerSourceBit(source);
        if (known) {
            unknownProducerSources_ = static_cast<UnknownProducerSourceMask>(
                unknownProducerSources_ & ~bit);
            return;
        }
        unknownProducerSources_ = static_cast<UnknownProducerSourceMask>(
            unknownProducerSources_ | bit);
        observe(FaultCode::SystemProducerUnknown);
    };

    observeProducerKnownness(UnknownProducerSource::ConfigurationServiceMode,
                             input.configurationServiceMode.has_value(),
                             input.configurationServiceMode.has_value() &&
                                 isKnown(*input.configurationServiceMode));
    observeProducerKnownness(UnknownProducerSource::ConfigurationCommitStatus,
                             input.configurationCommitStatus.has_value(),
                             input.configurationCommitStatus.has_value() &&
                                 isKnown(*input.configurationCommitStatus));
    observeProducerKnownness(UnknownProducerSource::ConfigurationRecoveryStatus,
                             input.configurationRecoveryStatus.has_value(),
                             input.configurationRecoveryStatus.has_value() &&
                                 isKnown(*input.configurationRecoveryStatus));
    observeProducerKnownness(UnknownProducerSource::ConfigurationSafetyProducer,
                             input.configurationProducer.has_value(),
                             input.configurationProducer.has_value() &&
                                 isKnown(*input.configurationProducer));
    observeProducerKnownness(UnknownProducerSource::PersistenceLoadStatus,
                             input.persistenceLoadStatus.has_value(),
                             input.persistenceLoadStatus.has_value() &&
                                 isKnown(*input.persistenceLoadStatus));
    observeProducerKnownness(UnknownProducerSource::PersistenceCoordinatorState,
                             true, isKnown(input.persistenceCoordinatorState));

    if (input.configurationProducer.has_value()) {
        switch (*input.configurationProducer) {
            case ConfigurationSafetyProducer::ConfigurationUnavailable:
                observe(FaultCode::ConfigurationUnavailable);
                break;
            case ConfigurationSafetyProducer::ConfigurationIntegrityFailure:
                observe(FaultCode::ConfigurationIntegrityFailure);
                break;
        }
    }
    // ConfigurationRecoveryStatus is diagnostic detail.  The recovery service's
    // safetyProducer is the only authority for whether a failed attempt
    // invalidated the old runtime.  In particular, a rejected reset/recovery
    // may retain a valid Operational runtime and deliberately clear that
    // producer; the same status value must not create a second Safety FSM here.
    if (input.configurationServiceMode.has_value()) {
        switch (*input.configurationServiceMode) {
            case ConfigurationServiceMode::CommitIndeterminate:
            case ConfigurationServiceMode::BootstrapFinalizationPending:
                observe(FaultCode::ConfigurationCommitIndeterminate);
                break;
            case ConfigurationServiceMode::RuntimeFailure:
                observe(FaultCode::ConfigurationRuntimeFailure);
                break;
            default:
                break;
        }
    }
    if (input.configurationCommitStatus.has_value()) {
        switch (*input.configurationCommitStatus) {
            case ConfigurationCommitStatus::ConfigurationCommitIndeterminate:
                observe(FaultCode::ConfigurationCommitIndeterminate);
                break;
            case ConfigurationCommitStatus::ConfigurationRuntimeFailure:
                observe(FaultCode::ConfigurationRuntimeFailure);
                break;
            case ConfigurationCommitStatus::Activated:
            case ConfigurationCommitStatus::NoChange:
                break;
            case ConfigurationCommitStatus::PreviewNotFound:
            case ConfigurationCommitStatus::PreviewSuperseded:
            case ConfigurationCommitStatus::ConfigurationMutationBusy:
            case ConfigurationCommitStatus::ConfigurationConflictFailure:
            case ConfigurationCommitStatus::ConfigurationValidationFailure:
            case ConfigurationCommitStatus::PersistenceFailure:
            case ConfigurationCommitStatus::CapacityFailure:
                break;
        }
    }
    if (!input.configurationValidated)
        observe(FaultCode::SystemProducerUnknown);

    RunLoadDisposition loadDisposition = RunLoadDisposition::SafeBoot;
    const bool hasKnownLoadStatus = input.persistenceLoadStatus.has_value() &&
                                    isKnown(*input.persistenceLoadStatus);
    const bool validFallbackRecoveryEvidence =
        isValidFallbackRecoveryEvidence(input);
    if (!hasKnownLoadStatus ||
        isPersistenceSafeBoot(input.persistenceLoadStatus.value_or(
            RunPersistenceLoadStatus::ReadFailed))) {
        observe(FaultCode::RunPersistenceUntrusted);
    } else {
        loadDisposition = boot_classification::classifyRunLoad(
            *input.persistenceLoadStatus, input.persistenceSnapshot);
        if (loadDisposition == RunLoadDisposition::SafeBoot)
            observe(FaultCode::RunPersistenceUntrusted);
    }
    if (input.persistenceLoadStatus.has_value() &&
        *input.persistenceLoadStatus ==
            RunPersistenceLoadStatus::FallbackRecovered &&
        !validFallbackRecoveryEvidence)
        observe(FaultCode::RunPersistenceUntrusted);
    switch (input.persistenceCoordinatorState) {
        case RunPersistenceCoordinatorState::BlockedIndeterminate:
        case RunPersistenceCoordinatorState::PersistenceCommittedApplyFailed:
        case RunPersistenceCoordinatorState::Busy:
        case RunPersistenceCoordinatorState::Uninitialized:
            observe(FaultCode::RunPersistenceUntrusted);
            break;
        case RunPersistenceCoordinatorState::FallbackRecoveryPending:
            if (!validFallbackRecoveryEvidence)
                observe(FaultCode::RunPersistenceUntrusted);
            break;
        default:
            break;
    }
    if (!input.persistenceValidated)
        observe(FaultCode::RunPersistenceUntrusted);

    const bool gateNeedsSensorEvidence =
        input.explicitActivationRequested ||
        input.activationKind != SafetyActivationKind::None ||
        loadDisposition == RunLoadDisposition::ResumeOffer;
    const bool sensorPointerIsInvalid =
        input.peltierSensor != nullptr &&
        input.peltierSensor->quality != device_platform::SensorQuality::Valid;
    if (sensorPointerIsInvalid ||
        (gateNeedsSensorEvidence && !hasFreshSensorEvidence(input)))
        observe(FaultCode::SafetySensorUnavailable);

    if (input.actuatorPlanner != nullptr &&
        input.actuatorPlanner->state().latchedWatchdogFault.has_value())
        observe(FaultCode::ActuatorRequestWatchdog);

    activeFaultMask_ =
        static_cast<FaultMask>(activeFaultMask_ | observedFaults);
    bool clearedFault = false;
    for (const auto code : kFaultPriority) {
        if (!hasFault(activeFaultMask_, code) || hasFault(observedFaults, code))
            continue;
        if (canClearFault(code, input, loadDisposition)) {
            clearFault(code);
            clearedFault = true;
        }
    }

    const FaultCode primary = primaryFault(activeFaultMask_);
    if (primary != FaultCode::None) {
        result.faultCode = primary;
        result.acknowledged = isAcknowledged(primary);
        result.disposition = dispositionForFault(primary);
        result.bootDisposition =
            result.disposition == SafetyDisposition::SafeBoot
                ? SafetyBootDisposition::SafeBoot
                : SafetyBootDisposition::Unresolved;
        return finalize(result);
    }

    result.faultCode = FaultCode::None;
    result.acknowledged = false;
    result.disposition = SafetyDisposition::Information;
    switch (loadDisposition) {
        case RunLoadDisposition::Standby:
            result.bootDisposition = SafetyBootDisposition::Standby;
            if (clearedFault ||
                !activationEvidenceComplete(input, loadDisposition,
                                            SafetyActivationKind::FreshStart))
                return finalize(result);
            break;
        case RunLoadDisposition::NoActiveRun:
            result.bootDisposition = SafetyBootDisposition::NoActiveRun;
            return finalize(result);
        case RunLoadDisposition::Completed:
            result.bootDisposition = SafetyBootDisposition::Completed;
            return finalize(result);
        case RunLoadDisposition::TerminalFault:
            result.bootDisposition = SafetyBootDisposition::TerminalFault;
            return finalize(result);
        case RunLoadDisposition::ResumeOffer:
            result.bootDisposition = SafetyBootDisposition::ResumeOffer;
            if (clearedFault ||
                !activationEvidenceComplete(input, loadDisposition,
                                            SafetyActivationKind::Resume))
                return finalize(result);
            break;
        case RunLoadDisposition::SafeBoot:
            observe(FaultCode::RunPersistenceUntrusted);
            setFault(FaultCode::RunPersistenceUntrusted);
            result.faultCode = primaryFault(activeFaultMask_);
            result.disposition = SafetyDisposition::SafeBoot;
            result.bootDisposition = SafetyBootDisposition::SafeBoot;
            return finalize(result);
    }

    result.gate.status = ActuatorSafetyGateStatus::Allowed;
    return finalize(result);
}

void SafetyCore::acknowledge(FaultCode code) noexcept {
    if (!hasFault(activeFaultMask_, code)) return;
    acknowledgedFaultMask_ =
        static_cast<FaultMask>(acknowledgedFaultMask_ | faultBit(code));
    const auto primary = primaryFault(activeFaultMask_);
    lastEvaluation_.acknowledged = isAcknowledged(primary);
}

bool SafetyCore::isAcknowledged(FaultCode code) const noexcept {
    return code != FaultCode::None && hasFault(acknowledgedFaultMask_, code);
}

bool SafetyCore::resetRequestWatchdog(ActuatorPlanner& planner,
                                      std::uint64_t nowMonotonicMillis,
                                      const SafetyCoreInput& input) {
    if (input.actuatorPlanner != &planner ||
        !planner.state().latchedWatchdogFault.has_value() ||
        !input.explicitActivationRequested || !input.bootValidationComplete ||
        !hasFreshConfigurationEvidence(input) ||
        !hasFreshSensorEvidence(input) || !input.plannerEvidenceValidated ||
        !input.persistenceValidated ||
        !input.persistenceLoadStatus.has_value() ||
        !isKnown(*input.persistenceLoadStatus) ||
        isPersistenceSafeBoot(*input.persistenceLoadStatus) ||
        !isTrustedCoordinatorState(input.persistenceCoordinatorState) ||
        boot_classification::classifyRunLoad(*input.persistenceLoadStatus,
                                             input.persistenceSnapshot) ==
            RunLoadDisposition::SafeBoot ||
        !hasFault(activeFaultMask_, FaultCode::ActuatorRequestWatchdog) ||
        activeFaultMask_ != faultBit(FaultCode::ActuatorRequestWatchdog))
        return false;

    // Re-project all current producer inputs before authorizing the existing
    // #23 reset. This prevents a stale previous evaluation from hiding a
    // simultaneous configuration, persistence or sensor fault.
    static_cast<void>(evaluate(input));
    if (activeFaultMask_ != faultBit(FaultCode::ActuatorRequestWatchdog) ||
        !planner.state().latchedWatchdogFault.has_value())
        return false;

    planner.applyExternalWatchdogFaultReset(nowMonotonicMillis);
    clearFault(FaultCode::ActuatorRequestWatchdog);
    lastEvaluation_ = SafetyEvaluation{};
    lastEvaluation_.resetCause = resetCause_;
    return true;
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

SafetyCore::FaultMask SafetyCore::faultBit(FaultCode code) noexcept {
    switch (code) {
        case FaultCode::ConfigurationRuntimeFailure:
            return 1U << 0U;
        case FaultCode::ConfigurationUnavailable:
            return 1U << 1U;
        case FaultCode::ConfigurationIntegrityFailure:
            return 1U << 2U;
        case FaultCode::ConfigurationCommitIndeterminate:
            return 1U << 3U;
        case FaultCode::RunPersistenceUntrusted:
            return 1U << 4U;
        case FaultCode::SafetySensorUnavailable:
            return 1U << 5U;
        case FaultCode::ActuatorRequestWatchdog:
            return 1U << 6U;
        case FaultCode::SystemProducerUnknown:
            return 1U << 7U;
        case FaultCode::None:
            return 0U;
    }
    return 0U;
}

SafetyCore::UnknownProducerSourceMask SafetyCore::unknownProducerSourceBit(
    UnknownProducerSource source) noexcept {
    const auto index = static_cast<std::uint8_t>(source);
    if (index >= 8U) return 0U;
    return static_cast<UnknownProducerSourceMask>(1U << index);
}

bool SafetyCore::hasFault(FaultMask mask, FaultCode code) noexcept {
    const auto bit = faultBit(code);
    return bit != 0U && (mask & bit) != 0U;
}

FaultCode SafetyCore::primaryFault(FaultMask mask) noexcept {
    for (const auto code : kFaultPriority) {
        if (hasFault(mask, code)) return code;
    }
    return FaultCode::None;
}

SafetyDisposition SafetyCore::dispositionForFault(FaultCode code) noexcept {
    switch (code) {
        case FaultCode::ConfigurationRuntimeFailure:
        case FaultCode::SafetySensorUnavailable:
        case FaultCode::ActuatorRequestWatchdog:
            return SafetyDisposition::BlockedImmediateStop;
        case FaultCode::ConfigurationUnavailable:
        case FaultCode::ConfigurationIntegrityFailure:
        case FaultCode::ConfigurationCommitIndeterminate:
        case FaultCode::RunPersistenceUntrusted:
        case FaultCode::SystemProducerUnknown:
            return SafetyDisposition::SafeBoot;
        case FaultCode::None:
            return SafetyDisposition::Information;
    }
    return SafetyDisposition::SafeBoot;
}

bool SafetyCore::activationEvidenceComplete(
    const SafetyCoreInput& input, RunLoadDisposition loadDisposition,
    SafetyActivationKind expectedKind) noexcept {
    return input.activationKind == expectedKind &&
           input.bootValidationComplete && input.explicitActivationRequested &&
           input.plannerEvidenceValidated &&
           hasFreshConfigurationEvidence(input) &&
           hasFreshSensorEvidence(input) && input.persistenceValidated &&
           input.persistenceLoadStatus.has_value() &&
           isKnown(*input.persistenceLoadStatus) &&
           !isPersistenceSafeBoot(*input.persistenceLoadStatus) &&
           isTrustedCoordinatorState(input.persistenceCoordinatorState) &&
           loadDisposition != RunLoadDisposition::SafeBoot &&
           input.activationPersistenceResult.has_value() &&
           *input.activationPersistenceResult ==
               RunPersistenceResultStatus::Applied &&
           input.processActivationApplied;
}

bool SafetyCore::canClearFault(
    FaultCode code, const SafetyCoreInput& input,
    RunLoadDisposition loadDisposition) const noexcept {
    switch (code) {
        case FaultCode::ConfigurationRuntimeFailure:
            return input.explicitActivationRequested &&
                   hasFreshConfigurationEvidence(input);
        case FaultCode::ConfigurationUnavailable:
            return input.bootValidationComplete &&
                   hasFreshConfigurationEvidence(input);
        case FaultCode::ConfigurationIntegrityFailure:
            return input.bootValidationComplete &&
                   hasFreshIntegrityEvidence(input);
        case FaultCode::ConfigurationCommitIndeterminate:
            return input.bootValidationComplete &&
                   hasResolvedCommitEvidence(input);
        case FaultCode::RunPersistenceUntrusted: {
            const bool trustedCoordinator =
                isTrustedCoordinatorState(input.persistenceCoordinatorState) ||
                isValidFallbackRecoveryEvidence(input);
            return input.bootValidationComplete && input.persistenceValidated &&
                   input.persistenceLoadStatus.has_value() &&
                   !isPersistenceSafeBoot(*input.persistenceLoadStatus) &&
                   trustedCoordinator &&
                   loadDisposition != RunLoadDisposition::SafeBoot &&
                   (loadDisposition != RunLoadDisposition::ResumeOffer ||
                    input.persistenceSnapshot != nullptr);
        }
        case FaultCode::SafetySensorUnavailable:
            return hasFreshSensorEvidence(input);
        case FaultCode::ActuatorRequestWatchdog:
            return false;
        case FaultCode::SystemProducerUnknown: {
            const bool trustedCoordinator =
                isTrustedCoordinatorState(input.persistenceCoordinatorState) ||
                isValidFallbackRecoveryEvidence(input);
            return unknownProducerSources_ == 0U &&
                   input.bootValidationComplete &&
                   hasFreshConfigurationEvidence(input) &&
                   input.persistenceValidated &&
                   input.persistenceLoadStatus.has_value() &&
                   !isPersistenceSafeBoot(*input.persistenceLoadStatus) &&
                   trustedCoordinator;
        }
        case FaultCode::None:
            return true;
    }
    return false;
}

void SafetyCore::setFault(FaultCode code) noexcept {
    const auto bit = faultBit(code);
    if (bit == 0U) return;
    if ((activeFaultMask_ & bit) == 0U)
        acknowledgedFaultMask_ =
            static_cast<FaultMask>(acknowledgedFaultMask_ & ~bit);
    activeFaultMask_ = static_cast<FaultMask>(activeFaultMask_ | bit);
}

void SafetyCore::clearFault(FaultCode code) noexcept {
    const auto bit = faultBit(code);
    activeFaultMask_ = static_cast<FaultMask>(activeFaultMask_ & ~bit);
    acknowledgedFaultMask_ =
        static_cast<FaultMask>(acknowledgedFaultMask_ & ~bit);
}

}  // namespace fermentation
