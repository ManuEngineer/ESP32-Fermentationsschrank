#include "actuation_interlock.hpp"

#include <array>

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

bool hasFreshConfigurationEvidence(const ActuationEvidence& evidence) {
    if (!evidence.configurationValidated) return false;
    if (evidence.configurationRecoveryStatus.has_value()) {
        switch (*evidence.configurationRecoveryStatus) {
            case ConfigurationRecoveryStatus::RuntimeReady:
            case ConfigurationRecoveryStatus::FactoryInitializationCompleted:
            case ConfigurationRecoveryStatus::FactoryResetCompleted:
                return true;
            default:
                break;
        }
    }
    if (evidence.configurationServiceMode.has_value() &&
        *evidence.configurationServiceMode ==
            ConfigurationServiceMode::Operational)
        return true;
    if (evidence.configurationCommitStatus.has_value() &&
        (*evidence.configurationCommitStatus ==
             ConfigurationCommitStatus::Activated ||
         *evidence.configurationCommitStatus ==
             ConfigurationCommitStatus::NoChange))
        return true;
    return false;
}

bool hasFreshSensorEvidence(const ActuationEvidence& evidence) {
    return evidence.sensorEvidenceValidated &&
           evidence.peltierSensor != nullptr &&
           evidence.peltierSensor->quality ==
               device_platform::SensorQuality::Valid &&
           evidence.sensorSelectionRuntime != nullptr &&
           evidence.sensorSelectionRuntime->permission ==
               SensorPeltierPermission::Allowed;
}

constexpr std::array<FaultCode, ActuationInterlock::kFaultCount>
    kFaultPriority = {
        FaultCode::ConfigurationUnavailable,
        FaultCode::ConfigurationIntegrityFailure,
        FaultCode::ConfigurationCommitIndeterminate,
        FaultCode::SystemProducerUnknown,
        FaultCode::RunPersistenceUntrusted,
        FaultCode::ConfigurationRuntimeFailure,
        FaultCode::SafetySensorUnavailable,
        FaultCode::ActuatorRequestWatchdog,
};

}  // namespace

