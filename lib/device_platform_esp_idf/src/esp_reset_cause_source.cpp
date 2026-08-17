#include "esp_reset_cause_source.hpp"

#include "esp_system.h"

namespace device_platform_esp_idf {

device_platform::ResetCause EspResetCauseSource::resetCause() const noexcept {
    switch (esp_reset_reason()) {
        case ESP_RST_UNKNOWN:
            return device_platform::ResetCause::Unknown;
        case ESP_RST_POWERON:
            return device_platform::ResetCause::PowerOn;
        case ESP_RST_EXT:
            return device_platform::ResetCause::External;
        case ESP_RST_SW:
            return device_platform::ResetCause::Software;
        case ESP_RST_PANIC:
            return device_platform::ResetCause::Panic;
        case ESP_RST_INT_WDT:
            return device_platform::ResetCause::InterruptWatchdog;
        case ESP_RST_TASK_WDT:
            return device_platform::ResetCause::TaskWatchdog;
        case ESP_RST_WDT:
            return device_platform::ResetCause::Watchdog;
        case ESP_RST_DEEPSLEEP:
            return device_platform::ResetCause::DeepSleep;
        case ESP_RST_BROWNOUT:
            return device_platform::ResetCause::Brownout;
        case ESP_RST_SDIO:
            return device_platform::ResetCause::Sdio;
        case ESP_RST_USB:
            return device_platform::ResetCause::Usb;
        case ESP_RST_JTAG:
            return device_platform::ResetCause::Jtag;
        case ESP_RST_EFUSE:
            return device_platform::ResetCause::Efuse;
        case ESP_RST_PWR_GLITCH:
            return device_platform::ResetCause::PowerGlitch;
        case ESP_RST_CPU_LOCKUP:
            return device_platform::ResetCause::CpuLockup;
    }
    return device_platform::ResetCause::Other;
}

}  // namespace device_platform_esp_idf
