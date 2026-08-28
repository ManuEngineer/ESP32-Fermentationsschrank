#pragma once

#include <cstdint>
#include <memory>
#include <optional>

#include "boot_classification.hpp"
#include "process_state_machine.hpp"
#include "platform_services.hpp"
#include "presentation_state.hpp"
#include "reset_cause.hpp"
#include "time_source.hpp"
#include "time_zone_resolver.hpp"

namespace fermentation {

class ConfigurationBootstrapStore;
class ConfigurationGraphStore;
class ConfigurationMutationCoordinator;
class ConfigurationRecoveryService;
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

class FermentationApplication {
   public:
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
    std::unique_ptr<RunPersistenceCoordinator> runPersistenceCoordinator_;
    std::unique_ptr<RunCommandState> runtimeRunState_;
    std::unique_ptr<RunCommandState> pendingResume_;
    std::unique_ptr<RunCommandState> pendingRecoverySource_;
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