ActuationEvaluation ActuationInterlock::evaluate(
    const ActuationEvidence& evidence) {
    ActuationEvaluation result;
    FaultMask observedFaults = 0U;
    const auto observe = [&observedFaults](FaultCode code) {
        observedFaults = static_cast<FaultMask>(
            observedFaults | ActuationInterlock::faultBit(code));
    };

    const auto observeProducerKnownness = [&observe](bool provided,
                                                     bool known) {
        if (provided && !known) observe(FaultCode::SystemProducerUnknown);
    };
    observeProducerKnownness(evidence.configurationServiceMode.has_value(),
                             evidence.configurationServiceMode.has_value() &&
                                 isKnown(*evidence.configurationServiceMode));
    observeProducerKnownness(evidence.configurationCommitStatus.has_value(),
                             evidence.configurationCommitStatus.has_value() &&
                                 isKnown(*evidence.configurationCommitStatus));
    observeProducerKnownness(
        evidence.configurationRecoveryStatus.has_value(),
        evidence.configurationRecoveryStatus.has_value() &&
            isKnown(*evidence.configurationRecoveryStatus));
    observeProducerKnownness(evidence.configurationProducer.has_value(),
                             evidence.configurationProducer.has_value() &&
                                 isKnown(*evidence.configurationProducer));
    observeProducerKnownness(evidence.persistenceLoadStatus.has_value(),
                             evidence.persistenceLoadStatus.has_value() &&
                                 isKnown(*evidence.persistenceLoadStatus));
    observeProducerKnownness(true,
                             isKnown(evidence.persistenceCoordinatorState));

    if (evidence.configurationProducer.has_value()) {
        switch (*evidence.configurationProducer) {
            case ConfigurationSafetyProducer::ConfigurationUnavailable:
                observe(FaultCode::ConfigurationUnavailable);
                break;
            case ConfigurationSafetyProducer::ConfigurationIntegrityFailure:
                observe(FaultCode::ConfigurationIntegrityFailure);
                break;
        }
    }
    // ConfigurationRecoveryStatus is diagnostic detail. The recovery
    // service's safetyProducer remains the authority for failed attempts.
    if (evidence.configurationServiceMode.has_value()) {
        switch (*evidence.configurationServiceMode) {
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
    if (evidence.configurationCommitStatus.has_value()) {
        switch (*evidence.configurationCommitStatus) {
            case ConfigurationCommitStatus::ConfigurationCommitIndeterminate:
                observe(FaultCode::ConfigurationCommitIndeterminate);
                break;
            case ConfigurationCommitStatus::ConfigurationRuntimeFailure:
                observe(FaultCode::ConfigurationRuntimeFailure);
                break;
            case ConfigurationCommitStatus::Activated:
            case ConfigurationCommitStatus::NoChange:
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
    if (!evidence.configurationValidated)
        observe(FaultCode::SystemProducerUnknown);

    const bool hasKnownLoadStatus =
        evidence.persistenceLoadStatus.has_value() &&
        isKnown(*evidence.persistenceLoadStatus) &&
        evidence.persistenceCoordinatorState !=
            RunPersistenceCoordinatorState::FallbackRecoveryPending;
    if (!hasKnownLoadStatus ||
        isPersistenceSafeBoot(evidence.persistenceLoadStatus.value_or(
            RunPersistenceLoadStatus::ReadFailed)) ||
        evidence.loadDisposition == RunLoadDisposition::SafeBoot) {
        observe(FaultCode::RunPersistenceUntrusted);
    }
    // R1 has no fallback trust exception. Both the recovered status and the
    // pending coordinator state remain denied until a later approved flow.
    if (evidence.persistenceLoadStatus.has_value() &&
        *evidence.persistenceLoadStatus ==
            RunPersistenceLoadStatus::FallbackRecovered)
        observe(FaultCode::RunPersistenceUntrusted);

    switch (evidence.persistenceCoordinatorState) {
        case RunPersistenceCoordinatorState::BlockedIndeterminate:
        case RunPersistenceCoordinatorState::PersistenceCommittedApplyFailed:
        case RunPersistenceCoordinatorState::Busy:
        case RunPersistenceCoordinatorState::Uninitialized:
        case RunPersistenceCoordinatorState::FallbackRecoveryPending:
            observe(FaultCode::RunPersistenceUntrusted);
            break;
        default:
            break;
    }
    if (!evidence.persistenceValidated)
        observe(FaultCode::RunPersistenceUntrusted);

    const bool gateNeedsSensorEvidence =
        evidence.explicitActivationRequested ||
        evidence.activationKind != SafetyActivationKind::None;
    const bool sensorPointerIsInvalid =
        evidence.peltierSensor != nullptr &&
        evidence.peltierSensor->quality !=
            device_platform::SensorQuality::Valid;
    if (sensorPointerIsInvalid ||
        (gateNeedsSensorEvidence && !hasFreshSensorEvidence(evidence)))
        observe(FaultCode::SafetySensorUnavailable);

    if (evidence.actuatorPlanner != nullptr &&
        evidence.actuatorPlanner->state().latchedWatchdogFault.has_value())
        observe(FaultCode::ActuatorRequestWatchdog);

    const FaultCode primary = primaryFault(observedFaults);
    if (primary != FaultCode::None) {
        result.faultCode = primary;
        return result;
    }

    switch (evidence.loadDisposition) {
        case RunLoadDisposition::Standby:
            if (activationEvidenceComplete(evidence,
                                           SafetyActivationKind::FreshStart,
                                           evidence.loadDisposition))
                result.permission = ActuatorSafetyGateStatus::Allowed;
            break;
        case RunLoadDisposition::ResumeOffer:
            if (activationEvidenceComplete(evidence,
                                           SafetyActivationKind::Resume,
                                           evidence.loadDisposition))
                result.permission = ActuatorSafetyGateStatus::Allowed;
            break;
        case RunLoadDisposition::NoActiveRun:
        case RunLoadDisposition::Completed:
        case RunLoadDisposition::TerminalFault:
        case RunLoadDisposition::SafeBoot:
            break;
    }
    return result;
}

bool ActuationInterlock::resetRequestWatchdog(
    ActuatorPlanner& planner, std::uint64_t nowMonotonicMillis,
    const ActuationEvidence& evidence) {
    if (evidence.actuatorPlanner != &planner) return false;
    if (!planner.state().latchedWatchdogFault.has_value()) return false;
    if (!evidence.explicitActivationRequested) return false;
    if (!evidence.bootValidationComplete) return false;
    if (!evidence.plannerEvidenceValidated) return false;
    if (!hasFreshConfigurationEvidence(evidence)) return false;
    if (!hasFreshSensorEvidence(evidence)) return false;
    if (!evidence.persistenceValidated) return false;
    if (!evidence.persistenceLoadStatus.has_value()) return false;
    if (!isKnown(*evidence.persistenceLoadStatus)) return false;
    if (isPersistenceSafeBoot(*evidence.persistenceLoadStatus)) return false;
    if (!isTrustedCoordinatorState(evidence.persistenceCoordinatorState))
        return false;
    if (evidence.loadDisposition == RunLoadDisposition::SafeBoot) return false;

    const ActuationEvaluation fresh = evaluate(evidence);
    if (fresh.faultCode != FaultCode::ActuatorRequestWatchdog) return false;

    planner.applyExternalWatchdogFaultReset(nowMonotonicMillis);
    return true;
}

bool ActuationInterlock::isKnown(ConfigurationRecoveryStatus status) noexcept {
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

bool ActuationInterlock::isKnown(
    ConfigurationSafetyProducer producer) noexcept {
    switch (producer) {
        case ConfigurationSafetyProducer::ConfigurationUnavailable:
        case ConfigurationSafetyProducer::ConfigurationIntegrityFailure:
            return true;
    }
    return false;
}

bool ActuationInterlock::isKnown(ConfigurationServiceMode mode) noexcept {
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

bool ActuationInterlock::isKnown(ConfigurationCommitStatus status) noexcept {
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

bool ActuationInterlock::isKnown(RunPersistenceLoadStatus status) noexcept {
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

bool ActuationInterlock::isKnown(
    RunPersistenceCoordinatorState state) noexcept {
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

ActuationInterlock::FaultMask ActuationInterlock::faultBit(
    FaultCode code) noexcept {
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

bool ActuationInterlock::hasFault(FaultMask mask, FaultCode code) noexcept {
    const auto bit = faultBit(code);
    return bit != 0U && (mask & bit) != 0U;
}

FaultCode ActuationInterlock::primaryFault(FaultMask mask) noexcept {
    for (const auto code : kFaultPriority) {
        if (hasFault(mask, code)) return code;
    }
    return FaultCode::None;
}

bool ActuationInterlock::activationEvidenceComplete(
    const ActuationEvidence& evidence, SafetyActivationKind expectedKind,
    RunLoadDisposition loadDisposition) noexcept {
    return evidence.activationKind == expectedKind &&
           evidence.bootValidationComplete &&
           evidence.explicitActivationRequested &&
           evidence.plannerEvidenceValidated &&
           hasFreshConfigurationEvidence(evidence) &&
           hasFreshSensorEvidence(evidence) && evidence.persistenceValidated &&
           evidence.persistenceLoadStatus.has_value() &&
           isKnown(*evidence.persistenceLoadStatus) &&
           !isPersistenceSafeBoot(*evidence.persistenceLoadStatus) &&
           isTrustedCoordinatorState(evidence.persistenceCoordinatorState) &&
           loadDisposition != RunLoadDisposition::SafeBoot &&
           evidence.activationPersistenceResult.has_value() &&
           *evidence.activationPersistenceResult ==
               RunPersistenceResultStatus::Applied &&
           evidence.processActivationApplied;
}

}  // namespace fermentation
