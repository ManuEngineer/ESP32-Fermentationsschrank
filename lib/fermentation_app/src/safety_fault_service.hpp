#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

#include "actuator_plan_types.hpp"
#include "fault_types.hpp"
#include "process_state_machine.hpp"
#include "restart_episode.hpp"
#include "run_commands.hpp"
#include "safety_state_store.hpp"
#include "sensor_quality_snapshot.hpp"

namespace fermentation {

class ActuatorPlanner;
class TemperatureControlApplicationOrchestrator;
struct ConfigurationRecoveryResult;
struct CrossRolePlausibilityContext;
struct SensorSelectionStateView;
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
    SafetyMarkerRecoveryCommitted,
};

enum class ConfigurationSafetyStatus : std::uint8_t {
    Operational,
    ConfigurationRuntimeFailure,
    ConfigurationCommitIndeterminate,
    ConfigurationUnavailable,
    ConfigurationIntegrityFailure,
    Unknown,
};

enum class SafetySensorRole : std::uint8_t {
    CabinetAir,
    Product,
    Cooling,
};

inline constexpr std::uint8_t kMinimumSafetyRecoveryAttempts = 0U;
inline constexpr std::uint8_t kDefaultSafetyRecoveryAttempts = 1U;
inline constexpr std::uint8_t kMaximumSafetyRecoveryAttempts = 2U;
static_assert(kDefaultSafetyRecoveryAttempts >=
                  kMinimumSafetyRecoveryAttempts &&
              kDefaultSafetyRecoveryAttempts <= kMaximumSafetyRecoveryAttempts);

struct SafetyBootResult {
    SafetyServiceStatus status{SafetyServiceStatus::NotStarted};
    RestartBootEvaluation restart;
    bool safeBootRequired{true};
};

struct SafetyResetResult {
    SafetyServiceStatus status{SafetyServiceStatus::SafetyRejected};
    FaultInstanceId targetFault;
    FaultResetEvaluation evaluation;
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

    [[nodiscard]] SafetyServiceStatus consumeProcessMessage(
        ProcessMessage message, std::uint32_t sourceKey,
        std::uint32_t correlationKey);
    [[nodiscard]] SafetyServiceStatus consumeSensorQuality(
        SafetySensorRole role,
        const device_platform::SensorQualitySnapshot& snapshot,
        std::uint32_t sourceKey, std::uint32_t correlationKey);
    [[nodiscard]] SafetyServiceStatus consumeSensorSelectionEvidence(
        const SensorSelectionStateView& selection,
        const CrossRolePlausibilityContext& plausibility,
        std::uint32_t sourceKey, std::uint32_t correlationKey);
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
    [[nodiscard]] FaultResetEvaluation evaluateFaultReset(
        const FaultResetRequest& request,
        const FaultResetAuthorizationEvidence& authorization) const;
    [[nodiscard]] SafetyResetResult resetFault(
        const FaultResetRequest& request,
        const FaultResetAuthorizationEvidence& authorization,
        ActuatorPlanner* planner = nullptr);
    [[nodiscard]] SafetyServiceStatus recoverSafetyStateMarker(
        const FaultResetAuthorizationEvidence& authorization,
        const SafetyMarkerRecoveryEvidence& evidence);
    [[nodiscard]] SafetyServiceStatus requestControlledSafetyRestart(
        FaultInstanceId id, std::uint32_t expectedRevision);
    [[nodiscard]] SafetyServiceStatus advanceStableWindow();
    [[nodiscard]] SafetyServiceStatus requestAuthorizedSafeBootExit(
        const FaultResetAuthorizationEvidence& authorization);
    [[nodiscard]] SafetyServiceStatus requestAuthorizedTechnicalRestart(
        const FaultResetAuthorizationEvidence& authorization,
        FaultInstanceId targetFault, std::uint32_t targetFaultRevision);

    [[nodiscard]] SafetyDisposition disposition() const;
    [[nodiscard]] bool safeBootRequired() const;
    [[nodiscard]] const FaultCore& faultCore() const { return faultCore_; }
    [[nodiscard]] const SafetyStateRecord& record() const { return record_; }

    // Projektion fuer vorhandene #15/#23-Konsumenten; die Projektion ist
    // niemals die Autoritaet.
    void projectTo(RunCommandState& state) const;
    [[nodiscard]] ActuatorSafetyGateInput actuatorGateInput() const;

#if defined(APP_PROFILE_NATIVE)
    // Native-only seam for contract tests. Production code cannot construct or
    // pass positive reset-safety fields; the service remains the authority.
    void injectResetSafetyEvidenceForTesting(FaultInstanceId targetFault,
                                             std::uint32_t targetRevision,
                                             std::uint8_t passedDomains);
    void injectSafeBootSafetyEvidenceForTesting(std::uint8_t passedDomains);
    void invalidateSafetyEvidenceForTesting(FaultResetCheckDomain domain);
    // Native-only seam for the stable Release-1 contract-/injection-only
    // codes (S3-003/004/005/006/007/009, Y4-007) whose real qualified
    // producer does not exist yet. Production code has no raw-raise entry
    // point; every real producer path uses a typed consume*() method that
    // owns its own bounded identity.
    [[nodiscard]] SafetyServiceStatus injectFaultForTesting(
        const FaultRaiseRequest& request);
#endif

