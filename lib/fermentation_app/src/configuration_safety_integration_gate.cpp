#include "configuration_safety_integration_gate.hpp"

#include <utility>

namespace fermentation {

ConfigurationSafetyIntegrationResult
ConfigurationSafetyIntegrationGate::boot() {
    return forward(recovery_.boot());
}

ConfigurationSafetyIntegrationResult
ConfigurationSafetyIntegrationGate::forward(
    ConfigurationRecoveryResult result) {
    // A producer status is a stable cause identity. Polling the same result
    // must not manufacture a new Y4 latch on every forward() call.
    const auto correlationKey = static_cast<std::uint32_t>(result.status) + 1U;
    const auto safetyStatus = safety_.consumeConfigurationRecoveryResult(
        result, sourceKey_, correlationKey);
    return {std::move(result), safetyStatus};
}

SafetyServiceStatus ConfigurationSafetyIntegrationGate::forward(
    ConfigurationCommitResult result) {
    const auto correlationKey =
        static_cast<std::uint32_t>(result.status) + 0x100U;
    return safety_.consumeConfigurationStatus(result.status, sourceKey_,
                                              correlationKey);
}

SafetyServiceStatus ConfigurationSafetyIntegrationGate::forward(
    ConfigurationServiceMode mode) {
    const auto correlationKey = static_cast<std::uint32_t>(mode) + 0x200U;
    return safety_.consumeConfigurationStatus(mode, sourceKey_, correlationKey);
}

}  // namespace fermentation
