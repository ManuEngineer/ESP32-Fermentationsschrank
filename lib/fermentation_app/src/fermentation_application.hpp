#pragma once

#include "platform_services.hpp"
#include "reset_cause.hpp"
#include "safety_core.hpp"

namespace fermentation {

class FermentationApplication {
   public:
    [[nodiscard]] bool begin(
        device_platform::IPlatformServices& platformServices,
        device_platform::IResetCauseSource* resetCauseSource = nullptr);
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
