#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

#include "boot_classification.hpp"
#include "process_state_machine.hpp"
#include "platform_services.hpp"
#include "presentation_state.hpp"
#include "reset_cause.hpp"
#include "state_store.hpp"
#include "sensor_selection.hpp"
#include "time_source.hpp"
#include "time_zone_resolver.hpp"
#include "application_run_identity.hpp"
#include "fermentation_ui_commands.hpp"

namespace fermentation {

class ConfigurationBootstrapStore;
class ConfigurationGraphStore;
class ConfigurationMutationCoordinator;
class ConfigurationRecoveryService;
struct ConfigurationRecoveryResult;
class ConfigurationService;
class RunPersistenceCoordinator;
enum class ConfigurationRecoveryStatus : std::uint8_t;

#if defined(APP_ISSUE_90_SLICE7_HARNESS)
namespace issue_90_slice7 {
class Harness;
}
#endif

enum class ApplicationLifecycleState : std::uint8_t {
    Initializing,
    Ready,
    ServiceRequired,
};

// Already evaluated evidence supplied by the owning application/orchestrator
// boundary. This is a composition input, not a UI-controlled safety or
// sensor decision and contains no fallback policy.
struct FermentationApplicationOwningEvidence {
    bool safetyAllowsStart{false};
    bool safetyAllowsCooling{false};
    bool airSensorValid{false};
    bool coolingSensorValid{false};
    bool productSensorValid{false};
};

class FermentationApplication {
   public:
    FermentationApplication() = default;
    FermentationApplication(const FermentationApplication&) = delete;
    FermentationApplication& operator=(const FermentationApplication&) = delete;
    FermentationApplication(FermentationApplication&&) = delete;
    FermentationApplication& operator=(FermentationApplication&&) = delete;
    ~FermentationApplication();

    [[nodiscard]] bool begin(
        device_platform::IPlatformServices& platformServices,
        const device_platform::IResetCauseSource* resetCauseSource = nullptr);
    [[nodiscard]] bool begin(
        device_platform::IPlatformServices& platformServices,
        device_platform::IStateStore& store,
        const device_platform::ITimeZoneResolver& timeZoneResolver,
        const device_platform::IResetCauseSource* resetCauseSource = nullptr);
    [[nodiscard]] bool begin(
        device_platform::IPlatformServices& platformServices,
        device_platform::IStateStore& store,
        const device_platform::ITimeZoneResolver& timeZoneResolver,
        const device_platform::ITimeSource& timeSource,
        const device_platform::IResetCauseSource* resetCauseSource = nullptr);
    void update();

    [[nodiscard]] bool ready() const;
    [[nodiscard]] ApplicationLifecycleState lifecycleState() const noexcept {
        return lifecycleState_;
    }
    [[nodiscard]] std::optional<ProcessRuntimeState> publishedProcessState()
        const;
    [[nodiscard]] const PresentationState& presentationState() const noexcept {
        return presentationState_;
    }
    [[nodiscard]] std::optional<RecoveryDisposition> recoveryDisposition()
        const noexcept {
        return recoveryDisposition_;
    }

    // These composition points are shared by local and future Web adapters.
    // They allocate identity, resolve current configuration, and return an
    // already application-bound owning request. The adapter supplies values
    // and expected revisions only.
    [[nodiscard]] FermentationApplicationRequestResult prepareStartProgram(
        const FermentationUiCommandContext& context,
        const FermentationUiStartProgramIntent& intent,
        const FermentationApplicationOwningEvidence& evidence);
    [[nodiscard]] FermentationApplicationRequestResult
    prepareStartManualHolding(
        const FermentationUiCommandContext& context,
        const FermentationUiStartManualHoldingIntent& intent,
        const FermentationApplicationOwningEvidence& evidence);
    [[nodiscard]] FermentationApplicationRequestResult prepareStop(
        const FermentationUiCommandContext& context,
        const FermentationUiStopRunIntent& intent,
        const FermentationApplicationOwningEvidence& evidence);
    [[nodiscard]] FermentationApplicationRequestResult prepareCompletion(
        const FermentationUiCommandContext& context,
        const FermentationUiCompleteRunIntent& intent,
        const FermentationApplicationOwningEvidence& evidence);