   private:
    friend class TemperatureControlApplicationOrchestrator;

    [[nodiscard]] SafetyServiceStatus raiseFault(
        const FaultRaiseRequest& request);

    static constexpr std::size_t kSafetyCheckDomainCount = 4U;

    struct SafetyDomainEvidence {
        std::uint32_t revision{0U};
        std::uint32_t recordRevision{0U};
        bool passed{false};
    };

    struct FaultSafetyEvidenceProjection {
        FaultInstanceId targetFault;
        std::uint32_t targetRevision{0U};
        std::array<SafetyDomainEvidence, kSafetyCheckDomainCount> domains{};
    };

    [[nodiscard]] SafetyServiceStatus persistCoreMutation(
        const FaultCoreSnapshot& before, std::uint32_t sourceKey = 0U,
        std::uint32_t correlationKey = 0U);
    [[nodiscard]] SafetyServiceStatus persistSafeBootLock();
    [[nodiscard]] bool copyCoreToRecord(SafetyStateRecord& candidate) const;
    [[nodiscard]] bool copyCoreToRecord(SafetyStateRecord& candidate,
                                        const FaultCore& core) const;
    [[nodiscard]] SafetyServiceStatus resolveFaultCause(
        FaultCode code, std::uint32_t sourceKey,
        std::optional<std::uint32_t> correlationKey = std::nullopt,
        std::optional<FaultDiagnosticOrigin> requiredOrigin = std::nullopt);
    [[nodiscard]] SafetyServiceStatus finalizePendingSafeBootExit();
    [[nodiscard]] SafetyServiceStatus attemptSafeBootExit(
        std::uint32_t evidenceIdForFailureTracking);
    [[nodiscard]] bool clearSafeBootTrackingFault(FaultCore& core) const;
    void retainRamFailClosed(
        std::uint32_t sourceKey, std::uint32_t correlationKey,
        SafetyMarkerErrorKind kind = SafetyMarkerErrorKind::Unknown);
    [[nodiscard]] static FaultResetAuthorizationLevel requiredAuthorizationFor(
        FaultCode code);
    [[nodiscard]] static std::uint8_t requiredResetCheckDomains(FaultCode code);
    [[nodiscard]] bool resetSafetyEvidenceMatches(
        const FaultResetRequest& request, std::uint32_t targetRevision,
        std::uint8_t requiredDomains) const;
    [[nodiscard]] static std::size_t safetyDomainIndex(
        FaultResetCheckDomain domain);
    void updateSafetyDomainEvidence(FaultResetCheckDomain domain, bool passed);
    void bindFaultSafetyEvidence(FaultInstanceId targetFault,
                                 std::uint32_t targetRevision,
                                 FaultResetCheckDomain domain);
    void bindPersistenceAndIntegrityToCurrentFaults();
    void updateSensorSafetyEvidence(SafetySensorRole role,
                                    std::uint32_t sourceKey, bool passed);
    void updateRequiredSensorRoleEvidence(SafetySensorRole role, bool passed);
    void consumeActuatorPlanEvidence(const ActuatorPlanTickResult& result);
    [[nodiscard]] bool safeBootSafetyEvidenceCurrent() const;
    [[nodiscard]] static std::uint32_t sensorRoleCorrelation(
        SafetySensorRole role);
    [[nodiscard]] bool authorizationIsCurrent(
        const FaultResetAuthorizationEvidence& authorization,
        FaultInstanceId targetFault, std::uint32_t targetRevision,
        FaultResetAuthorizationLevel required) const;
    [[nodiscard]] bool restartEvidenceMatchesCurrentFault(
        const PersistedRestartEvidence& evidence) const;
    [[nodiscard]] SafetyServiceStatus finalizeRestartRequestResult(
        device_platform::RestartRequestResult result);
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
    std::optional<std::uint32_t> pendingAuthorizedSafeBootExitEvidenceId_;
    std::array<SafetyDomainEvidence, kSafetyCheckDomainCount>
        currentSafetyEvidence_{};
    SafetyDomainEvidence cabinetAirSafetyEvidence_{};
    SafetyDomainEvidence coolingSafetyEvidence_{};
    std::array<FaultSafetyEvidenceProjection, kMaximumActiveFaults>
        faultSafetyEvidence_{};
    std::uint32_t nextSafetyEvidenceRevision_{1U};
};

}  // namespace fermentation
