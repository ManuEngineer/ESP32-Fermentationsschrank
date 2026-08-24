#pragma once

#include "platform_services.hpp"
#include "reset_cause.hpp"
#include "safety_core.hpp"

namespace fermentation {

class ConfigurationService;
struct ConfigurationRecoveryResult;
class RunPersistenceCoordinator;
struct RunPersistenceLoadResult;

class FermentationApplication {
   public:
    [[nodiscard]] bool begin(
        device_platform::IPlatformServices& platformServices,
        ConfigurationService& configurationService,
        const ConfigurationRecoveryResult& configurationRecoveryResult,
        RunPersistenceCoordinator* runPersistenceCoordinator,
        const RunPersistenceLoadResult* runPersistenceLoadResult,
        const device_platform::IResetCauseSource* resetCauseSource = nullptr);

    // The native executable is a platform/application smoke path without an
    // ESP-IDF persistence backend. Keep its existing fail-closed startup seam
    // separate from the real ESP-IDF composition above.
    [[nodiscard]] bool begin(
        device_platform::IPlatformServices& platformServices,
        const device_platform::IResetCauseSource* resetCauseSource = nullptr);
    void update();

    [[nodiscard]] bool ready() const;
    [[nodiscard]] const SafetyCore& safetyCore() const noexcept {
        return safetyCore_;
    }

   private:
    device_platform::IPlatformServices* platformServices_{nullptr};
    SafetyCore safetyCore_;
};

}  // namespace fermentation
