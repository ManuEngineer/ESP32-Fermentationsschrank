#pragma once

#include <cstdint>
#include <memory>

#include "configuration_safety_integration_gate.hpp"
#include "platform_services.hpp"

namespace fermentation {

class ConfigurationRecoveryService;
class SafetyFaultService;

class FermentationApplication {
   public:
    struct SafetyDependencies {
        device_platform::IStateStore& stateStore;
        device_platform::IResetController& resetController;
        device_platform::ITimeSource& timeSource;
        ConfigurationRecoveryService& configurationRecovery;
        device_platform::IEventJournal* journal{nullptr};
    };

    [[nodiscard]] bool begin(
        device_platform::IPlatformServices& platformServices);
    [[nodiscard]] bool begin(
        device_platform::IPlatformServices& platformServices,
        SafetyDependencies dependencies,
        const FactoryNewSafetyProof& factoryProof = {});
    void update();

    [[nodiscard]] bool ready() const;
    [[nodiscard]] ActuatorSafetyGateInput actuatorSafetyGateInput() const;
    [[nodiscard]] SafetyServiceStatus forward(
        ConfigurationRecoveryResult result);
    [[nodiscard]] SafetyServiceStatus forward(ConfigurationCommitResult result);
    [[nodiscard]] SafetyServiceStatus forward(ConfigurationServiceMode mode);

   private:
    device_platform::IPlatformServices* platformServices_{nullptr};
    std::unique_ptr<SafetyFaultService> safety_;
    std::unique_ptr<ConfigurationSafetyIntegrationGate> configurationGate_;
};

}  // namespace fermentation
