#pragma once

#include <cstdint>
#include <optional>

#include "actuator_plan_types.hpp"
#include "fault_types.hpp"
#include "restart_episode.hpp"
#include "run_commands.hpp"
#include "safety_state_store.hpp"

namespace fermentation {

class ActuatorPlanner;
struct ConfigurationRecoveryResult;
enum class ConfigurationServiceMode : std::uint8_t;
enum class ConfigurationCommitStatus : std::uint8_t;
enum class ConfigurationRecoveryStatus : std::uint8_t;

enum class SafetyServiceStatus : std::uint8_t {
    Ready,
    NotStarted,
    FactoryBootstrapRequired,
    PersistentReadFailed,
    PersistentWriteFailed,
    InvalidFault,
    FaultCapacityReached,
    StaleFault,
    SafetyRejected,
    ResetCommitted,
    ResetBootRejected,
    ResetBootOutcomeUnknown,
};

enum class ConfigurationSafetyStatus : std::uint8_t {
    Operational,
    ConfigurationRuntimeFailure,
    ConfigurationCommitIndeterminate,
    ConfigurationUnavailable,
    ConfigurationIntegrityFailure,
    Unknown,
};

struct SafetyBootResult {
    SafetyServiceStatus status{SafetyServiceStatus::NotStarted};
    RestartBootEvaluation restart;
    bool safeBootRequired{true};
};

struct SafetyResetResult {
    SafetyServiceStatus status{SafetyServiceStatus::SafetyRejected};
    FaultInstanceId targetFault;
};

// Anwendungsseitige #24-Orchestrierungsgrenze. Sie besitzt den Faultkern und
// den SafetyStateRecord; Plattform, Zeit und Journal werden nur ueber die
// bestehenden abstrakten Ports konsumiert.
class SafetyFaultService final {
   public:
    SafetyFaultService(device_platform::IStateStore& store,
                       device_platform::IResetController& resetController,
                       device_platform::ITimeSource& timeSource,
                       device_platform::IEventJournal* journal = nullptr);

    [[nodiscard]] SafetyServiceStatus begin(
        const FactoryNewSafetyProof& factoryProof = {});
    [[nodiscard]] SafetyBootResult evaluateBoot();

    [[nodiscard]] SafetyServiceStatus raiseFault(
        const FaultRaiseRequest& request);
    [[nodiscard]] SafetyServiceStatus consumeConfigurationStatus(
        ConfigurationSafetyStatus status, std::uint32_t sourceKey,
        std::uint32_t correlationKey);
    [[nodiscard]] SafetyServiceStatus consumeConfigurationStatus(
        ConfigurationServiceMode status, std::uint32_t sourceKey,
        std::uint32_t correlationKey);
    [[nodiscard]] SafetyServiceStatus consumeConfigurationStatus(
        ConfigurationCommitStatus status, std::uint32_t sourceKey,
        std::uint32_t correlationKey);
    [[nodiscard]] SafetyServiceStatus consumeConfigurationStatus(
        ConfigurationRecoveryStatus status, std::uint32_t sourceKey,
        std::uint32_t correlationKey);
    [[nodiscard]] SafetyServiceStatus consumeConfigurationRecoveryResult(
        const ConfigurationRecoveryResult& result, std::uint32_t sourceKey,
        std::uint32_t correlationKey);
    [[nodiscard]] SafetyServiceStatus consumeWatchdogEvidence(
        const ActuatorWatchdogFaultEvidence& evidence);
    [[nodiscard]] SafetyServiceStatus acknowledgeFault(
        FaultInstanceId id, std::uint32_t expectedRevision);
    [[nodiscard]] SafetyServiceStatus clearFaultCause(
        FaultInstanceId id, std::uint32_t expectedRevision);
    [[nodiscard]] SafetyResetResult resetFault(
        FaultInstanceId id, std::uint32_t expectedRevision,
        ActuatorPlanner* planner = nullptr);
    [[nodiscard]] SafetyServiceStatus requestControlledSafetyRestart(
        FaultInstanceId id, std::uint32_t expectedRevision);
    [[nodiscard]] SafetyServiceStatus advanceStableWindow(bool stable);

    [[nodiscard]] SafetyDisposition disposition() const;
    [[nodiscard]] bool safeBootRequired() const;
    [[nodiscard]] const FaultCore& faultCore() const { return faultCore_; }
    [[nodiscard]] const SafetyStateRecord& record() const { return record_; }

    // Projektion fuer vorhandene #15/#23-Konsumenten; die Projektion ist
    // niemals die Autoritaet.
    void projectTo(RunCommandState& state) const;
    [[nodiscard]] ActuatorSafetyGateInput actuatorGateInput() const;

   private:
    [[nodiscard]] SafetyServiceStatus persistCoreMutation(
        const FaultCoreSnapshot& before);
    [[nodiscard]] SafetyServiceStatus persistSafeBootLock();
    [[nodiscard]] bool copyCoreToRecord(SafetyStateRecord& candidate) const;
    [[nodiscard]] bool copyCoreToRecord(SafetyStateRecord& candidate,
                                        const FaultCore& core) const;
    void recordEvent(FaultEventType type, const FaultRecord* fault,
                     bool accepted, std::uint32_t episodeId = 0U,
                     std::uint32_t restartEvidenceId = 0U) const;

    SafetyStateStore stateStore_;
    device_platform::IResetController& resetController_;
    device_platform::ITimeSource& timeSource_;
    device_platform::IEventJournal* journal_{nullptr};
    FaultCore faultCore_;
    RestartEpisodeCoordinator restartEpisode_;
    SafetyStateRecord record_;
    bool started_{false};
    bool configurationGateQualified_{false};
};

}  // namespace fermentation
