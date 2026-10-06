#pragma once

#include <cstdint>

namespace device_platform {

// Howard Hinnant's civil-calendar conversion: days since 1970-01-01 for a
// proleptic Gregorian date. It is general and independent of timezone and
// locale; callers own any range restriction (for example the DS3231SN
// 2000..2099 contract of the RTC adapter).
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

}  // namespace device_platform
