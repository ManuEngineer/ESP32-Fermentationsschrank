#pragma once

#include <cstdint>
#include <ctime>
#include <memory>
#include <optional>

#include "esp_err.h"

#include "esp_idf_i2c_subsystem.hpp"

namespace device_platform_esp_idf {

struct Ds3231SnRtcConfig {
    // `false` is the supported NTP-only profile.  The remaining values are
    // intentionally unusable until a verified board profile supplies them.
    bool present{false};
    int bus{0};
    int sdaPin{-1};
    int sclPin{-1};
    std::uint8_t address{0x68U};
};

class Ds3231SnRtcAdapter final {
   public:
    explicit Ds3231SnRtcAdapter(EspIdfI2cSubsystem& i2c) noexcept;
    ~Ds3231SnRtcAdapter();

    Ds3231SnRtcAdapter(const Ds3231SnRtcAdapter&) = delete;
    Ds3231SnRtcAdapter& operator=(const Ds3231SnRtcAdapter&) = delete;
    Ds3231SnRtcAdapter(Ds3231SnRtcAdapter&&) = delete;
    Ds3231SnRtcAdapter& operator=(Ds3231SnRtcAdapter&&) = delete;

    // Disabled RTC is a valid product profile and returns success.  A
    // present RTC is only usable after the configured i2cdev descriptor and
    // device probe succeed; trust is evaluated separately by readTrustedUtc.
    [[nodiscard]] esp_err_t initialize(
        const Ds3231SnRtcConfig& config) noexcept;
    [[nodiscard]] esp_err_t shutdown() noexcept;

    [[nodiscard]] bool present() const noexcept { return config_.present; }
    [[nodiscard]] bool initialized() const noexcept { return initialized_; }

    [[nodiscard]] std::optional<std::int64_t> readTrustedUtc() noexcept;
    [[nodiscard]] bool synchronizeFromSystemUtc(
        std::int64_t utcUnixSeconds) noexcept;

   private:
    struct RawRegisters {
        std::uint8_t calendar[7]{};
        std::uint8_t control{0U};
        std::uint8_t status{0U};
    };

    struct RtcDevice;

    [[nodiscard]] esp_err_t readRaw(RawRegisters& registers) noexcept;
    [[nodiscard]] esp_err_t writeControl(std::uint8_t value) noexcept;
    [[nodiscard]] bool ensureHealthControls() noexcept;
    [[nodiscard]] bool validateRaw(
        const RawRegisters& registers) const noexcept;
    [[nodiscard]] bool validateCalendar(
        const std::uint8_t (&calendar)[7]) const noexcept;
    [[nodiscard]] bool readLibraryTimeMatchesRaw(
        const RawRegisters& registers) noexcept;
    [[nodiscard]] std::optional<std::int64_t> rawCalendarToUnix(
        const std::uint8_t (&calendar)[7]) const noexcept;
    [[nodiscard]] bool unixToTm(std::int64_t utcUnixSeconds,
                                ::tm& value) const noexcept;

    EspIdfI2cSubsystem& i2c_;
    Ds3231SnRtcConfig config_{};
    std::unique_ptr<RtcDevice> device_;
    bool initialized_{false};
};

}  // namespace device_platform_esp_idf
