#include "fermentation_application.hpp"

#include <utility>

#include "configuration_recovery_service.hpp"
#include "safety_fault_service.hpp"

namespace fermentation {

bool FermentationApplication::begin(
    device_platform::IPlatformServices& platformServices) {
    if (!platformServices.ready()) {
        return false;
    }

    platformServices_ = &platformServices;
    return true;
}

bool FermentationApplication::begin(
    device_platform::IPlatformServices& platformServices,
    SafetyDependencies dependencies,
    const FactoryNewSafetyProof& factoryProof) {
    if (!platformServices.ready()) return false;
    auto safety = std::make_unique<SafetyFaultService>(
        dependencies.stateStore, dependencies.resetController,
        dependencies.timeSource, dependencies.journal);
    if (safety->begin(factoryProof) != SafetyServiceStatus::Ready) return false;
    auto gate = std::make_unique<ConfigurationSafetyIntegrationGate>(
        dependencies.configurationRecovery, *safety);

    // Reset observation/evidence is consumed exactly once before the real
    // #56/#57 result is read. An authorized SAFE_BOOT exit is deliberately
    // deferred inside SafetyFaultService until this gate qualifies the
    // configuration in the same boot.
    const auto boot = safety->evaluateBoot();
    if (boot.status == SafetyServiceStatus::PersistentWriteFailed ||
        boot.status == SafetyServiceStatus::NotStarted) {
        return false;
    }

    // The real #56/#57 recovery result crosses this boundary before the
    // application is exposed as ready. A failed result leaves the safety
    // projection locked; it is not converted into an application-level
    // Allowed default.
    static_cast<void>(gate->boot());
    platformServices_ = &platformServices;
    safety_ = std::move(safety);
    configurationGate_ = std::move(gate);
    return true;
}

void FermentationApplication::update() {
    // Die Fermentationslogik wird issueweise in diesem Modul implementiert.
}

bool FermentationApplication::ready() const {
    return platformServices_ != nullptr && platformServices_->ready();
}

SafetyFaultService* FermentationApplication::safetyFaultService() const {
    return safety_.get();
}

ActuatorSafetyGateInput FermentationApplication::actuatorSafetyGateInput()
    const {
    return safety_ == nullptr
               ? ActuatorSafetyGateInput{ActuatorSafetyGateStatus::
                                             ImmediateStop}
               : safety_->actuatorGateInput();
}

SafetyServiceStatus FermentationApplication::forward(
    ConfigurationRecoveryResult result) {
    if (configurationGate_ == nullptr) return SafetyServiceStatus::NotStarted;
    return configurationGate_->forward(std::move(result)).safetyStatus;
}

SafetyServiceStatus FermentationApplication::forward(
    ConfigurationCommitResult result) {
    if (configurationGate_ == nullptr) return SafetyServiceStatus::NotStarted;
    return configurationGate_->forward(result);
}

SafetyServiceStatus FermentationApplication::forward(
    ConfigurationServiceMode mode) {
    if (configurationGate_ == nullptr) return SafetyServiceStatus::NotStarted;
    return configurationGate_->forward(mode);
}

}  // namespace fermentation
