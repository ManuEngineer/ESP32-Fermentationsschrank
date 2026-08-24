#include "fermentation_application.hpp"

#include "configuration_recovery_service.hpp"
#include "configuration_service.hpp"
#include "run_persistence_coordinator.hpp"

namespace fermentation {

bool FermentationApplication::begin(
    device_platform::IPlatformServices& platformServices,
    ConfigurationService& configurationService,
    const ConfigurationRecoveryResult& configurationRecoveryResult,
    RunPersistenceCoordinator* runPersistenceCoordinator,
    const RunPersistenceLoadResult* runPersistenceLoadResult,
    const device_platform::IResetCauseSource* resetCauseSource) {
    if (!platformServices.ready()) {
        return false;
    }
    if ((runPersistenceCoordinator == nullptr) !=
        (runPersistenceLoadResult == nullptr)) {
        return false;
    }

    platformServices_ = &platformServices;
    safetyCore_.beginBoot(resetCauseSource == nullptr
                              ? device_platform::ResetCause::Unknown
                              : resetCauseSource->resetCause());

    SafetyCoreInput bootEvidence;
    bootEvidence.bootValidationComplete = true;
    bootEvidence.configurationValidated =
        configurationRecoveryResult.status ==
            ConfigurationRecoveryStatus::RuntimeReady ||
        configurationRecoveryResult.status ==
            ConfigurationRecoveryStatus::FactoryInitializationCompleted ||
        configurationRecoveryResult.status ==
            ConfigurationRecoveryStatus::FactoryResetCompleted;
    bootEvidence.configurationRecoveryStatus =
        configurationRecoveryResult.status;
    bootEvidence.configurationServiceMode = configurationService.mode();
    bootEvidence.configurationProducer =
        configurationRecoveryResult.safetyProducer;

    if (runPersistenceCoordinator != nullptr &&
        runPersistenceLoadResult != nullptr) {
        bootEvidence.persistenceLoadStatus = runPersistenceLoadResult->status;
        bootEvidence.persistenceSnapshot =
            runPersistenceLoadResult->snapshot.has_value()
                ? &*runPersistenceLoadResult->snapshot
                : nullptr;
        bootEvidence.persistenceCoordinatorState =
            runPersistenceCoordinator->state();
        bootEvidence.persistenceValidated =
            runPersistenceLoadResult->status ==
                RunPersistenceLoadStatus::NoPersistedRun ||
            runPersistenceLoadResult->status ==
                RunPersistenceLoadStatus::Current ||
            runPersistenceLoadResult->status ==
                RunPersistenceLoadStatus::NoActiveRun ||
            (runPersistenceLoadResult->status ==
                 RunPersistenceLoadStatus::FallbackRecovered &&
             runPersistenceLoadResult->snapshot.has_value());
    }

    static_cast<void>(safetyCore_.evaluate(bootEvidence));
    return true;
}

bool FermentationApplication::begin(
    device_platform::IPlatformServices& platformServices,
    const device_platform::IResetCauseSource* resetCauseSource) {
    if (!platformServices.ready()) {
        return false;
    }

    platformServices_ = &platformServices;
    safetyCore_.beginBoot(resetCauseSource == nullptr
                              ? device_platform::ResetCause::Unknown
                              : resetCauseSource->resetCause());
    // Native smoke startup has no persistent production composition. Its
    // missing evidence remains fail-closed until the ESP-IDF root is used.
    const SafetyCoreInput bootEvidence;
    static_cast<void>(safetyCore_.evaluate(bootEvidence));
    return true;
}

void FermentationApplication::update() {
    // Die Fermentationslogik wird issueweise in diesem Modul implementiert.
}

bool FermentationApplication::ready() const {
    return platformServices_ != nullptr && platformServices_->ready();
}

}  // namespace fermentation
