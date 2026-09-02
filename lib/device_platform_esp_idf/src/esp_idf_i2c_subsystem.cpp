#include "esp_idf_i2c_subsystem.hpp"

#include "i2cdev.h"

namespace device_platform_esp_idf {

EspIdfI2cSubsystem::~EspIdfI2cSubsystem() {
    if (initialized()) static_cast<void>(shutdown());
}

esp_err_t EspIdfI2cSubsystem::begin() noexcept {
    return lifecycle_.begin([]() { return i2cdev_init(); });
}

esp_err_t EspIdfI2cSubsystem::shutdown() noexcept {
    if (claims_.hasClaims()) return ESP_ERR_INVALID_STATE;
    return lifecycle_.shutdown([]() { return i2cdev_done(); });
}

bool EspIdfI2cSubsystem::claimPort(const int bus, const int sda,
                                   const int scl) noexcept {
    if (!initialized()) return false;
    return claims_.claim({bus, sda, scl});
}

void EspIdfI2cSubsystem::releasePort(const int bus, const int sda,
                                     const int scl) noexcept {
    static_cast<void>(claims_.release({bus, sda, scl}));
}

}  // namespace device_platform_esp_idf
