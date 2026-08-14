#include "configuration_safety_integration_gate.hpp"

#include <limits>
#include <utility>

namespace fermentation {

ConfigurationSafetyIntegrationResult
ConfigurationSafetyIntegrationGate::boot() {
    return forward(recovery_.boot());
}

ConfigurationSafetyIntegrationResult
ConfigurationSafetyIntegrationGate::forward(
    ConfigurationRecoveryResult result) {
    const auto correlationKey = nextCorrelationKey_;
    if (nextCorrelationKey_ != std::numeric_limits<std::uint32_t>::max()) {
        ++nextCorrelationKey_;
    }
    const auto safetyStatus = safety_.consumeConfigurationRecoveryResult(
        result, sourceKey_, correlationKey);
    return {std::move(result), safetyStatus};
}

SafetyServiceStatus ConfigurationSafetyIntegrationGate::forward(
    ConfigurationCommitResult result) {
    const auto correlationKey = nextCorrelationKey_;
    if (nextCorrelationKey_ != std::numeric_limits<std::uint32_t>::max()) {
        ++nextCorrelationKey_;
    }
    return safety_.consumeConfigurationStatus(result.status, sourceKey_,
                                              correlationKey);
}

SafetyServiceStatus ConfigurationSafetyIntegrationGate::forward(
    ConfigurationServiceMode mode) {
    const auto correlationKey = nextCorrelationKey_;
    if (nextCorrelationKey_ != std::numeric_limits<std::uint32_t>::max()) {
        ++nextCorrelationKey_;
    }
    return safety_.consumeConfigurationStatus(mode, sourceKey_, correlationKey);
}

}  // namespace fermentation
