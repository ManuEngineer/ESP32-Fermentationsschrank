#include <unity.h>

#include <cstdint>
#include <optional>
#include <string>

#include "local_time.hpp"
#include "mock_time_zone_resolver.hpp"
#include "time_zone_rule.hpp"

namespace {

using device_platform::LocalTime;
using device_platform::PreparedTimeZone;

PreparedTimeZone zurich() {
    PreparedTimeZone zone;
    zone.canonicalIdentifier = "Europe/Zurich";
    zone.rule = device_platform::findTimeZoneRule("Europe/Zurich").value();
    return zone;
}

// Referenzvektoren: unabhaengig mit IANA-Daten (Python zoneinfo,
// Europe/Zurich) berechnet; Offset in Minuten.
struct Vector {
    std::int64_t utc;
    int year;
    int month;
    int day;
    int hour;
    int minute;
    int second;
    int offsetMinutes;
    bool dst;
};

constexpr Vector kVectors[] = {
    {1767225600, 2026, 1, 1, 1, 0, 0, 60, false},    // Winter
    {1782864000, 2026, 7, 1, 2, 0, 0, 120, true},    // Sommer
    {1774745999, 2026, 3, 29, 1, 59, 59, 60, false}, // Fruehjahr 2026
    {1774746000, 2026, 3, 29, 3, 0, 0, 120, true},
    {1792889999, 2026, 10, 25, 2, 59, 59, 120, true},  // Herbst 2026
    {1792890000, 2026, 10, 25, 2, 0, 0, 60, false},
    {1806195599, 2027, 3, 28, 1, 59, 59, 60, false},  // Fruehjahr 2027
    {1806195600, 2027, 3, 28, 3, 0, 0, 120, true},
    {1824944399, 2027, 10, 31, 2, 59, 59, 120, true},  // Herbst 2027
    {1824944400, 2027, 10, 31, 2, 0, 0, 60, false},
    {1711846799, 2024, 3, 31, 1, 59, 59, 60, false},  // Fruehjahr 2024
    {1711846800, 2024, 3, 31, 3, 0, 0, 120, true},
    {1729990799, 2024, 10, 27, 2, 59, 59, 120, true},  // Herbst 2024
    {1729990800, 2024, 10, 27, 2, 0, 0, 60, false},
    {2216249999, 2040, 3, 25, 1, 59, 59, 60, false},  // Fruehjahr 2040
    {2216250000, 2040, 3, 25, 3, 0, 0, 120, true},
    {2234998799, 2040, 10, 28, 2, 59, 59, 120, true},  // Herbst 2040
    {2234998800, 2040, 10, 28, 2, 0, 0, 60, false},
    {4109878799, 2100, 3, 28, 1, 59, 59, 60, false},  // Fruehjahr 2100
    {4109878800, 2100, 3, 28, 3, 0, 0, 120, true},
    {4128627599, 2100, 10, 31, 2, 59, 59, 120, true},  // Herbst 2100
    {4128627600, 2100, 10, 31, 2, 0, 0, 60, false},
    {2147483648, 2038, 1, 19, 4, 14, 8, 60, false},  // int32-Grenze
    {1798759800, 2027, 1, 1, 0, 30, 0, 60, false},   // Jahreswechsel
};

void assertVector(const Vector& v, const LocalTime& local) {
    TEST_ASSERT_EQUAL_INT32(v.year, local.year);
    TEST_ASSERT_EQUAL_UINT8(v.month, local.month);
    TEST_ASSERT_EQUAL_UINT8(v.day, local.day);
    TEST_ASSERT_EQUAL_UINT8(v.hour, local.hour);
    TEST_ASSERT_EQUAL_UINT8(v.minute, local.minute);
    TEST_ASSERT_EQUAL_UINT8(v.second, local.second);
    TEST_ASSERT_EQUAL_INT16(v.offsetMinutes, local.utcOffsetMinutes);
    TEST_ASSERT_EQUAL(v.dst, local.daylightSaving);
}

void test_zurich_reference_vectors_including_dst_boundaries() {
    const auto zone = zurich();
    for (const auto& vector : kVectors) {
        const auto local = device_platform::toLocalTime(vector.utc, zone);
        TEST_ASSERT_TRUE_MESSAGE(local.has_value(), "no local time");
        assertVector(vector, *local);
    }
}

void test_autumn_overlap_is_distinguished_by_daylight_saving_flag() {
    const auto zone = zurich();
    // Two different UTC instants share the local time 02:30:00 on 2026-10-25.
    const auto summer = device_platform::toLocalTime(1792890000 - 1800, zone);
    const auto standard = device_platform::toLocalTime(1792890000 + 1800, zone);
    TEST_ASSERT_TRUE(summer.has_value());
    TEST_ASSERT_TRUE(standard.has_value());
    TEST_ASSERT_EQUAL_UINT8(2, summer->hour);
    TEST_ASSERT_EQUAL_UINT8(30, summer->minute);
    TEST_ASSERT_EQUAL_UINT8(2, standard->hour);
    TEST_ASSERT_EQUAL_UINT8(30, standard->minute);
    TEST_ASSERT_TRUE(summer->daylightSaving);
    TEST_ASSERT_FALSE(standard->daylightSaving);
    TEST_ASSERT_EQUAL_INT16(120, summer->utcOffsetMinutes);
    TEST_ASSERT_EQUAL_INT16(60, standard->utcOffsetMinutes);
}

void test_missing_trusted_utc_yields_no_local_time() {
    TEST_ASSERT_FALSE(
        device_platform::toLocalTime(std::nullopt, zurich()).has_value());
}

void test_unprepared_zone_yields_no_local_time() {
    const PreparedTimeZone unprepared;
    TEST_ASSERT_EQUAL(
        static_cast<int>(device_platform::DaylightSavingRule::Unavailable),
        static_cast<int>(unprepared.rule.dst));
    TEST_ASSERT_FALSE(
        device_platform::toLocalTime(1782864000, unprepared).has_value());
}

void test_negative_utc_yields_no_local_time() {
    TEST_ASSERT_FALSE(device_platform::toLocalTime(-1, zurich()).has_value());
}

void test_lookup_finds_only_exact_canonical_identifier() {
    const auto rule = device_platform::findTimeZoneRule("Europe/Zurich");
    TEST_ASSERT_TRUE(rule.has_value());
    TEST_ASSERT_EQUAL_INT16(60, rule->standardOffsetMinutes);
    TEST_ASSERT_EQUAL(
        static_cast<int>(device_platform::DaylightSavingRule::EuropeanUnion),
        static_cast<int>(rule->dst));
    TEST_ASSERT_FALSE(device_platform::findTimeZoneRule("").has_value());
    TEST_ASSERT_FALSE(
        device_platform::findTimeZoneRule("europe/zurich").has_value());
    TEST_ASSERT_FALSE(
        device_platform::findTimeZoneRule("Europe/Berlin").has_value());
    TEST_ASSERT_FALSE(
        device_platform::findTimeZoneRule("Europe/Zurich ").has_value());
}

void test_prepared_zone_from_resolver_converts_and_unknown_zone_does_not() {
    const device_platform_test_support::MockTimeZoneResolver resolver;
    const auto known = resolver.prepare("Europe/Zurich");
    TEST_ASSERT_TRUE(known.prepared.has_value());
    const auto local = device_platform::toLocalTime(1782864000, *known.prepared);
    TEST_ASSERT_TRUE(local.has_value());
    TEST_ASSERT_EQUAL_UINT8(2, local->hour);

    // The mock accepts any identifier, but only a catalog zone carries a rule.
    const auto unknown = resolver.prepare("Europe/Berlin");
    TEST_ASSERT_TRUE(unknown.prepared.has_value());
    TEST_ASSERT_FALSE(
        device_platform::toLocalTime(1782864000, *unknown.prepared)
            .has_value());
}

}  // namespace

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_zurich_reference_vectors_including_dst_boundaries);
    RUN_TEST(test_autumn_overlap_is_distinguished_by_daylight_saving_flag);
    RUN_TEST(test_missing_trusted_utc_yields_no_local_time);
    RUN_TEST(test_unprepared_zone_yields_no_local_time);
    RUN_TEST(test_negative_utc_yields_no_local_time);
    RUN_TEST(test_lookup_finds_only_exact_canonical_identifier);
    RUN_TEST(test_prepared_zone_from_resolver_converts_and_unknown_zone_does_not);
    return UNITY_END();
}
