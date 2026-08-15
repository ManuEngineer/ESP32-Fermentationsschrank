#include "configuration_safety_integration_gate.hpp"

#include <utility>

namespace fermentation {
namespace {

constexpr std::uint32_t kRuntimeFailureCorrelation = 1U;
constexpr std::uint32_t kCommitIndeterminateCorrelation = 2U;
constexpr std::uint32_t kConfigurationUnavailableCorrelation = 3U;
constexpr std::uint32_t kConfigurationIntegrityCorrelation = 4U;
constexpr std::uint32_t kUnknownConfigurationCorrelation = 5U;

std::uint32_t correlationFor(ConfigurationRecoveryStatus status) {
    switch (status) {
        case ConfigurationRecoveryStatus::ConfigurationCommitIndeterminate:
        case ConfigurationRecoveryStatus::BootstrapCommitIndeterminate:
        case ConfigurationRecoveryStatus::
            ConfigurationRecordOutcomeIndeterminate:
            return kCommitIndeterminateCorrelation;
        case ConfigurationRecoveryStatus::ConfigurationIntegrityFailure:
        case ConfigurationRecoveryStatus::UnsupportedNewerConfigurationSchema:
            return kConfigurationIntegrityCorrelation;
        case ConfigurationRecoveryStatus::ConfigurationUnavailable:
        case ConfigurationRecoveryStatus::PersistenceReadFailure:
        case ConfigurationRecoveryStatus::PersistenceCapacityFailure:
        case ConfigurationRecoveryStatus::PersistenceWriteFailure:
        case ConfigurationRecoveryStatus::RuntimePreparationFailure:
            return kConfigurationUnavailableCorrelation;
        default:
            return kUnknownConfigurationCorrelation;
    }
}

std::uint32_t correlationFor(ConfigurationCommitStatus status) {
    switch (status) {
        case ConfigurationCommitStatus::ConfigurationCommitIndeterminate:
            return kCommitIndeterminateCorrelation;
        case ConfigurationCommitStatus::ConfigurationRuntimeFailure:
            return kRuntimeFailureCorrelation;
        case ConfigurationCommitStatus::PersistenceFailure:
        case ConfigurationCommitStatus::CapacityFailure:
            return kConfigurationUnavailableCorrelation;
        default:
            return kUnknownConfigurationCorrelation;
    }
}

std::uint32_t correlationFor(ConfigurationServiceMode mode) {
    switch (mode) {
        case ConfigurationServiceMode::CommitIndeterminate:
            return kCommitIndeterminateCorrelation;
        case ConfigurationServiceMode::RuntimeFailure:
            return kRuntimeFailureCorrelation;
        default:
            return kUnknownConfigurationCorrelation;
    }
}

}  // namespace

ConfigurationSafetyIntegrationResult
ConfigurationSafetyIntegrationGate::boot() {
    return forward(recovery_.boot());
}

ConfigurationSafetyIntegrationResult
ConfigurationSafetyIntegrationGate::forward(
    ConfigurationRecoveryResult result) {
    // Normalize producer variants to the bounded R3 fault domain. The raw
    // enum value is not a new causal identity.
    const auto correlationKey = correlationFor(result.status);
    const auto safetyStatus = safety_.consumeConfigurationRecoveryResult(
        result, sourceKey_, correlationKey);
    return {std::move(result), safetyStatus};
}

SafetyServiceStatus ConfigurationSafetyIntegrationGate::forward(
    ConfigurationCommitResult result) {
    const auto correlationKey = correlationFor(result.status);
    return safety_.consumeConfigurationStatus(result.status, sourceKey_,
                                              correlationKey);
}

SafetyServiceStatus ConfigurationSafetyIntegrationGate::forward(
    ConfigurationServiceMode mode) {
    const auto correlationKey = correlationFor(mode);
    return safety_.consumeConfigurationStatus(mode, sourceKey_, correlationKey);
}

}  // namespace fermentation
