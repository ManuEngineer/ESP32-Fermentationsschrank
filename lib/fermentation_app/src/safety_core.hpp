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

enum class SafetyActivationKind : std::uint8_t {
    None,
    FreshStart,
    Resume,
};

class ActuatorPlanner;

struct SafetyCoreInput {
    // These flags are evidence from the existing producers, not alternate
    // safety state.  Missing evidence is deliberately not equivalent to true.
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
    const RunPersistenceSnapshot* persistenceSnapshot{nullptr};
    RunPersistenceCoordinatorState persistenceCoordinatorState{
        RunPersistenceCoordinatorState::Uninitialized};
    // Post-commit/post-FSM evidence from the existing application path. These
    // are required before a fresh start or resume can become an Allowed gate.
    std::optional<RunPersistenceResultStatus> activationPersistenceResult;
    bool processActivationApplied{false};

    const device_platform::SensorQualitySnapshot* peltierSensor{nullptr};
    const SensorSelectionRuntimeState* sensorSelectionRuntime{nullptr};
    // The Planner is the #23 watchdog authority. SafetyCore reads its
    // existing RAM state; callers cannot inject a replacement bool.
    const ActuatorPlanner* actuatorPlanner{nullptr};
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
    // method validates the supplied current producer evidence and the actual
    // Planner latch before applying the existing reset operation.
    [[nodiscard]] bool resetRequestWatchdog(ActuatorPlanner& planner,
                                            std::uint64_t nowMonotonicMillis,
                                            const SafetyCoreInput& input);

    [[nodiscard]] FaultCode activeFault() const noexcept {
        return primaryFault(activeFaultMask_);
    }
    [[nodiscard]] bool isAcknowledged() const noexcept {
        return isAcknowledged(activeFault());
    }
    [[nodiscard]] bool isAcknowledged(FaultCode code) const noexcept;
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
    using FaultMask = std::uint16_t;
    enum class UnknownProducerSource : std::uint8_t {
        ConfigurationServiceMode,
        ConfigurationCommitStatus,
        ConfigurationRecoveryStatus,
        ConfigurationSafetyProducer,
        PersistenceLoadStatus,
        PersistenceCoordinatorState,
    };
    using UnknownProducerSourceMask = std::uint8_t;

    [[nodiscard]] static FaultMask faultBit(FaultCode code) noexcept;
    [[nodiscard]] static UnknownProducerSourceMask unknownProducerSourceBit(
        UnknownProducerSource source) noexcept;
    [[nodiscard]] static bool hasFault(FaultMask mask, FaultCode code) noexcept;
    [[nodiscard]] static FaultCode primaryFault(FaultMask mask) noexcept;
    [[nodiscard]] static bool activationEvidenceComplete(
        const SafetyCoreInput& input, RunLoadDisposition loadDisposition,
        SafetyActivationKind expectedKind) noexcept;
    [[nodiscard]] bool canClearFault(
        FaultCode code, const SafetyCoreInput& input,
        RunLoadDisposition loadDisposition) const noexcept;

    void setFault(FaultCode code) noexcept;
    void clearFault(FaultCode code) noexcept;

    device_platform::ResetCause resetCause_{
        device_platform::ResetCause::Unknown};
    FaultMask activeFaultMask_{0U};
    FaultMask acknowledgedFaultMask_{0U};
    UnknownProducerSourceMask unknownProducerSources_{0U};
    SafetyEvaluation lastEvaluation_{};
};

}  // namespace fermentation
