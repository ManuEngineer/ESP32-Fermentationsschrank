#pragma once

#include <cstdint>

#include "reset_port.hpp"

namespace device_platform_test_support {

class SimulatedResetController final : public device_platform::IResetController {
   public:
    [[nodiscard]] device_platform::ResetCauseSnapshot observeBootReset()
        const override;
    [[nodiscard]] device_platform::ControlledRestartResult requestRestart(
        const device_platform::ControlledRestartRequest& request) override;

    void setBootReset(device_platform::ResetCause cause,
                      bool valid = true,
                      std::uint64_t observationId = 1U);
    void setNextRestartResult(
        device_platform::ControlledRestartResult result);
    [[nodiscard]] std::uint32_t restartRequestCount() const {
        return restartRequestCount_;
    }
    [[nodiscard]] device_platform::ControlledRestartPurpose lastPurpose() const {
        return lastPurpose_;
    }

   private:
    device_platform::ResetCauseSnapshot snapshot_;
    device_platform::ControlledRestartResult nextResult_{
        device_platform::ControlledRestartResult::Accepted};
    device_platform::ControlledRestartPurpose lastPurpose_{
        device_platform::ControlledRestartPurpose::ControlledSafetyRestart};
    std::uint32_t restartRequestCount_{0U};
};

}  // namespace device_platform_test_support
