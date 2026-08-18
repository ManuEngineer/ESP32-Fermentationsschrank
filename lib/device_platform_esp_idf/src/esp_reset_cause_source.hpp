#pragma once

#include "reset_cause.hpp"

namespace device_platform_esp_idf {

class EspResetCauseSource final : public device_platform::IResetCauseSource {
   public:
    [[nodiscard]] device_platform::ResetCause resetCause()
        const noexcept override;
};

}  // namespace device_platform_esp_idf
