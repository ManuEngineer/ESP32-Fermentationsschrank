#pragma once

#include "reset_cause.hpp"

namespace device_platform_test_support {

class MockResetCauseSource final : public device_platform::IResetCauseSource {
   public:
    explicit MockResetCauseSource(device_platform::ResetCause cause =
                                      device_platform::ResetCause::Unknown)
        : cause_(cause) {}

    [[nodiscard]] device_platform::ResetCause resetCause()
        const noexcept override {
        return cause_;
    }

    void setResetCause(device_platform::ResetCause cause) noexcept {
        cause_ = cause;
    }

   private:
    device_platform::ResetCause cause_;
};

}  // namespace device_platform_test_support