    // Existing configuration recovery remains the authorization owner. This
    // application entry point composes its FactoryResetCompleted result with
    // the run-persistence epoch handoff before publishing the new runtime.
    [[nodiscard]] ConfigurationRecoveryResult beginAuthorizedFactoryReset();

    // Explicit R1 selected-fallback action.  The command carries only the
    // app-owned confirmation/revision contract; fresh sensor/planner evidence
    // is supplied by the owning application/orchestrator boundary, never by
    // a UI transport.
    // The orchestrator publishes fresh owning evidence at the application
    // boundary. UI/Web commands never carry sensor, planner or safety data.
    void publishOwningRecoveryEvidence(
        const CrossRolePlausibilityContext& evidence);
    [[nodiscard]] RunPersistenceResult resumeFallback(
        const FermentationUiResumeFallbackCommand& command);

   private:
#if defined(APP_ISSUE_90_SLICE7_HARNESS)
    friend class issue_90_slice7::Harness;
#endif
    void requireService(FaultCode faultCode,
                        bool applicationAllocationFailure = false) noexcept;
    [[nodiscard]] bool publishStandby();
    [[nodiscard]] bool beginPersistent(
        device_platform::IPlatformServices& platformServices,
        device_platform::IStateStore& store,
        const device_platform::ITimeZoneResolver& timeZoneResolver,
        const device_platform::ITimeSource* timeSource,
        const device_platform::IResetCauseSource* resetCauseSource);
    [[nodiscard]] bool processBootClassification(
        BootClassification classification,
        const RunPersistenceSnapshot* snapshot,
        const RunCheckpointTime& bootTime);
    [[nodiscard]] bool prepareResumeOffer(
        const RunPersistenceSnapshot* snapshot);
    [[nodiscard]] bool prepareFallbackSelection(
        const RunPersistenceSnapshot* snapshot);
    [[nodiscard]] bool evaluateCurrentRecovery(
        const RunPersistenceSnapshot* snapshot,
        const RunCheckpointTime& bootTime);
    [[nodiscard]] bool processTerminalClassification(
        BootClassification classification,
        const RunPersistenceSnapshot* snapshot,
        const RunCheckpointTime& bootTime);
    [[nodiscard]] RunCheckpointTime currentCheckpointTime() const noexcept;
    [[nodiscard]] bool enterRecoveryEvaluationRamState(
        const RunCommandState& source);
    void reevaluateWaitingForTrustedTime();

    device_platform::IPlatformServices* platformServices_{nullptr};
    const device_platform::ITimeSource* timeSource_{nullptr};
    std::unique_ptr<ConfigurationBootstrapStore> bootstrapStore_;
    std::unique_ptr<ConfigurationMutationCoordinator> mutationCoordinator_;
    std::unique_ptr<ConfigurationGraphStore> graphStore_;
    std::unique_ptr<ConfigurationService> configurationService_;
    std::unique_ptr<ConfigurationRecoveryService> configurationRecoveryService_;
    std::unique_ptr<RunPersistenceCoordinator> runPersistenceCoordinator_;
    std::unique_ptr<ApplicationRunIdentity> runIdentity_;
    device_platform::IStateStore* stateStore_{nullptr};
    std::optional<device_platform::StorageEpoch> storageEpoch_;
    std::unique_ptr<RunCommandState> runtimeRunState_;
    std::unique_ptr<RunCommandState> pendingResume_;
    std::unique_ptr<RunCommandState> pendingFallbackResume_;
    std::unique_ptr<RunCommandState> pendingRecoverySource_;
    std::optional<CrossRolePlausibilityContext> owningRecoveryEvidence_;
    std::optional<RunPersistenceLoadStatus> persistenceLoadStatus_;
    RunLoadDisposition loadDisposition_{RunLoadDisposition::SafeBoot};
    std::optional<RecoveryDisposition> recoveryDisposition_;
#if defined(APP_ISSUE_90_SLICE7_HARNESS)
    std::optional<ConfigurationRecoveryStatus> configurationRecoveryStatus_;
#endif
    ApplicationLifecycleState lifecycleState_{
        ApplicationLifecycleState::Initializing};
    PresentationState presentationState_;
};

}  // namespace fermentation
