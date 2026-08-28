#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <ctime>
#include <limits>
#include <optional>

namespace device_platform_esp_idf::internal {

// Internal, transport-independent representation of the DS3231/DS3231SN
// calendar and health registers.  The production adapter fills this from one
// coherent register burst; native tests exercise the same validation policy.
struct Ds3231SnRawRegisters {
    std::array<std::uint8_t, 7> calendar{};
    std::uint8_t control{0U};
    std::uint8_t status{0U};
};

constexpr std::uint8_t kDs3231EoscMask = 0x80U;
constexpr std::uint8_t kDs3231RsMask = 0x18U;
constexpr std::uint8_t kDs3231OsfMask = 0x80U;
constexpr std::uint8_t kDs3231En32khzMask = 0x08U;

inline bool validBcd(const std::uint8_t value, const std::uint8_t maximum) {
    if ((value & 0x0FU) > 9U || ((value >> 4U) & 0x0FU) > 9U) return false;
    const auto decoded =
        static_cast<unsigned>((value >> 4U) * 10U + (value & 0x0FU));
    return decoded <= maximum;
}

inline std::uint8_t decodeBcd(const std::uint8_t value) {
    return static_cast<std::uint8_t>((value >> 4U) * 10U + (value & 0x0FU));
}

inline bool leapYear(const int year) {
    return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
}

inline std::uint8_t daysInMonth(const int year, const std::uint8_t month) {
    constexpr std::uint8_t days[] = {31U, 28U, 31U, 30U, 31U, 30U,
                                     31U, 31U, 30U, 31U, 30U, 31U};
    if (month == 2U && leapYear(year)) return 29U;
    return days[month - 1U];
}

// Howard Hinnant's civil-calendar conversion, restricted to R1's explicit
// 2000..2099 DS3231SN contract.  It avoids timezone/locale behavior.
inline std::int64_t daysFromCivil(const int year, const unsigned month,
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

inline bool validateCalendar(const std::array<std::uint8_t, 7>& calendar) {
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
    if (month > 12U || day > daysInMonth(year, month)) return false;

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

inline bool validateRaw(const Ds3231SnRawRegisters& registers) {
    // DS3231/DS3231SN status bits 6..4 are reserved.  BSY and alarm flags
    // remain device-owned and are therefore not rejected by R1.
    return validateCalendar(registers.calendar) &&
           (registers.status & 0x70U) == 0U &&
           (registers.control & kDs3231EoscMask) == 0U &&
           (registers.status & kDs3231OsfMask) == 0U &&
           (registers.status & kDs3231En32khzMask) == 0U;
}

inline std::optional<std::int64_t> rawCalendarToUnix(
    const std::array<std::uint8_t, 7>& calendar) {
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

inline std::uint8_t clearEoscPreservingControlBits(const std::uint8_t control) {
    // R1 does not use RS1/RS2, but the health shim must preserve them (and all
    // other control bits) when it changes EOSC.
    return static_cast<std::uint8_t>(control & ~kDs3231EoscMask);
}

struct Ds3231SnSyncBackend {
    void* context{nullptr};
    bool (*setTime)(void*, const ::tm&){nullptr};
    bool (*readRaw)(void*, Ds3231SnRawRegisters&){nullptr};
    bool (*writeControl)(void*, std::uint8_t){nullptr};
    bool (*disable32khz)(void*){nullptr};
    bool (*readOsf)(void*, bool&){nullptr};
    bool (*clearOsf)(void*){nullptr};
};

struct I2cMutexWriteBackend {
    void* context{nullptr};
    int (*take)(void*){nullptr};
    int (*write)(void*, std::uint8_t){nullptr};
    int (*give)(void*){nullptr};
};

// The mutex is released only after a successful take.  A failed register
// write remains the primary error; a release error is reported only when the
// write itself succeeded.
inline int writeRegisterWithMutex(I2cMutexWriteBackend backend,
                                  const std::uint8_t value) noexcept {
    if (backend.take == nullptr || backend.write == nullptr ||
        backend.give == nullptr) {
        return -1;
    }
    const int takeStatus = backend.take(backend.context);
    if (takeStatus != 0) return takeStatus;
    const int writeStatus = backend.write(backend.context, value);
    const int giveStatus = backend.give(backend.context);
    return writeStatus == 0 ? giveStatus : writeStatus;
}

inline bool unixToDs3231Tm(const std::int64_t utcUnixSeconds, ::tm& value) {
    const auto seconds = static_cast<std::time_t>(utcUnixSeconds);
    if (static_cast<std::int64_t>(seconds) != utcUnixSeconds ||
        gmtime_r(&seconds, &value) == nullptr || value.tm_year < 100 ||
        value.tm_year > 199) {
        return false;
    }
    return true;
}

// The production adapter and native tests use this same narrow sequence.  A
// set/readback/control/OSF failure is terminal for the RTC synchronization;
// the caller may keep the already trusted system clock independently.
inline bool synchronizeDs3231FromSystemUtc(Ds3231SnSyncBackend backend,
                                           const std::int64_t utcUnixSeconds) {
    if (backend.setTime == nullptr || backend.readRaw == nullptr ||
        backend.writeControl == nullptr || backend.disable32khz == nullptr ||
        backend.readOsf == nullptr || backend.clearOsf == nullptr) {
        return false;
    }

    ::tm value{};
    if (!unixToDs3231Tm(utcUnixSeconds, value) ||
        !backend.setTime(backend.context, value)) {
        return false;
    }

    Ds3231SnRawRegisters registers;
    const auto readbackMatchesTarget = [&]() {
        const auto readback = rawCalendarToUnix(registers.calendar);
        return readback.has_value() && *readback == utcUnixSeconds;
    };
    if (!backend.readRaw(backend.context, registers) ||
        !validateCalendar(registers.calendar) || !readbackMatchesTarget()) {
        return false;
    }

    if ((registers.control & kDs3231EoscMask) != 0U &&
        !backend.writeControl(backend.context, clearEoscPreservingControlBits(
                                                   registers.control))) {
        return false;
    }
    if (!backend.disable32khz(backend.context) ||
        !backend.readRaw(backend.context, registers) ||
        (registers.control & kDs3231EoscMask) != 0U ||
        (registers.status & kDs3231En32khzMask) != 0U ||
        !validateCalendar(registers.calendar) || !readbackMatchesTarget()) {
        return false;
    }

    bool osf = true;
    if (!backend.readOsf(backend.context, osf) ||
        (osf && !backend.clearOsf(backend.context)) ||
        !backend.readRaw(backend.context, registers) ||
        !validateRaw(registers) || !readbackMatchesTarget()) {
        return false;
    }
    return true;
}

class UtcHighWaterPublicationGate final {
   public:
    void markTrusted() noexcept { trusted_ = true; }

    [[nodiscard]] bool trusted() const noexcept { return trusted_; }

    [[nodiscard]] std::optional<std::int64_t> publish(
        const std::int64_t utc) noexcept {
        if (!trusted_ ||
            (lastPublished_.has_value() && utc < *lastPublished_)) {
            return std::nullopt;
        }
        lastPublished_ = utc;
        return utc;
    }

   private:
    bool trusted_{false};
    std::optional<std::int64_t> lastPublished_;
};

enum class SntpSyncObservation : std::uint8_t {
    Other,
    Reset,
    InProgress,
    Completed,
};

struct SntpArbitrationAction {
    bool promoteSystemTrust{false};
    bool synchronizeRtc{false};
};

class SntpArbitration final {
   public:
    void reset() noexcept { completionHandled_ = false; }

    [[nodiscard]] SntpArbitrationAction observe(
        const SntpSyncObservation observation) noexcept {
        switch (observation) {
            case SntpSyncObservation::Reset:
            case SntpSyncObservation::InProgress:
                completionHandled_ = false;
                return {};
            case SntpSyncObservation::Completed:
                if (completionHandled_) return {};
                completionHandled_ = true;
                return {true, true};
            case SntpSyncObservation::Other:
                return {};
        }
        return {};
    }

   private:
    bool completionHandled_{false};
};

class I2cLifecycleState final {
   public:
    template <typename Init>
    auto begin(Init&& init) -> decltype(init()) {
        if (initialized_) return decltype(init()){};
        const auto status = init();
        if (status == 0) initialized_ = true;
        return status;
    }

    template <typename Shutdown>
    auto shutdown(Shutdown&& shutdownCall) -> decltype(shutdownCall()) {
        if (!initialized_) return decltype(shutdownCall()){};
        const auto status = shutdownCall();
        if (status == 0) initialized_ = false;
        return status;
    }

    [[nodiscard]] bool initialized() const noexcept { return initialized_; }

   private:
    bool initialized_{false};
};

struct I2cPortConfiguration {
    int bus{-1};
    int sda{-1};
    int scl{-1};
};

class SharedI2cPortClaims final {
   public:
    [[nodiscard]] bool claim(
        const I2cPortConfiguration configuration) noexcept {
        if (configuration.bus < 0 || configuration.sda < 0 ||
            configuration.scl < 0) {
            return false;
        }
        if (users_ == 0U) {
            configuration_ = configuration;
            users_ = 1U;
            return true;
        }
        if (configuration.bus != configuration_.bus ||
            configuration.sda != configuration_.sda ||
            configuration.scl != configuration_.scl) {
            return false;
        }
        ++users_;
        return true;
    }

    [[nodiscard]] bool release(
        const I2cPortConfiguration configuration) noexcept {
        if (users_ == 0U || configuration.bus != configuration_.bus ||
            configuration.sda != configuration_.sda ||
            configuration.scl != configuration_.scl) {
            return false;
        }
        --users_;
        if (users_ == 0U) configuration_ = {};
        return true;
    }

    [[nodiscard]] bool hasClaims() const noexcept { return users_ != 0U; }

    [[nodiscard]] std::size_t users() const noexcept { return users_; }

   private:
    I2cPortConfiguration configuration_{};
    std::size_t users_{0U};
};

}  // namespace device_platform_esp_idf::internal
