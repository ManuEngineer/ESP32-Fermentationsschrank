#pragma once

#include <cstdint>

#include "configuration_recovery_service.hpp"
#include "safety_fault_service.hpp"

namespace fermentation {

// Composition-root bridge for the real #56/#57 producer. It deliberately
// accepts ConfigurationRecoveryResult, not a test-only status enum, and maps
// every producer outcome into the single #24 SafetyFaultService authority.
struct ConfigurationSafetyIntegrationResult {
    ConfigurationRecoveryResult recovery;
    SafetyServiceStatus safetyStatus{SafetyServiceStatus::NotStarted};
};

class ConfigurationSafetyIntegrationGate final {
   public:
    ConfigurationSafetyIntegrationGate(ConfigurationRecoveryService& recovery,
                                       SafetyFaultService& safety,
                                       std::uint32_t sourceKey = 56U)
        : recovery_(recovery), safety_(safety), sourceKey_(sourceKey) {}

    [[nodiscard]] ConfigurationSafetyIntegrationResult boot();
    [[nodiscard]] ConfigurationSafetyIntegrationResult forward(
        ConfigurationRecoveryResult result);
    [[nodiscard]] SafetyServiceStatus forward(ConfigurationCommitResult result);
    [[nodiscard]] SafetyServiceStatus forward(ConfigurationServiceMode mode);

   private:
    ConfigurationRecoveryService& recovery_;
    SafetyFaultService& safety_;
    std::uint32_t sourceKey_{56U};
    std::uint32_t nextCorrelationKey_{1U};
};

}  // namespace fermentation
