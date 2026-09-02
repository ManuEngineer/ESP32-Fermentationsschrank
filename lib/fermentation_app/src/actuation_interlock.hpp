#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>

#include "actuator_plan_types.hpp"
#include "boot_classification.hpp"
#include "configuration_recovery_service.hpp"
#include "configuration_service.hpp"
#include "run_persistence_coordinator.hpp"
#include "sensor_quality_snapshot.hpp"
#include "sensor_selection_types.hpp"

namespace fermentation {

enum class FaultCode : std::uint16_t {
    None = 0x0000U,
    ConfigurationRuntimeFailure = 0x0101U,
    ConfigurationUnavailable = 0x0102U,
    ConfigurationIntegrityFailure = 0x0103U,
    ConfigurationCommitIndeterminate = 0x0104U,
    RunPersistenceUntrusted = 0x0201U,
    SafetySensorUnavailable = 0x0301U,
    ActuatorRequestWatchdog = 0x0401U,
    SystemProducerUnknown = 0x0501U,
};

enum class SafetyActivationKind : std::uint8_t {
    None,
    FreshStart,
    Resume,
};

class ActuatorPlanner;

// A complete, current producer snapshot. This type deliberately contains no
// persisted snapshot or interlock-owned acknowledgement/latch state.
struct ActuationEvidence {
    bool bootValidationComplete{false};
    bool configurationValidated{false};
    bool persistenceValidated{false};
    bool sensorEvidenceValidated{false};
    bool explicitActivationRequested{false};
    bool plannerEvidenceValidated{false};
    SafetyActivationKind activationKind{SafetyActivationKind::None};

    std::optional<ConfigurationRecoveryStatus> configurationRecoveryStatus;
    std::optional<ConfigurationSafetyProducer> configurationProducer;
    std::optional<ConfigurationServiceMode> configurationServiceMode;
    std::optional<ConfigurationCommitStatus> configurationCommitStatus;

    std::optional<RunPersistenceLoadStatus> persistenceLoadStatus;
    RunPersistenceCoordinatorState persistenceCoordinatorState{
        RunPersistenceCoordinatorState::Uninitialized};
    RunLoadDisposition loadDisposition{RunLoadDisposition::SafeBoot};
    std::optional<RunPersistenceResultStatus> activationPersistenceResult;
    bool processActivationApplied{false};

    const device_platform::SensorQualitySnapshot* peltierSensor{nullptr};
    const SensorSelectionRuntimeState* sensorSelectionRuntime{nullptr};
    // The planner remains the #23 watchdog authority. The interlock reads its
    // current latch but never stores a copy of it.
    const ActuatorPlanner* actuatorPlanner{nullptr};
};

struct ActuationEvaluation {
    ActuatorSafetyGateStatus permission{ActuatorSafetyGateStatus::Unresolved};
    FaultCode faultCode{FaultCode::None};
};

class ActuationInterlock final {
   public:
    static constexpr std::size_t kFaultCount = 8U;

    [[nodiscard]] static ActuationEvaluation evaluate(
        const ActuationEvidence& evidence);

    [[nodiscard]] static bool resetRequestWatchdog(
        ActuatorPlanner& planner, std::uint64_t nowMonotonicMillis,
        const ActuationEvidence& evidence);

    [[nodiscard]] static std::uint16_t wireValue(FaultCode code) noexcept {
        return static_cast<std::uint16_t>(code);
    }

   private:
    using FaultMask = std::uint16_t;

    [[nodiscard]] static bool isKnown(
        ConfigurationRecoveryStatus status) noexcept;
    [[nodiscard]] static bool isKnown(
        ConfigurationSafetyProducer producer) noexcept;
    [[nodiscard]] static bool isKnown(ConfigurationServiceMode mode) noexcept;
    [[nodiscard]] static bool isKnown(
        ConfigurationCommitStatus status) noexcept;
    [[nodiscard]] static bool isKnown(RunPersistenceLoadStatus status) noexcept;
    [[nodiscard]] static bool isKnown(
        RunPersistenceCoordinatorState state) noexcept;

    [[nodiscard]] static FaultMask faultBit(FaultCode code) noexcept;
    [[nodiscard]] static bool hasFault(FaultMask mask, FaultCode code) noexcept;
    [[nodiscard]] static FaultCode primaryFault(FaultMask mask) noexcept;
    [[nodiscard]] static bool activationEvidenceComplete(
        const ActuationEvidence& evidence, SafetyActivationKind expectedKind,
        RunLoadDisposition loadDisposition) noexcept;
};

}  // namespace fermentation
