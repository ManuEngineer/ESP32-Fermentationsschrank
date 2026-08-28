#include "esp_idf_i2c_subsystem.hpp"

#include "i2cdev.h"

namespace device_platform_esp_idf {

EspIdfI2cSubsystem::~EspIdfI2cSubsystem() {
    if (initialized_) static_cast<void>(shutdown());
}

esp_err_t EspIdfI2cSubsystem::begin() noexcept {
    if (initialized_) return ESP_OK;
    const esp_err_t status = i2cdev_init();
    if (status == ESP_OK) initialized_ = true;
    return status;
}

esp_err_t EspIdfI2cSubsystem::shutdown() noexcept {
    if (!initialized_) return ESP_OK;
    const esp_err_t status = i2cdev_done();
    if (status == ESP_OK) initialized_ = false;
    return status;
}

}  // namespace device_platform_esp_idf
