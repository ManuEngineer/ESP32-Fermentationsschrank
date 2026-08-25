#pragma once

#include "platform_services.hpp"
#include "presentation_state.hpp"

namespace fermentation {

class FermentationApplication {
   public:
    [[nodiscard]] bool begin(
        device_platform::IPlatformServices& platformServices,
        const device_platform::IResetCauseSource* resetCauseSource = nullptr);
    void update();

    [[nodiscard]] bool ready() const;
    [[nodiscard]] const PresentationState& presentationState() const noexcept {
        return presentationState_;
    }

   private:
    device_platform::IPlatformServices* platformServices_{nullptr};
    PresentationState presentationState_;
};

}  // namespace fermentation
