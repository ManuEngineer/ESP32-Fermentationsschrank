#pragma once

#include <array>
#include <cstdint>
#include <optional>

#include "actuator_plan_types.hpp"
#include "configuration_recovery_service.hpp"
#include "configuration_service.hpp"
#include "reset_cause.hpp"
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

enum class SafetyDisposition : std::uint8_t {
    Information,
    BlockedImmediateStop,
    SafeBoot,
};

enum class SafetyBootDisposition : std::uint8_t {
    Unresolved,
    Standby,
    ResumeOffer,
    NoActiveRun,
    Completed,
    TerminalFault,
    SafeBoot,
};

enum class RunLoadDisposition : std::uint8_t {
    Standby,
    ResumeOffer,
    NoActiveRun,
    Completed,
    TerminalFault,
    SafeBoot,
};

struct SafetyCoreInput {
    // These flags are evidence from the existing producers, not alternate
    // safety state.  Missing evidence is deliberately not equivalent to true.
    bool bootValidationComplete{false};
    bool configurationValidated{false};
    bool persistenceValidated{false};
    bool sensorEvidenceValidated{false};
    bool explicitActivationRequested{false};
    bool plannerEvidenceValidated{false};

    std::optional<ConfigurationRecoveryStatus> configurationRecoveryStatus;
    std::optional<ConfigurationSafetyProducer> configurationProducer;
    std::optional<ConfigurationServiceMode> configurationServiceMode;
    std::optional<ConfigurationCommitStatus> configurationCommitStatus;

    std::optional<RunPersistenceLoadStatus> persistenceLoadStatus;
    const RunPersistenceSnapshot* persistenceSnapshot{nullptr};
    RunPersistenceCoordinatorState persistenceCoordinatorState{
        RunPersistenceCoordinatorState::Uninitialized};
    // Post-commit/post-FSM evidence from the existing application path. These
    // are required before a resume offer can become an Allowed gate.
    std::optional<RunPersistenceResultStatus> resumePersistenceResult;
    bool processActivationApplied{false};

    const device_platform::SensorQualitySnapshot* peltierSensor{nullptr};
    const SensorSelectionRuntimeState* sensorSelectionRuntime{nullptr};
    bool requestWatchdogTripped{false};
};

struct SafetyEvaluation {
    ActuatorSafetyGateInput gate;
    SafetyDisposition disposition{SafetyDisposition::SafeBoot};
    SafetyBootDisposition bootDisposition{SafetyBootDisposition::SafeBoot};
    FaultCode faultCode{FaultCode::None};
    bool acknowledged{false};
    device_platform::ResetCause resetCause{
        device_platform::ResetCause::Unknown};
};

class ActuatorPlanner;

class SafetyCore final {
   public:
    static constexpr std::size_t kFaultCount = 8U;

    SafetyCore() noexcept = default;

    void beginBoot(device_platform::ResetCause resetCause) noexcept;

    [[nodiscard]] SafetyEvaluation evaluate(const SafetyCoreInput& input);

    // Acknowledgement changes presentation state only. It never changes the
    // gate or the active fault condition.
    void acknowledge(FaultCode code) noexcept;

    // The existing #23 RAM-only reset is the sole watchdog-clear path. The
    // caller must supply fresh evidence from the existing safety/planner path.
    [[nodiscard]] bool resetRequestWatchdog(ActuatorPlanner& planner,
                                            std::uint64_t nowMonotonicMillis,
                                            bool freshSafetyEvidence);

    [[nodiscard]] FaultCode activeFault() const noexcept {
        return activeFault_;
    }
    [[nodiscard]] bool isAcknowledged() const noexcept { return acknowledged_; }
    [[nodiscard]] const SafetyEvaluation& lastEvaluation() const noexcept {
        return lastEvaluation_;
    }
    [[nodiscard]] device_platform::ResetCause resetCause() const noexcept {
        return resetCause_;
    }

    [[nodiscard]] static std::uint16_t wireValue(FaultCode code) noexcept {
        return static_cast<std::uint16_t>(code);
    }

    [[nodiscard]] static bool isR1ResumeEligible(
        const RunPersistenceSnapshot& snapshot) noexcept;
    [[nodiscard]] static RunLoadDisposition classifyRunLoad(
        RunPersistenceLoadStatus status,
        const RunPersistenceSnapshot* snapshot) noexcept;

   private:
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
    [[nodiscard]] static SafetyDisposition dispositionForFault(
        FaultCode code) noexcept;
    [[nodiscard]] bool canClearFault(
        const SafetyCoreInput& input,
        RunLoadDisposition loadDisposition) const noexcept;

    void setFault(FaultCode code) noexcept;
    void clearFault() noexcept;

    device_platform::ResetCause resetCause_{
        device_platform::ResetCause::Unknown};
    FaultCode activeFault_{FaultCode::None};
    bool acknowledged_{false};
    SafetyEvaluation lastEvaluation_{};
};

}  // namespace fermentation
