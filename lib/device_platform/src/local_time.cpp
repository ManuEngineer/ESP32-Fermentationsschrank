#include "local_time.hpp"

#include <ctime>

#include "civil_calendar.hpp"

namespace device_platform {
namespace {

constexpr std::int64_t kSecondsPerDay = 86400;
constexpr std::int64_t kTransitionSecondsIntoDayUtc = 3600;  // 01:00 UTC
constexpr std::int16_t kDaylightSavingMinutes = 60;

// Zerlegt eine UTC-Instanz in Kalenderfelder; `false`, wenn libc/time_t sie
// nicht darstellen kann.
bool breakDown(const std::int64_t seconds, std::tm& out) noexcept {
    const auto value = static_cast<std::time_t>(seconds);
    if (static_cast<std::int64_t>(value) != seconds) return false;
    return gmtime_r(&value, &out) != nullptr;
}

// UTC-Instanz "letzter Sonntag des Monats 01:00 UTC". Maerz und Oktober haben
// je 31 Tage; 1970-01-01 war ein Donnerstag, daher Sonntag = 0 bei
// (Tage + 4) mod 7.
std::int64_t lastSundayTransitionUtc(const int year,
                                     const unsigned month) noexcept {
    const auto lastDay = daysFromCivil(year, month, 31U);
    const auto weekday = (lastDay + 4) % 7;
    return (lastDay - weekday) * kSecondsPerDay + kTransitionSecondsIntoDayUtc;
}

}  // namespace

std::optional<LocalTime> toLocalTime(
    const std::optional<std::int64_t> trustedUtc,
    const TimeZoneRule& rule) noexcept {
    if (!trustedUtc.has_value() || *trustedUtc < 0) return std::nullopt;
    if (rule.dst != DaylightSavingRule::EuropeanUnion) return std::nullopt;

    const auto utc = *trustedUtc;
    std::tm utcFields{};
    if (!breakDown(utc, utcFields)) return std::nullopt;
    const int year = utcFields.tm_year + 1900;

    const bool daylightSaving = utc >= lastSundayTransitionUtc(year, 3U) &&
                                utc < lastSundayTransitionUtc(year, 10U);
    const auto offsetMinutes = static_cast<std::int16_t>(
        rule.standardOffsetMinutes +
        (daylightSaving ? kDaylightSavingMinutes : 0));

    std::tm localFields{};
    if (!breakDown(utc + static_cast<std::int64_t>(offsetMinutes) * 60,
                   localFields)) {
        return std::nullopt;
    }

    LocalTime local;
    local.year = static_cast<std::int32_t>(localFields.tm_year + 1900);
    local.month = static_cast<std::uint8_t>(localFields.tm_mon + 1);
    local.day = static_cast<std::uint8_t>(localFields.tm_mday);
    local.hour = static_cast<std::uint8_t>(localFields.tm_hour);
    local.minute = static_cast<std::uint8_t>(localFields.tm_min);
    local.second = static_cast<std::uint8_t>(localFields.tm_sec);
    local.utcOffsetMinutes = offsetMinutes;
    local.daylightSaving = daylightSaving;
    return local;
}

}  // namespace device_platform
