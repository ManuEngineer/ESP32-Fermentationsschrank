#include "ds3231_sn_rtc_adapter.hpp"

#include <array>
#include <ctime>

#include "ds3231.h"
#include "i2cdev.h"

namespace device_platform_esp_idf {
namespace {

constexpr std::uint8_t kRtcAddress = 0x68U;
constexpr std::uint8_t kTimeRegister = 0x00U;
constexpr std::uint8_t kControlRegister = 0x0EU;
constexpr std::uint8_t kStatusRegister = 0x0FU;
constexpr std::uint8_t kEoscMask = 0x80U;
constexpr std::uint8_t kRsMask = 0x18U;
constexpr std::uint8_t kOsfMask = 0x80U;
constexpr std::uint8_t kEn32khzMask = 0x08U;

bool validBcd(const std::uint8_t value, const std::uint8_t maximum) {
    if ((value & 0x0FU) > 9U || ((value >> 4U) & 0x0FU) > 9U) return false;
    const auto decoded =
        static_cast<unsigned>((value >> 4U) * 10U + (value & 0x0FU));
    return decoded <= maximum;
}

std::uint8_t decodeBcd(const std::uint8_t value) {
    return static_cast<std::uint8_t>((value >> 4U) * 10U + (value & 0x0FU));
}

bool leapYear(const int year) {
    return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
}

std::uint8_t daysInMonth(const int year, const std::uint8_t month) {
    constexpr std::uint8_t days[] = {31U, 28U, 31U, 30U, 31U, 30U,
                                     31U, 31U, 30U, 31U, 30U, 31U};
    if (month == 2U && leapYear(year)) return 29U;
    return days[month - 1U];
}

// Howard Hinnant's civil-calendar conversion, restricted by the R1 gate to
// 2000..2099.  It avoids timezone and libc range/locale behavior entirely.
std::int64_t daysFromCivil(const int year, const unsigned month,
                           const unsigned day) {
    const int adjustedYear = year - (month <= 2U ? 1 : 0);
    const int era =
        (adjustedYear >= 0 ? adjustedYear : adjustedYear - 399) / 400;
    const unsigned yearOfEra = static_cast<unsigned>(adjustedYear - era * 400);
    const unsigned marchBasedMonth = month > 2U ? month - 3U : month + 9U;
    const unsigned dayOfYear = (153U * marchBasedMonth + 2U) / 5U + day - 1U;
    const unsigned dayOfEra =
        yearOfEra * 365U + yearOfEra / 4U - yearOfEra / 100U + dayOfYear;
    return static_cast<std::int64_t>(era) * 146097LL +
           static_cast<std::int64_t>(dayOfEra) - 719468LL;
}

}  // namespace

struct Ds3231SnRtcAdapter::RtcDevice {
    i2c_dev_t value{};
};

Ds3231SnRtcAdapter::Ds3231SnRtcAdapter(EspIdfI2cSubsystem& i2c) noexcept
    : i2c_(i2c) {}

Ds3231SnRtcAdapter::~Ds3231SnRtcAdapter() { static_cast<void>(shutdown()); }

esp_err_t Ds3231SnRtcAdapter::initialize(
    const Ds3231SnRtcConfig& config) noexcept {
    if (initialized_) return ESP_ERR_INVALID_STATE;
    config_ = config;
    if (!config_.present) return ESP_OK;
    if (config_.address != kRtcAddress || !i2c_.initialized() ||
        config_.sdaPin < 0 || config_.sclPin < 0 || config_.bus < I2C_NUM_0 ||
        config_.bus >= I2C_NUM_MAX) {
        return ESP_ERR_INVALID_ARG;
    }

    auto descriptor = std::make_unique<RtcDevice>();
    const auto initStatus = ds3231_init_desc(
        &descriptor->value, static_cast<i2c_port_t>(config_.bus),
        static_cast<gpio_num_t>(config_.sdaPin),
        static_cast<gpio_num_t>(config_.sclPin));
    if (initStatus != ESP_OK) return initStatus;
    device_ = std::move(descriptor);
    if (i2c_dev_check_present(&device_->value) != ESP_OK) {
        static_cast<void>(ds3231_free_desc(&device_->value));
        device_.reset();
        return ESP_ERR_NOT_FOUND;
    }
    initialized_ = true;
    if (!ensureHealthControls()) {
        static_cast<void>(shutdown());
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t Ds3231SnRtcAdapter::shutdown() noexcept {
    if (!initialized_ && device_ == nullptr) return ESP_OK;
    esp_err_t status = ESP_OK;
    if (device_ != nullptr) {
        status = ds3231_free_desc(&device_->value);
        device_.reset();
    }
    initialized_ = false;
    return status;
}

esp_err_t Ds3231SnRtcAdapter::readRaw(RawRegisters& registers) noexcept {
    if (!initialized_ || device_ == nullptr) return ESP_ERR_INVALID_STATE;
    auto status = i2c_dev_take_mutex(&device_->value);
    if (status != ESP_OK) return status;
    status = i2c_dev_read_reg(&device_->value, kTimeRegister,
                              registers.calendar, sizeof(registers.calendar));
    if (status == ESP_OK)
        status = i2c_dev_read_reg(&device_->value, kControlRegister,
                                  &registers.control, 1U);
    if (status == ESP_OK)
        status = i2c_dev_read_reg(&device_->value, kStatusRegister,
                                  &registers.status, 1U);
    const auto giveStatus = i2c_dev_give_mutex(&device_->value);
    return status == ESP_OK ? giveStatus : status;
}

esp_err_t Ds3231SnRtcAdapter::writeControl(const std::uint8_t value) noexcept {
    if (!initialized_ || device_ == nullptr) return ESP_ERR_INVALID_STATE;
    auto status = i2c_dev_take_mutex(&device_->value);
    if (status == ESP_OK)
        status =
            i2c_dev_write_reg(&device_->value, kControlRegister, &value, 1U);
    const auto giveStatus = i2c_dev_give_mutex(&device_->value);
    return status == ESP_OK ? giveStatus : status;
}

bool Ds3231SnRtcAdapter::ensureHealthControls() noexcept {
    RawRegisters registers;
    if (readRaw(registers) != ESP_OK) return false;
    const auto originalRs = registers.control & kRsMask;
    if ((registers.control & kEoscMask) != 0U) {
        if (writeControl(static_cast<std::uint8_t>(registers.control &
                                                   ~kEoscMask)) != ESP_OK)
            return false;
        if (readRaw(registers) != ESP_OK ||
            (registers.control & kEoscMask) != 0U ||
            (registers.control & kRsMask) != originalRs)
            return false;
    }
    // Use the adopted library operation for the public R1 feature, then use
    // the narrow raw shim only to prove the exact DS3231SN register contract.
    if (ds3231_disable_32khz(&device_->value) != ESP_OK) return false;
    if (readRaw(registers) != ESP_OK) return false;
    return (registers.control & kEoscMask) == 0U &&
           (registers.status & kEn32khzMask) == 0U &&
           (registers.control & kRsMask) == originalRs;
}

bool Ds3231SnRtcAdapter::validateCalendar(
    const std::uint8_t (&calendar)[7]) const noexcept {
    if ((calendar[0] & 0x80U) != 0U || !validBcd(calendar[0] & 0x7FU, 59U) ||
        (calendar[1] & 0x80U) != 0U || !validBcd(calendar[1], 59U) ||
        (calendar[3] & 0xF8U) != 0U || !validBcd(calendar[3], 7U) ||
        decodeBcd(calendar[3]) == 0U || decodeBcd(calendar[3]) > 7U ||
        (calendar[4] & 0xC0U) != 0U || !validBcd(calendar[4] & 0x3FU, 31U) ||
        decodeBcd(calendar[4] & 0x3FU) == 0U || (calendar[5] & 0x80U) != 0U ||
        !validBcd(calendar[5] & 0x1FU, 12U) ||
        decodeBcd(calendar[5] & 0x1FU) == 0U || !validBcd(calendar[6], 99U)) {
        return false;
    }
    const auto month = decodeBcd(calendar[5] & 0x1FU);
    const auto day = decodeBcd(calendar[4] & 0x3FU);
    const int year = 2000 + decodeBcd(calendar[6]);
    if (month < 1U || month > 12U || day > daysInMonth(year, month))
        return false;
    const auto rawHour = calendar[2];
    if ((rawHour & 0x80U) != 0U) return false;
    if ((rawHour & 0x40U) != 0U) {
        if (!validBcd(rawHour & 0x1FU, 12U)) return false;
        const auto hour = decodeBcd(rawHour & 0x1FU);
        if (hour == 0U || hour > 12U) return false;
    } else if (!validBcd(rawHour & 0x3FU, 23U)) {
        return false;
    }
    return true;
}

bool Ds3231SnRtcAdapter::validateRaw(
    const RawRegisters& registers) const noexcept {
    // Status bits 6..4 are reserved in the DS3231/DS3231SN contract.  BSY
    // and alarm flags remain device-owned and are valid values for R1.
    return validateCalendar(registers.calendar) &&
           (registers.status & 0x70U) == 0U &&
           (registers.control & kEoscMask) == 0U &&
           (registers.status & kOsfMask) == 0U &&
           (registers.status & kEn32khzMask) == 0U;
}

bool Ds3231SnRtcAdapter::readLibraryTimeMatchesRaw(
    const RawRegisters& registers) noexcept {
    ::tm decoded{};
    if (ds3231_get_time(&device_->value, &decoded) != ESP_OK) return false;
    const auto& raw = registers.calendar;
    const auto rawHour =
        (raw[2] & 0x40U) != 0U
            ? static_cast<int>(decodeBcd(raw[2] & 0x1FU) % 12U) +
                  ((raw[2] & 0x20U) != 0U ? 12 : 0)
            : static_cast<int>(decodeBcd(raw[2] & 0x3FU));
    return decoded.tm_sec == decodeBcd(raw[0] & 0x7FU) &&
           decoded.tm_min == decodeBcd(raw[1]) && decoded.tm_hour == rawHour &&
           decoded.tm_mday == decodeBcd(raw[4] & 0x3FU) &&
           decoded.tm_mon == static_cast<int>(decodeBcd(raw[5] & 0x1FU)) - 1 &&
           decoded.tm_year == static_cast<int>(decodeBcd(raw[6])) + 100 &&
           decoded.tm_isdst == 0;
}

std::optional<std::int64_t> Ds3231SnRtcAdapter::rawCalendarToUnix(
    const std::uint8_t (&calendar)[7]) const noexcept {
    if (!validateCalendar(calendar)) return std::nullopt;
    const int year = 2000 + decodeBcd(calendar[6]);
    const auto month = decodeBcd(calendar[5] & 0x1FU);
    const auto day = decodeBcd(calendar[4] & 0x3FU);
    const auto hour =
        (calendar[2] & 0x40U) != 0U
            ? static_cast<int>(decodeBcd(calendar[2] & 0x1FU) % 12U) +
                  ((calendar[2] & 0x20U) != 0U ? 12 : 0)
            : static_cast<int>(decodeBcd(calendar[2] & 0x3FU));
    return daysFromCivil(year, month, day) * 86400LL +
           static_cast<std::int64_t>(hour) * 3600LL +
           static_cast<std::int64_t>(decodeBcd(calendar[1])) * 60LL +
           static_cast<std::int64_t>(decodeBcd(calendar[0] & 0x7FU));
}

std::optional<std::int64_t> Ds3231SnRtcAdapter::readTrustedUtc() noexcept {
    if (!initialized_) return std::nullopt;
    RawRegisters registers;
    if (readRaw(registers) != ESP_OK || !validateRaw(registers) ||
        !readLibraryTimeMatchesRaw(registers))
        return std::nullopt;
    return rawCalendarToUnix(registers.calendar);
}

bool Ds3231SnRtcAdapter::unixToTm(const std::int64_t utcUnixSeconds,
                                  ::tm& value) const noexcept {
    // R1 only writes the DS3231SN's explicitly supported 2000..2099 range.
    const std::time_t seconds = static_cast<std::time_t>(utcUnixSeconds);
    if (static_cast<std::int64_t>(seconds) != utcUnixSeconds ||
        gmtime_r(&seconds, &value) == nullptr || value.tm_year < 100 ||
        value.tm_year > 199) {
        return false;
    }
    return true;
}

bool Ds3231SnRtcAdapter::synchronizeFromSystemUtc(
    const std::int64_t utcUnixSeconds) noexcept {
    if (!initialized_) return false;
    ::tm value{};
    if (!unixToTm(utcUnixSeconds, value)) return false;
    if (ds3231_set_time(&device_->value, &value) != ESP_OK) return false;

    RawRegisters registers;
    if (readRaw(registers) != ESP_OK || !validateCalendar(registers.calendar) ||
        !readLibraryTimeMatchesRaw(registers))
        return false;
    const auto readbackUtc = rawCalendarToUnix(registers.calendar);
    if (!readbackUtc.has_value() || *readbackUtc != utcUnixSeconds)
        return false;
    if ((registers.control & kEoscMask) != 0U &&
        (writeControl(static_cast<std::uint8_t>(registers.control &
                                                ~kEoscMask)) != ESP_OK))
        return false;
    if (ds3231_disable_32khz(&device_->value) != ESP_OK ||
        readRaw(registers) != ESP_OK || (registers.control & kEoscMask) != 0U ||
        (registers.status & kEn32khzMask) != 0U ||
        !validateCalendar(registers.calendar))
        return false;

    bool osf = true;
    if (ds3231_get_oscillator_stop_flag(&device_->value, &osf) != ESP_OK)
        return false;
    if (osf && ds3231_clear_oscillator_stop_flag(&device_->value) != ESP_OK)
        return false;
    if (readRaw(registers) != ESP_OK || !validateRaw(registers) ||
        !readLibraryTimeMatchesRaw(registers))
        return false;
    const auto finalUtc = rawCalendarToUnix(registers.calendar);
    if (!finalUtc.has_value() || *finalUtc != utcUnixSeconds) return false;
    return true;
}

}  // namespace device_platform_esp_idf
