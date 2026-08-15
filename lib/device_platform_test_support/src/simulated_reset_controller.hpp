#pragma once

#include <cstdint>

#include "reset_port.hpp"

namespace device_platform_test_support {

class SimulatedResetController final
    : public device_platform::IResetController {
   public:
    [[nodiscard]] device_platform::ResetCauseSnapshot observeBootReset()
        const override;
    [[nodiscard]] device_platform::RestartRequestResult requestRestart()
        override;

    void setBootReset(device_platform::ResetCause cause, bool valid = true,
                      std::uint64_t observationId = 1U);
    void setNextRestartResult(device_platform::RestartRequestResult result);
    [[nodiscard]] std::uint32_t restartRequestCount() const {
        return restartRequestCount_;
    }

   private:
    device_platform::ResetCauseSnapshot snapshot_;
    device_platform::RestartRequestResult nextResult_{
        device_platform::RestartRequestResult::Accepted};
    std::uint32_t restartRequestCount_{0U};
};

}  // namespace device_platform_test_support
