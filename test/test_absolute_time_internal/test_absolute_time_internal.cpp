#include <unity.h>

#include <array>
#include <cstdint>
#include <ctime>

#include "../../lib/device_platform_esp_idf/src/absolute_time_internal.hpp"

namespace {

namespace internal = device_platform_esp_idf::internal;

constexpr std::uint8_t bcd(const unsigned value) {
    return static_cast<std::uint8_t>(((value / 10U) << 4U) | (value % 10U));
}

internal::Ds3231SnRawRegisters validRaw() {
    internal::Ds3231SnRawRegisters raw;
    raw.calendar = {bcd(58U), bcd(59U), bcd(23U), bcd(4U),
                    bcd(29U), bcd(2U),  bcd(24U)};
    return raw;
}

std::int64_t expectedUtc(const int year, const unsigned month,
                         const unsigned day, const unsigned hour,
                         const unsigned minute, const unsigned second) {
    return internal::daysFromCivil(year, month, day) * 86400LL +
           static_cast<std::int64_t>(hour) * 3600LL +
           static_cast<std::int64_t>(minute) * 60LL +
           static_cast<std::int64_t>(second);
}

void test_valid_24_hour_calendar_is_trusted_and_converted() {
    const auto raw = validRaw();

    TEST_ASSERT_TRUE(internal::validateCalendar(raw.calendar));
    const auto utc = internal::rawCalendarToUnix(raw.calendar);
    TEST_ASSERT_TRUE(utc.has_value());
    TEST_ASSERT_EQUAL_INT64(expectedUtc(2024, 2U, 29U, 23U, 59U, 58U),
                            utc.value());
}

void test_valid_12_hour_am_and_pm_calendar_is_converted() {
    auto am = validRaw();
    am.calendar[2] = static_cast<std::uint8_t>(0x40U | bcd(12U));
    auto pm = am;
    pm.calendar[2] = static_cast<std::uint8_t>(0x40U | 0x20U | bcd(12U));

    const auto amUtc = internal::rawCalendarToUnix(am.calendar);
    const auto pmUtc = internal::rawCalendarToUnix(pm.calendar);
    TEST_ASSERT_TRUE(amUtc.has_value());
    TEST_ASSERT_TRUE(pmUtc.has_value());
    TEST_ASSERT_EQUAL_INT64(expectedUtc(2024, 2U, 29U, 0U, 59U, 58U),
                            amUtc.value());
    TEST_ASSERT_EQUAL_INT64(expectedUtc(2024, 2U, 29U, 12U, 59U, 58U),
                            pmUtc.value());
}

void test_invalid_bcd_is_rejected() {
    auto raw = validRaw();
    raw.calendar[0] = 0x6AU;

    TEST_ASSERT_FALSE(internal::validateCalendar(raw.calendar));
    TEST_ASSERT_FALSE(internal::rawCalendarToUnix(raw.calendar).has_value());
}

void test_invalid_calendar_day_month_and_leap_year_are_rejected() {
    auto invalidLeapDay = validRaw();
    invalidLeapDay.calendar[6] = bcd(23U);
    TEST_ASSERT_FALSE(internal::validateCalendar(invalidLeapDay.calendar));

    auto invalidDay = validRaw();
    invalidDay.calendar[4] = bcd(0U);
    TEST_ASSERT_FALSE(internal::validateCalendar(invalidDay.calendar));

    auto invalidMonth = validRaw();
    invalidMonth.calendar[5] = bcd(13U);
    TEST_ASSERT_FALSE(internal::validateCalendar(invalidMonth.calendar));
}

void test_month_reserved_and_century_bits_are_rejected() {
    for (const std::uint8_t invalidMonth : {0x21U, 0x41U, 0x61U, 0x81U}) {
        auto raw = validRaw();
        raw.calendar[5] = invalidMonth;
        TEST_ASSERT_FALSE(internal::validateCalendar(raw.calendar));
        TEST_ASSERT_FALSE(
            internal::rawCalendarToUnix(raw.calendar).has_value());
    }

    for (unsigned month = 1U; month <= 12U; ++month) {
        auto raw = validRaw();
        raw.calendar[5] = bcd(month);
        TEST_ASSERT_TRUE(internal::validateCalendar(raw.calendar));
        TEST_ASSERT_TRUE(internal::rawCalendarToUnix(raw.calendar).has_value());
    }
}

void test_year_outside_r1_contract_is_rejected() {
    ::tm value{};
    TEST_ASSERT_FALSE(
        internal::unixToDs3231Tm(expectedUtc(2100, 1U, 1U, 0U, 0U, 0U), value));
    TEST_ASSERT_TRUE(internal::unixToDs3231Tm(
        expectedUtc(2099, 12U, 31U, 23U, 59U, 59U), value));
}

void test_invalid_health_bits_are_untrusted() {
    auto osf = validRaw();
    osf.status = internal::kDs3231OsfMask;
    TEST_ASSERT_FALSE(internal::validateRaw(osf));

    auto eosc = validRaw();
    eosc.control = internal::kDs3231EoscMask;
    TEST_ASSERT_FALSE(internal::validateRaw(eosc));

    auto en32khz = validRaw();
    en32khz.status = internal::kDs3231En32khzMask;
    TEST_ASSERT_FALSE(internal::validateRaw(en32khz));

    auto reserved = validRaw();
    reserved.status = 0x10U;
    TEST_ASSERT_FALSE(internal::validateRaw(reserved));
}

void test_control_read_modify_write_clears_eosc_and_preserves_rs_bits() {
    constexpr std::uint8_t original = 0x98U;
    const auto corrected = internal::clearEoscPreservingControlBits(original);

    TEST_ASSERT_EQUAL_UINT8(0x18U, corrected);
    TEST_ASSERT_EQUAL_UINT8(original & internal::kDs3231RsMask,
                            corrected & internal::kDs3231RsMask);
}

void test_coherent_raw_sample_accepts_second_boundary_values() {
    auto beforeBoundary = validRaw();
    beforeBoundary.calendar = {bcd(59U), bcd(59U), bcd(23U), bcd(4U),
                               bcd(29U), bcd(2U),  bcd(24U)};
    auto afterBoundary = validRaw();
    afterBoundary.calendar = {bcd(0U), bcd(0U), bcd(0U), bcd(5U),
                              bcd(1U), bcd(3U), bcd(24U)};

    TEST_ASSERT_TRUE(internal::validateRaw(beforeBoundary));
    TEST_ASSERT_TRUE(internal::validateRaw(afterBoundary));
    TEST_ASSERT_EQUAL_INT64(
        expectedUtc(2024, 2U, 29U, 23U, 59U, 59U),
        internal::rawCalendarToUnix(beforeBoundary.calendar).value());
    TEST_ASSERT_EQUAL_INT64(
        expectedUtc(2024, 3U, 1U, 0U, 0U, 0U),
        internal::rawCalendarToUnix(afterBoundary.calendar).value());
}

struct MutexFake {
    int takeResult{0};
    int writeResult{0};
    int giveResult{0};
    int takeCalls{0};
    int writeCalls{0};
    int giveCalls{0};
};

int mutexTake(void* context) {
    auto& fake = *static_cast<MutexFake*>(context);
    ++fake.takeCalls;
    return fake.takeResult;
}

int mutexWrite(void* context, std::uint8_t) {
    auto& fake = *static_cast<MutexFake*>(context);
    ++fake.writeCalls;
    return fake.writeResult;
}

int mutexGive(void* context) {
    auto& fake = *static_cast<MutexFake*>(context);
    ++fake.giveCalls;
    return fake.giveResult;
}

internal::I2cMutexWriteBackend mutexBackend(MutexFake& fake) {
    return {&fake, &mutexTake, &mutexWrite, &mutexGive};
}

void test_mutex_take_failure_does_not_write_or_give() {
    MutexFake fake;
    fake.takeResult = -7;

    const auto result =
        internal::writeRegisterWithMutex(mutexBackend(fake), 0x12U);

    TEST_ASSERT_EQUAL_INT(-7, result);
    TEST_ASSERT_EQUAL_INT(1, fake.takeCalls);
    TEST_ASSERT_EQUAL_INT(0, fake.writeCalls);
    TEST_ASSERT_EQUAL_INT(0, fake.giveCalls);
}

void test_mutex_write_failure_gives_once_and_preserves_write_error() {
    MutexFake fake;
    fake.writeResult = -8;

    const auto result =
        internal::writeRegisterWithMutex(mutexBackend(fake), 0x12U);

    TEST_ASSERT_EQUAL_INT(-8, result);
    TEST_ASSERT_EQUAL_INT(1, fake.takeCalls);
    TEST_ASSERT_EQUAL_INT(1, fake.writeCalls);
    TEST_ASSERT_EQUAL_INT(1, fake.giveCalls);
}

void test_mutex_give_failure_is_returned_after_successful_write() {
    MutexFake fake;
    fake.giveResult = -9;

    const auto result =
        internal::writeRegisterWithMutex(mutexBackend(fake), 0x12U);

    TEST_ASSERT_EQUAL_INT(-9, result);
    TEST_ASSERT_EQUAL_INT(1, fake.takeCalls);
    TEST_ASSERT_EQUAL_INT(1, fake.writeCalls);
    TEST_ASSERT_EQUAL_INT(1, fake.giveCalls);
}

struct SyncFake {
    internal::Ds3231SnRawRegisters raw{validRaw()};
    bool failSet{false};
    bool failRead{false};
    int failReadAt{0};
    bool failWriteControl{false};
    bool failDisable32khz{false};
    bool failReadOsf{false};
    bool failClearOsf{false};
    bool mismatchAfterSet{false};
    bool malformedFirstReadback{false};
    std::array<std::int64_t, 3> scriptedReadbacks{};
    std::size_t scriptedReadbackCount{0U};
    int setCalls{0};
    int readCalls{0};
    int writeControlCalls{0};
    int disable32khzCalls{0};
    int readOsfCalls{0};
    int clearOsfCalls{0};
};

void encodeCalendar(const ::tm& value, std::array<std::uint8_t, 7>& calendar) {
    calendar[0] = bcd(static_cast<unsigned>(value.tm_sec));
    calendar[1] = bcd(static_cast<unsigned>(value.tm_min));
    calendar[2] = bcd(static_cast<unsigned>(value.tm_hour));
    calendar[3] =
        bcd(static_cast<unsigned>(value.tm_wday == 0 ? 7 : value.tm_wday));
    calendar[4] = bcd(static_cast<unsigned>(value.tm_mday));
    calendar[5] = bcd(static_cast<unsigned>(value.tm_mon + 1));
    calendar[6] = bcd(static_cast<unsigned>(value.tm_year - 100));
}

bool syncSetTime(void* context, const ::tm& value) {
    auto& fake = *static_cast<SyncFake*>(context);
    ++fake.setCalls;
    if (fake.failSet) return false;
    encodeCalendar(value, fake.raw.calendar);
    if (fake.mismatchAfterSet) fake.raw.calendar[0] = bcd(57U);
    return true;
}

bool syncReadRaw(void* context, internal::Ds3231SnRawRegisters& raw) {
    auto& fake = *static_cast<SyncFake*>(context);
    ++fake.readCalls;
    if (fake.failRead || fake.failReadAt == fake.readCalls) return false;
    raw = fake.raw;
    if (fake.scriptedReadbackCount > 0U &&
        static_cast<std::size_t>(fake.readCalls) <=
            fake.scriptedReadbackCount) {
        ::tm value{};
        if (!internal::unixToDs3231Tm(
                fake.scriptedReadbacks[static_cast<std::size_t>(fake.readCalls -
                                                                1)],
                value)) {
            return false;
        }
        encodeCalendar(value, raw.calendar);
    }
    if (fake.malformedFirstReadback && fake.readCalls == 1)
        raw.calendar[0] = 0x6AU;
    return true;
}

bool syncWriteControl(void* context, const std::uint8_t value) {
    auto& fake = *static_cast<SyncFake*>(context);
    ++fake.writeControlCalls;
    if (fake.failWriteControl) return false;
    fake.raw.control = value;
    return true;
}

bool syncDisable32khz(void* context) {
    auto& fake = *static_cast<SyncFake*>(context);
    ++fake.disable32khzCalls;
    if (fake.failDisable32khz) return false;
    fake.raw.status = static_cast<std::uint8_t>(fake.raw.status &
                                                ~internal::kDs3231En32khzMask);
    return true;
}

bool syncReadOsf(void* context, bool& osf) {
    auto& fake = *static_cast<SyncFake*>(context);
    ++fake.readOsfCalls;
    if (fake.failReadOsf) return false;
    osf = (fake.raw.status & internal::kDs3231OsfMask) != 0U;
    return true;
}

bool syncClearOsf(void* context) {
    auto& fake = *static_cast<SyncFake*>(context);
    ++fake.clearOsfCalls;
    if (fake.failClearOsf) return false;
    fake.raw.status =
        static_cast<std::uint8_t>(fake.raw.status & ~internal::kDs3231OsfMask);
    return true;
}

internal::Ds3231SnSyncBackend syncBackend(SyncFake& fake) {
    return {&fake,
            &syncSetTime,
            &syncReadRaw,
            &syncWriteControl,
            &syncDisable32khz,
            &syncReadOsf,
            &syncClearOsf};
}

constexpr std::int64_t syncUtc() { return 1709251198LL; }

void test_rtc_sync_set_failure_is_untrusted() {
    SyncFake fake;
    fake.failSet = true;

    TEST_ASSERT_FALSE(
        internal::synchronizeDs3231FromSystemUtc(syncBackend(fake), syncUtc()));
    TEST_ASSERT_EQUAL_INT(1, fake.setCalls);
    TEST_ASSERT_EQUAL_INT(0, fake.readCalls);
}

void test_rtc_sync_readback_mismatch_is_untrusted() {
    SyncFake fake;
    fake.mismatchAfterSet = true;

    TEST_ASSERT_FALSE(
        internal::synchronizeDs3231FromSystemUtc(syncBackend(fake), syncUtc()));
    TEST_ASSERT_EQUAL_INT(1, fake.setCalls);
    TEST_ASSERT_EQUAL_INT(1, fake.readCalls);
}

void test_rtc_sync_allows_normal_forward_second_rollover() {
    SyncFake fake;
    fake.scriptedReadbacks = {syncUtc(), syncUtc() + 1, syncUtc() + 1};
    fake.scriptedReadbackCount = 3U;

    TEST_ASSERT_TRUE(
        internal::synchronizeDs3231FromSystemUtc(syncBackend(fake), syncUtc()));
    TEST_ASSERT_EQUAL_INT(3, fake.readCalls);
}

void test_rtc_sync_allows_midnight_forward_second_rollover() {
    SyncFake fake;
    const auto target = expectedUtc(2024, 2U, 29U, 23U, 59U, 59U);
    fake.scriptedReadbacks = {target, target + 1, target + 1};
    fake.scriptedReadbackCount = 3U;

    TEST_ASSERT_TRUE(
        internal::synchronizeDs3231FromSystemUtc(syncBackend(fake), target));
}

void test_rtc_sync_rejects_backward_readback() {
    SyncFake fake;
    fake.scriptedReadbacks = {syncUtc(), syncUtc() - 1, syncUtc() - 1};
    fake.scriptedReadbackCount = 3U;

    TEST_ASSERT_FALSE(
        internal::synchronizeDs3231FromSystemUtc(syncBackend(fake), syncUtc()));
    TEST_ASSERT_EQUAL_INT(2, fake.readCalls);
}

void test_rtc_sync_rejects_implausible_forward_readback() {
    SyncFake fake;
    fake.scriptedReadbacks = {syncUtc(), syncUtc() + 2, syncUtc() + 2};
    fake.scriptedReadbackCount = 3U;

    TEST_ASSERT_FALSE(
        internal::synchronizeDs3231FromSystemUtc(syncBackend(fake), syncUtc()));
    TEST_ASSERT_EQUAL_INT(2, fake.readCalls);
}

void test_rtc_sync_rejects_malformed_readback() {
    SyncFake fake;
    fake.malformedFirstReadback = true;

    TEST_ASSERT_FALSE(
        internal::synchronizeDs3231FromSystemUtc(syncBackend(fake), syncUtc()));
    TEST_ASSERT_EQUAL_INT(1, fake.readCalls);
}

void test_rtc_sync_raw_read_failure_is_fail_closed() {
    SyncFake fake;
    fake.failRead = true;

    TEST_ASSERT_FALSE(
        internal::synchronizeDs3231FromSystemUtc(syncBackend(fake), syncUtc()));
}

void test_rtc_sync_control_write_failure_is_untrusted() {
    SyncFake fake;
    fake.raw.control = static_cast<std::uint8_t>(internal::kDs3231EoscMask |
                                                 internal::kDs3231RsMask);
    fake.failWriteControl = true;

    TEST_ASSERT_FALSE(
        internal::synchronizeDs3231FromSystemUtc(syncBackend(fake), syncUtc()));
    TEST_ASSERT_EQUAL_INT(1, fake.writeControlCalls);
}

void test_rtc_sync_osf_clear_failure_is_untrusted() {
    SyncFake fake;
    fake.raw.status = internal::kDs3231OsfMask;
    fake.failClearOsf = true;

    TEST_ASSERT_FALSE(
        internal::synchronizeDs3231FromSystemUtc(syncBackend(fake), syncUtc()));
    TEST_ASSERT_EQUAL_INT(1, fake.clearOsfCalls);
}

void test_rtc_sync_final_read_failure_is_untrusted() {
    SyncFake fake;
    fake.failReadAt = 3;

    TEST_ASSERT_FALSE(
        internal::synchronizeDs3231FromSystemUtc(syncBackend(fake), syncUtc()));
    TEST_ASSERT_EQUAL_INT(3, fake.readCalls);
}

void test_rtc_sync_success_completes_control_and_osf_sequence() {
    SyncFake fake;
    fake.raw.control = static_cast<std::uint8_t>(internal::kDs3231EoscMask |
                                                 internal::kDs3231RsMask);
    fake.raw.status = static_cast<std::uint8_t>(internal::kDs3231OsfMask |
                                                internal::kDs3231En32khzMask);

    TEST_ASSERT_TRUE(
        internal::synchronizeDs3231FromSystemUtc(syncBackend(fake), syncUtc()));
    TEST_ASSERT_EQUAL_INT(1, fake.setCalls);
    TEST_ASSERT_EQUAL_INT(3, fake.readCalls);
    TEST_ASSERT_EQUAL_INT(1, fake.writeControlCalls);
    TEST_ASSERT_EQUAL_INT(1, fake.disable32khzCalls);
    TEST_ASSERT_EQUAL_INT(1, fake.readOsfCalls);
    TEST_ASSERT_EQUAL_INT(1, fake.clearOsfCalls);
    TEST_ASSERT_EQUAL_UINT8(0x18U, fake.raw.control & internal::kDs3231RsMask);
    TEST_ASSERT_EQUAL_UINT8(0U, fake.raw.control & internal::kDs3231EoscMask);
    TEST_ASSERT_EQUAL_UINT8(0U, fake.raw.status & internal::kDs3231En32khzMask);
    TEST_ASSERT_EQUAL_UINT8(0U, fake.raw.status & internal::kDs3231OsfMask);
}

void test_sntp_reset_and_in_progress_do_not_promote_or_write_rtc() {
    internal::SntpArbitration arbitration;

    auto action = arbitration.observe(internal::SntpSyncObservation::Reset);
    TEST_ASSERT_FALSE(action.promoteSystemTrust);
    TEST_ASSERT_FALSE(action.synchronizeRtc);
    action = arbitration.observe(internal::SntpSyncObservation::InProgress);
    TEST_ASSERT_FALSE(action.promoteSystemTrust);
    TEST_ASSERT_FALSE(action.synchronizeRtc);
}

void test_sntp_completed_promotes_once_and_retries_after_next_sync() {
    internal::SntpArbitration arbitration;
    int rtcSynchronizationAttempts = 0;

    auto action = arbitration.observe(internal::SntpSyncObservation::Completed);
    TEST_ASSERT_TRUE(action.promoteSystemTrust);
    TEST_ASSERT_TRUE(action.synchronizeRtc);
    if (action.synchronizeRtc) ++rtcSynchronizationAttempts;
    action = arbitration.observe(internal::SntpSyncObservation::Completed);
    TEST_ASSERT_FALSE(action.promoteSystemTrust);
    TEST_ASSERT_FALSE(action.synchronizeRtc);
    if (action.synchronizeRtc) ++rtcSynchronizationAttempts;
    TEST_ASSERT_EQUAL_INT(1, rtcSynchronizationAttempts);

    action = arbitration.observe(internal::SntpSyncObservation::Reset);
    TEST_ASSERT_FALSE(action.promoteSystemTrust);
    action = arbitration.observe(internal::SntpSyncObservation::Completed);
    TEST_ASSERT_TRUE(action.promoteSystemTrust);
    TEST_ASSERT_TRUE(action.synchronizeRtc);
    if (action.synchronizeRtc) ++rtcSynchronizationAttempts;
    TEST_ASSERT_EQUAL_INT(2, rtcSynchronizationAttempts);
}

struct SntpActionFake {
    bool systemTimeTrusted{false};
    std::optional<std::int64_t> systemUtc{syncUtc()};
    bool rtcSyncResult{true};
    int rtcSyncCalls{0};
};

void fakePromoteSystemTrust(void* context) {
    static_cast<SntpActionFake*>(context)->systemTimeTrusted = true;
}

std::optional<std::int64_t> fakeReadSystemUtc(void* context) {
    return static_cast<SntpActionFake*>(context)->systemUtc;
}

bool fakeSynchronizeRtc(void* context, const std::int64_t) {
    auto& fake = *static_cast<SntpActionFake*>(context);
    ++fake.rtcSyncCalls;
    return fake.rtcSyncResult;
}

void test_sntp_completed_keeps_system_trust_when_rtc_write_fails_and_retries() {
    internal::SntpArbitration arbitration;
    SntpActionFake fake;
    const internal::SntpActionBackend backend{&fake, &fakePromoteSystemTrust,
                                              &fakeReadSystemUtc,
                                              &fakeSynchronizeRtc};

    auto action = arbitration.observe(internal::SntpSyncObservation::Completed);
    internal::consumeSntpArbitrationAction(action, backend);
    TEST_ASSERT_TRUE(fake.systemTimeTrusted);
    TEST_ASSERT_EQUAL_INT(1, fake.rtcSyncCalls);

    fake.rtcSyncResult = false;
    static_cast<void>(
        arbitration.observe(internal::SntpSyncObservation::Reset));
    action = arbitration.observe(internal::SntpSyncObservation::Completed);
    internal::consumeSntpArbitrationAction(action, backend);
    TEST_ASSERT_TRUE(fake.systemTimeTrusted);
    TEST_ASSERT_TRUE(fake.systemUtc.has_value());
    TEST_ASSERT_EQUAL_INT(2, fake.rtcSyncCalls);
}

void test_high_water_gate_is_fail_closed_until_trusted() {
    internal::UtcHighWaterPublicationGate gate;

    TEST_ASSERT_FALSE(gate.publish(100).has_value());
    gate.markTrusted();
    TEST_ASSERT_EQUAL_INT64(100, gate.publish(100).value());
    TEST_ASSERT_EQUAL_INT64(101, gate.publish(101).value());
    TEST_ASSERT_FALSE(gate.publish(99).has_value());
    TEST_ASSERT_EQUAL_INT64(101, gate.publish(101).value());
    TEST_ASSERT_EQUAL_INT64(102, gate.publish(102).value());
}

void test_i2c_lifecycle_initializes_and_shuts_down_exactly_once() {
    internal::I2cLifecycleState lifecycle;
    int initCalls = 0;
    int shutdownCalls = 0;
    const auto init = [&]() {
        ++initCalls;
        return 0;
    };
    const auto shutdown = [&]() {
        ++shutdownCalls;
        return 0;
    };

    TEST_ASSERT_EQUAL_INT(0, lifecycle.begin(init));
    TEST_ASSERT_EQUAL_INT(0, lifecycle.begin(init));
    TEST_ASSERT_EQUAL_INT(1, initCalls);
    TEST_ASSERT_EQUAL_INT(0, lifecycle.shutdown(shutdown));
    TEST_ASSERT_EQUAL_INT(0, lifecycle.shutdown(shutdown));
    TEST_ASSERT_EQUAL_INT(1, shutdownCalls);
}

void test_i2c_lifecycle_failed_operations_keep_retryable_state() {
    internal::I2cLifecycleState lifecycle;
    int initCalls = 0;
    int shutdownCalls = 0;
    bool failInit = true;
    bool failShutdown = true;
    const auto init = [&]() {
        ++initCalls;
        return failInit ? -1 : 0;
    };
    const auto shutdown = [&]() {
        ++shutdownCalls;
        return failShutdown ? -2 : 0;
    };

    TEST_ASSERT_EQUAL_INT(-1, lifecycle.begin(init));
    TEST_ASSERT_FALSE(lifecycle.initialized());
    failInit = false;
    TEST_ASSERT_EQUAL_INT(0, lifecycle.begin(init));
    TEST_ASSERT_TRUE(lifecycle.initialized());
    TEST_ASSERT_EQUAL_INT(-2, lifecycle.shutdown(shutdown));
    TEST_ASSERT_TRUE(lifecycle.initialized());
    failShutdown = false;
    TEST_ASSERT_EQUAL_INT(0, lifecycle.shutdown(shutdown));
    TEST_ASSERT_FALSE(lifecycle.initialized());
    TEST_ASSERT_EQUAL_INT(2, initCalls);
    TEST_ASSERT_EQUAL_INT(2, shutdownCalls);
}

void test_shared_i2c_claims_one_bus_and_rejects_pin_conflict() {
    internal::SharedI2cPortClaims claims;
    constexpr internal::I2cPortConfiguration configuration{0, 21, 22};
    constexpr internal::I2cPortConfiguration conflict{0, 25, 26};
    constexpr internal::I2cPortConfiguration secondPort{1, 25, 26};
    constexpr internal::I2cPortConfiguration secondPortConflict{1, 27, 28};

    TEST_ASSERT_TRUE(claims.claim(configuration));
    TEST_ASSERT_TRUE(claims.claim(configuration));
    TEST_ASSERT_TRUE(claims.claim(secondPort));
    TEST_ASSERT_EQUAL_UINT32(3U, claims.users());
    TEST_ASSERT_FALSE(claims.claim(conflict));
    TEST_ASSERT_FALSE(claims.claim(secondPortConflict));
    TEST_ASSERT_EQUAL_UINT32(3U, claims.users());

    TEST_ASSERT_TRUE(claims.release(configuration));
    TEST_ASSERT_TRUE(claims.hasClaims());
    TEST_ASSERT_FALSE(claims.claim(conflict));
    TEST_ASSERT_TRUE(claims.release(configuration));
    TEST_ASSERT_TRUE(claims.claim(conflict));
    TEST_ASSERT_TRUE(claims.release(conflict));
    TEST_ASSERT_TRUE(claims.hasClaims());
    TEST_ASSERT_TRUE(claims.release(secondPort));
    TEST_ASSERT_FALSE(claims.hasClaims());
}

void test_shared_i2c_removal_keeps_bus_until_last_device() {
    internal::SharedI2cPortClaims claims;
    constexpr internal::I2cPortConfiguration configuration{0, 21, 22};

    TEST_ASSERT_TRUE(claims.claim(configuration));
    TEST_ASSERT_TRUE(claims.claim(configuration));
    TEST_ASSERT_TRUE(claims.release(configuration));
    TEST_ASSERT_TRUE(claims.hasClaims());
    TEST_ASSERT_EQUAL_UINT32(1U, claims.users());
    TEST_ASSERT_TRUE(claims.release(configuration));
    TEST_ASSERT_FALSE(claims.hasClaims());
    TEST_ASSERT_FALSE(claims.release(configuration));
}

void test_shared_i2c_invalid_pin_claim_is_rejected_without_reconfiguration() {
    internal::SharedI2cPortClaims claims;
    constexpr internal::I2cPortConfiguration invalid{0, -1, 22};

    TEST_ASSERT_FALSE(claims.claim(invalid));
    TEST_ASSERT_FALSE(claims.hasClaims());
}

}  // namespace

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_valid_24_hour_calendar_is_trusted_and_converted);
    RUN_TEST(test_valid_12_hour_am_and_pm_calendar_is_converted);
    RUN_TEST(test_invalid_bcd_is_rejected);
    RUN_TEST(test_invalid_calendar_day_month_and_leap_year_are_rejected);
    RUN_TEST(test_month_reserved_and_century_bits_are_rejected);
    RUN_TEST(test_year_outside_r1_contract_is_rejected);
    RUN_TEST(test_invalid_health_bits_are_untrusted);
    RUN_TEST(test_control_read_modify_write_clears_eosc_and_preserves_rs_bits);
    RUN_TEST(test_coherent_raw_sample_accepts_second_boundary_values);
    RUN_TEST(test_mutex_take_failure_does_not_write_or_give);
    RUN_TEST(test_mutex_write_failure_gives_once_and_preserves_write_error);
    RUN_TEST(test_mutex_give_failure_is_returned_after_successful_write);
    RUN_TEST(test_rtc_sync_set_failure_is_untrusted);
    RUN_TEST(test_rtc_sync_readback_mismatch_is_untrusted);
    RUN_TEST(test_rtc_sync_allows_normal_forward_second_rollover);
    RUN_TEST(test_rtc_sync_allows_midnight_forward_second_rollover);
    RUN_TEST(test_rtc_sync_rejects_backward_readback);
    RUN_TEST(test_rtc_sync_rejects_implausible_forward_readback);
    RUN_TEST(test_rtc_sync_rejects_malformed_readback);
    RUN_TEST(test_rtc_sync_raw_read_failure_is_fail_closed);
    RUN_TEST(test_rtc_sync_control_write_failure_is_untrusted);
    RUN_TEST(test_rtc_sync_osf_clear_failure_is_untrusted);
    RUN_TEST(test_rtc_sync_final_read_failure_is_untrusted);
    RUN_TEST(test_rtc_sync_success_completes_control_and_osf_sequence);
    RUN_TEST(test_sntp_reset_and_in_progress_do_not_promote_or_write_rtc);
    RUN_TEST(test_sntp_completed_promotes_once_and_retries_after_next_sync);
    RUN_TEST(
        test_sntp_completed_keeps_system_trust_when_rtc_write_fails_and_retries);
    RUN_TEST(test_high_water_gate_is_fail_closed_until_trusted);
    RUN_TEST(test_i2c_lifecycle_initializes_and_shuts_down_exactly_once);
    RUN_TEST(test_i2c_lifecycle_failed_operations_keep_retryable_state);
    RUN_TEST(test_shared_i2c_claims_one_bus_and_rejects_pin_conflict);
    RUN_TEST(test_shared_i2c_removal_keeps_bus_until_last_device);
    RUN_TEST(
        test_shared_i2c_invalid_pin_claim_is_rejected_without_reconfiguration);
    return UNITY_END();
}
