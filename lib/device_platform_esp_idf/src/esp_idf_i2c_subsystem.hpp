#pragma once

#include "esp_err.h"

#include "absolute_time_internal.hpp"

namespace device_platform_esp_idf {

// Composition-root lifetime owner for the i2cdev subsystem.  i2cdev owns the
// per-port master bus, device registry, mutexes, and reference counting; this
// class only owns when that subsystem is initialized and shut down.
class EspIdfI2cSubsystem final {
   public:
    EspIdfI2cSubsystem() = default;
    ~EspIdfI2cSubsystem();

    EspIdfI2cSubsystem(const EspIdfI2cSubsystem&) = delete;
    EspIdfI2cSubsystem& operator=(const EspIdfI2cSubsystem&) = delete;
    EspIdfI2cSubsystem(EspIdfI2cSubsystem&&) = delete;
    EspIdfI2cSubsystem& operator=(EspIdfI2cSubsystem&&) = delete;

    [[nodiscard]] esp_err_t begin() noexcept;
    [[nodiscard]] esp_err_t shutdown() noexcept;
    [[nodiscard]] bool initialized() const noexcept {
        return lifecycle_.initialized();
    }

    // Claims the canonical i2cdev port/configuration for one device.  A
    // second device may share the exact configuration, but cannot silently
    // reconfigure the port.
    [[nodiscard]] bool claimPort(int bus, int sda, int scl) noexcept;
    void releasePort(int bus, int sda, int scl) noexcept;

   private:
    internal::I2cLifecycleState lifecycle_;
    internal::SharedI2cPortClaims claims_;
};

}  // namespace device_platform_esp_idf
