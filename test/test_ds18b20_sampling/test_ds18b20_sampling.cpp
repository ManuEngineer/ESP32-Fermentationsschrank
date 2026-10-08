#include <unity.h>

#include <array>
#include <cmath>
#include <cstdint>
#include <optional>

#include "ds18b20_channel_source.hpp"
#include "ds18b20_sampling_engine.hpp"
#include "fake_ds18b20_bus.hpp"
#include "sensor_calibration.hpp"
#include "sensor_identity.hpp"
#include "sensor_offset.hpp"
#include "sensor_quality_config.hpp"
#include "sensor_quality_pipeline.hpp"
#include "temperature_source.hpp"
#include "virtual_time_source.hpp"

// Native Matrix fuer Issue #30, Softwarepfad C1 (docs/tasks/issue-30-ds18b20-
// sensor-adapters-plan.md, Abschnitte 4a, 4b, 4c, 5.2). Alle Tests sind
// Simulationen (SIM-30-*) mit Fake-Bus und virtueller Zeit; sie sind KEIN
// Nachweis realer Hardware. Die Engine kennt nur technische Kanaele (0/1 am
// ExpectedRoms-Bus, 2 am SingleDevice-Bus); Rollen kommen nicht vor.

namespace {

using device_platform::Ds18b20ChannelBinding;
using device_platform::Ds18b20ChannelSource;
using device_platform::Ds18b20DriverResult;
using device_platform::Ds18b20SamplingEngine;
using device_platform::OneWireRom;
using device_platform::SensorCalibration;
using device_platform::SensorFaultReason;
using device_platform::SensorIdentity;
using device_platform::SensorOffset;
using device_platform::SensorQuality;
using device_platform::SensorQualityConfig;
using device_platform::SensorQualityPipeline;
using device_platform::TemperatureReading;
using device_platform::TemperatureSampleStatus;
using device_platform::VirtualTimeSource;
using device_platform_test_support::FakeDs18b20Bus;

constexpr OneWireRom kRomA = 0x28FF000000000001ULL;
constexpr OneWireRom kRomB = 0x28FF000000000002ULL;
constexpr OneWireRom kRomC = 0x28FF000000000003ULL;
constexpr OneWireRom kRomD = 0x28FF000000000004ULL;
constexpr OneWireRom kRomE = 0x28FF000000000005ULL;
constexpr uint8_t kProduct = Ds18b20SamplingEngine::kSingleDeviceChannel;

struct Fixture {
    VirtualTimeSource time;
    FakeDs18b20Bus bus0;
    FakeDs18b20Bus bus1;
    Ds18b20SamplingEngine engine{time, bus0, bus1};

    void bind(OneWireRom channel0, OneWireRom channel1) {
        engine.setBinding({{Ds18b20ChannelBinding{0U, channel0},
                            Ds18b20ChannelBinding{1U, channel1}}});
    }

    // Laeuft einen vollstaendigen Zyklus (Beginn, Wartezeit, Lesen).
    void cycle() {
        const uint64_t target = engine.completedCycles() + 1U;
        for (int guard = 0; guard < 8 && engine.completedCycles() < target;
             ++guard) {
            const uint64_t due = engine.nextDueMillis();
            if (due > time.monotonicMillis()) {
                time.advanceMonotonicMillis(due - time.monotonicMillis());
            }
            engine.step();
        }
        TEST_ASSERT_EQUAL_UINT64(target, engine.completedCycles());
    }

    TemperatureReading r(uint8_t channel) const {
        return engine.reading(channel);
    }
};

void assertStatus(const TemperatureReading& reading,
                  TemperatureSampleStatus status) {
    TEST_ASSERT_TRUE(reading.status() == status);
}

void assertIdentity(const TemperatureReading& reading,
                    std::optional<OneWireRom> rom) {
    if (!rom.has_value()) {
        TEST_ASSERT_FALSE(reading.identity().has_value());
        return;
    }
    TEST_ASSERT_TRUE(reading.identity().has_value());
    TEST_ASSERT_EQUAL_UINT64(*rom, reading.identity()->value());
}

SensorQualityConfig makeConfig() {
    return SensorQualityConfig::create(
               /*medianWindowSize=*/3U, /*lowPassTauSeconds=*/5.0,
               /*minPlausibleCelsius=*/-20.0, /*maxPlausibleCelsius=*/80.0,
               /*maxRateOfChangeCelsiusPerSecond=*/100.0,
               /*maxStaleAgeMs=*/10'000U, /*maxConsecutiveInvalid=*/3U,
               /*minConsecutiveValidSamples=*/2U,
               /*minRecoveryStabilityDurationMs=*/2'000U)
        .config.value();
}

SensorCalibration makeCalibration(OneWireRom rom, double offset) {
    return SensorCalibration(SensorIdentity::create(rom).identity.value(),
                             SensorOffset::create(offset).offset.value());
}

// Von Hand aufgebaute Referenzfolge ohne Adapter: Zyklus k beginnt bei
// 2000*k; Ok- und CRC-Proben tragen den Zeitpunkt nach der Konvertierung
// (2000*k + 800), alle anderen den Zyklusbeginn. Jede Probe wird mit dem
// Zeitpunkt des Zyklusendes zugestellt, wie im Adapterlauf.
void refIngest(SensorQualityPipeline& pipeline, int cycle,
               TemperatureSampleStatus status, OneWireRom rom,
               std::optional<double> celsius) {
    const uint64_t start = static_cast<uint64_t>(cycle) * 2'000U;
    const uint64_t end = start + 800U;
    const bool converted = status == TemperatureSampleStatus::Ok ||
                           status == TemperatureSampleStatus::CrcFault;
    const auto reading =
        TemperatureReading::create(SensorIdentity::create(rom).identity,
                                   converted ? end : start, status, celsius);
    (void)pipeline.ingest(reading.reading.value(), end);
}

TemperatureSampleStatus gapStatus(int gapKind) {
    // 0 Absent, 1 BusFault, 2 CrcFault, 3 MultiDevice
    switch (gapKind) {
        case 0:
            return TemperatureSampleStatus::MissingSample;
        case 2:
            return TemperatureSampleStatus::CrcFault;
        default:
            return TemperatureSampleStatus::BusFault;
    }
}

void assertSnapshotsEqual(const device_platform::SensorQualitySnapshot& a,
                          const device_platform::SensorQualitySnapshot& b) {
    TEST_ASSERT_TRUE(a.quality == b.quality);
    TEST_ASSERT_TRUE(a.lastFaultReason == b.lastFaultReason);
    TEST_ASSERT_EQUAL_UINT16(a.consecutiveInvalidCount,
                             b.consecutiveInvalidCount);
    TEST_ASSERT_EQUAL_UINT16(a.recoveryProgressCount, b.recoveryProgressCount);
    TEST_ASSERT_EQUAL(a.identity.has_value(), b.identity.has_value());
    if (a.identity.has_value()) {
        TEST_ASSERT_EQUAL_UINT64(a.identity->value(), b.identity->value());
    }
    TEST_ASSERT_EQUAL(a.filteredCelsius.has_value(),
                      b.filteredCelsius.has_value());
    if (a.filteredCelsius.has_value()) {
        TEST_ASSERT_DOUBLE_WITHIN(1e-9, *b.filteredCelsius, *a.filteredCelsius);
    }
    TEST_ASSERT_EQUAL(a.appliedOffset.has_value(), b.appliedOffset.has_value());
    if (a.appliedOffset.has_value()) {
        TEST_ASSERT_DOUBLE_WITHIN(1e-9, *b.appliedOffset, *a.appliedOffset);
    }
}

// ---------------------------------------------------------------------------
// SIM-30-01 Bindung (4a)
// ---------------------------------------------------------------------------

void test_unbound_expected_rom_channels_never_ok_and_have_no_identity() {
    Fixture f;
    f.bus0.addDevice(kRomA, 20.0);
    f.bus0.addDevice(kRomB, 5.0);
    f.cycle();
    for (uint8_t channel = 0U; channel < 2U; ++channel) {
        assertStatus(f.r(channel), TemperatureSampleStatus::MissingSample);
        assertIdentity(f.r(channel), std::nullopt);
    }
    // Der Bus wird fuer den Bericht enumeriert, aber nie konvertiert.
    TEST_ASSERT_EQUAL_UINT32(0U, f.bus0.conversionCount());
    const auto report = f.engine.report();
    TEST_ASSERT_FALSE(report.bindingValid);
    TEST_ASSERT_EQUAL_UINT8(2U, report.expectedRomBus.enumeration.count);
}

void test_invalid_bindings_stay_unbound() {
    const std::array<std::array<std::optional<Ds18b20ChannelBinding>, 2>, 5>
        invalid{{
            {{std::nullopt, std::nullopt}},
            {{Ds18b20ChannelBinding{0U, kRomA}, std::nullopt}},
            {{Ds18b20ChannelBinding{0U, 0U}, Ds18b20ChannelBinding{1U, kRomB}}},
            {{Ds18b20ChannelBinding{0U, kRomA},
              Ds18b20ChannelBinding{1U, kRomA}}},
            {{Ds18b20ChannelBinding{0U, kRomA},
              Ds18b20ChannelBinding{0U, kRomB}}},
        }};
    for (const auto& bindings : invalid) {
        Fixture f;
        f.bus0.addDevice(kRomA, 20.0);
        f.bus0.addDevice(kRomB, 5.0);
        f.engine.setBinding(bindings);
        f.cycle();
        for (uint8_t channel = 0U; channel < 2U; ++channel) {
            assertStatus(f.r(channel), TemperatureSampleStatus::MissingSample);
            assertIdentity(f.r(channel), std::nullopt);
        }
        TEST_ASSERT_FALSE(f.engine.report().bindingValid);
    }
}

void test_first_ok_after_verified_binding_fresh_conversion_and_resolution() {
    Fixture f;
    f.bind(kRomA, kRomB);
    f.bus0.addDevice(kRomA, 21.5);
    f.bus0.addDevice(kRomB, 5.0625);
    f.cycle();
    assertStatus(f.r(0U), TemperatureSampleStatus::Ok);
    assertIdentity(f.r(0U), kRomA);
    TEST_ASSERT_EQUAL_DOUBLE(21.5, f.r(0U).celsius().value());
    assertStatus(f.r(1U), TemperatureSampleStatus::Ok);
    assertIdentity(f.r(1U), kRomB);
    TEST_ASSERT_EQUAL_DOUBLE(5.0625, f.r(1U).celsius().value());
    // 12 Bit genau einmal je ROM; Konvertierung genau einmal je Zyklus.
    TEST_ASSERT_EQUAL_UINT32(2U, f.bus0.resolutionCalls());
    TEST_ASSERT_EQUAL_UINT32(1U, f.bus0.conversionCount());
    f.cycle();
    TEST_ASSERT_EQUAL_UINT32(2U, f.bus0.resolutionCalls());
    TEST_ASSERT_EQUAL_UINT32(2U, f.bus0.conversionCount());
    TEST_ASSERT_EQUAL_UINT32(0U, f.bus0.staleReadCount());
}

void test_channel_mapping_follows_rom_not_enumeration_order() {
    Fixture f;
    // kRomB sortiert nach kRomA in der Enumeration, ist aber an Kanal 0
    // gebunden.
    f.bind(kRomB, kRomA);
    f.bus0.addDevice(kRomA, 20.0);
    f.bus0.addDevice(kRomB, 5.0);
    f.cycle();
    assertIdentity(f.r(0U), kRomB);
    TEST_ASSERT_EQUAL_DOUBLE(5.0, f.r(0U).celsius().value());
    assertIdentity(f.r(1U), kRomA);
    TEST_ASSERT_EQUAL_DOUBLE(20.0, f.r(1U).celsius().value());
}

void test_missing_expected_rom_affects_only_its_channel() {
    Fixture f;
    f.bind(kRomA, kRomB);
    f.bus0.addDevice(kRomA, 20.0);
    f.cycle();
    assertStatus(f.r(0U), TemperatureSampleStatus::Ok);
    assertStatus(f.r(1U), TemperatureSampleStatus::MissingSample);
    assertIdentity(f.r(1U), kRomB);  // 4b: erwartetes ROM bleibt Identitaet
    // Wiederkehr desselben ROM liest den Kanal wieder.
    f.bus0.addDevice(kRomB, 6.0);
    f.cycle();
    assertStatus(f.r(1U), TemperatureSampleStatus::Ok);
    assertIdentity(f.r(1U), kRomB);
}

void test_unknown_extra_rom_is_binding_conflict_for_both_channels() {
    Fixture f;
    f.bind(kRomA, kRomB);
    f.bus0.addDevice(kRomA, 20.0);
    f.bus0.addDevice(kRomB, 5.0);
    f.cycle();
    assertStatus(f.r(0U), TemperatureSampleStatus::Ok);
    f.bus0.addDevice(kRomC, 10.0);
    f.cycle();
    for (uint8_t channel = 0U; channel < 2U; ++channel) {
        assertStatus(f.r(channel), TemperatureSampleStatus::MissingSample);
    }
    assertIdentity(f.r(0U), kRomA);
    assertIdentity(f.r(1U), kRomB);
    TEST_ASSERT_TRUE(f.engine.report().expectedRomBus.bindingConflict);
    // Konflikt = keine neue Konvertierung auf Bus 0.
    TEST_ASSERT_EQUAL_UINT32(1U, f.bus0.conversionCount());
    // Konflikt endet, sobald die Enumeration wieder dem Datensatz entspricht.
    f.bus0.removeDevice(kRomC);
    f.cycle();
    assertStatus(f.r(0U), TemperatureSampleStatus::Ok);
    assertStatus(f.r(1U), TemperatureSampleStatus::Ok);
}

void test_more_than_four_devices_is_a_binding_conflict() {
    Fixture f;
    f.bind(kRomA, kRomB);
    f.bus0.addDevice(kRomA, 20.0);
    f.bus0.addDevice(kRomB, 5.0);
    f.bus0.addDevice(kRomC, 1.0);
    f.bus0.addDevice(kRomD, 1.0);
    f.bus0.addDevice(kRomE, 1.0);
    f.cycle();
    for (uint8_t channel = 0U; channel < 2U; ++channel) {
        assertStatus(f.r(channel), TemperatureSampleStatus::MissingSample);
    }
    TEST_ASSERT_TRUE(f.engine.report().expectedRomBus.enumeration.overflow);
}

// ---------------------------------------------------------------------------
// SIM-30-02 Fehlermapping
// ---------------------------------------------------------------------------

void test_bus_wide_failures_hit_both_expected_rom_channels() {
    struct Case {
        Ds18b20DriverResult presence;
        TemperatureSampleStatus expected;
    };
    const std::array<Case, 3> cases{{
        {Ds18b20DriverResult::NotFound, TemperatureSampleStatus::MissingSample},
        {Ds18b20DriverResult::Timeout, TemperatureSampleStatus::BusFault},
        {Ds18b20DriverResult::Other, TemperatureSampleStatus::BusFault},
    }};
    for (const auto& c : cases) {
        Fixture f;
        f.bind(kRomA, kRomB);
        f.bus0.addDevice(kRomA, 20.0);
        f.bus0.addDevice(kRomB, 5.0);
        f.cycle();
        f.bus0.overridePresence(c.presence);
        f.cycle();
        for (uint8_t channel = 0U; channel < 2U; ++channel) {
            assertStatus(f.r(channel), c.expected);
        }
        assertIdentity(f.r(0U), kRomA);
        assertIdentity(f.r(1U), kRomB);
    }
    // ROM-Enumerationsfehler (CRC) ebenfalls auf beiden Kanaelen.
    Fixture f;
    f.bind(kRomA, kRomB);
    f.bus0.addDevice(kRomA, 20.0);
    f.bus0.addDevice(kRomB, 5.0);
    f.bus0.overrideEnumeration(Ds18b20DriverResult::InvalidCrc);
    f.cycle();
    assertStatus(f.r(0U), TemperatureSampleStatus::CrcFault);
    assertStatus(f.r(1U), TemperatureSampleStatus::CrcFault);
}

void test_single_sensor_faults_do_not_affect_the_other_channel() {
    struct Case {
        Ds18b20DriverResult read;
        TemperatureSampleStatus expected;
    };
    const std::array<Case, 4> cases{{
        {Ds18b20DriverResult::InvalidCrc, TemperatureSampleStatus::CrcFault},
        {Ds18b20DriverResult::PowerOnValue,
         TemperatureSampleStatus::KnownInvalidMeasurement},
        {Ds18b20DriverResult::Timeout, TemperatureSampleStatus::BusFault},
        {Ds18b20DriverResult::NotFound, TemperatureSampleStatus::MissingSample},
    }};
    for (const auto& c : cases) {
        Fixture f;
        f.bind(kRomA, kRomB);
        f.bus0.addDevice(kRomA, 20.0);
        f.bus0.addDevice(kRomB, 5.0);
        f.bus0.setReadResult(kRomA, c.read);
        f.cycle();
        assertStatus(f.r(0U), c.expected);
        assertIdentity(f.r(0U), kRomA);
        TEST_ASSERT_FALSE(f.r(0U).celsius().has_value());
        assertStatus(f.r(1U), TemperatureSampleStatus::Ok);
        TEST_ASSERT_EQUAL_DOUBLE(5.0, f.r(1U).celsius().value());
    }
}

void test_power_on_value_is_never_a_measurement() {
    Fixture f;
    f.bus1.addDevice(kRomA, 85.0);
    f.bus1.setReadResult(kRomA, Ds18b20DriverResult::PowerOnValue);
    f.cycle();
    assertStatus(f.r(kProduct),
                 TemperatureSampleStatus::KnownInvalidMeasurement);
    TEST_ASSERT_FALSE(f.r(kProduct).celsius().has_value());
}

void test_start_conversion_and_set_resolution_failures_are_typed() {
    Fixture f;
    f.bind(kRomA, kRomB);
    f.bus0.addDevice(kRomA, 20.0);
    f.bus0.addDevice(kRomB, 5.0);
    f.bus0.setSetResolutionResult(Ds18b20DriverResult::Timeout);
    f.cycle();
    assertStatus(f.r(0U), TemperatureSampleStatus::BusFault);
    assertStatus(f.r(1U), TemperatureSampleStatus::BusFault);
    f.bus0.setSetResolutionResult(Ds18b20DriverResult::Ok);
    f.bus0.overrideStartConversion(Ds18b20DriverResult::Timeout);
    f.cycle();
    assertStatus(f.r(0U), TemperatureSampleStatus::BusFault);
    assertIdentity(f.r(0U), kRomA);
    f.bus0.overrideStartConversion(std::nullopt);
    f.cycle();
    assertStatus(f.r(0U), TemperatureSampleStatus::Ok);
}

void test_bus_faults_are_independent_between_the_two_buses() {
    Fixture f;
    f.bind(kRomA, kRomB);
    f.bus0.addDevice(kRomA, 20.0);
    f.bus0.addDevice(kRomB, 5.0);
    f.bus1.addDevice(kRomC, 30.0);
    f.bus1.overridePresence(Ds18b20DriverResult::Timeout);
    f.cycle();
    assertStatus(f.r(0U), TemperatureSampleStatus::Ok);
    assertStatus(f.r(1U), TemperatureSampleStatus::Ok);
    assertStatus(f.r(kProduct), TemperatureSampleStatus::BusFault);
    f.bus1.overridePresence(std::nullopt);
    f.bus0.overridePresence(Ds18b20DriverResult::Timeout);
    f.cycle();
    assertStatus(f.r(0U), TemperatureSampleStatus::BusFault);
    assertStatus(f.r(kProduct), TemperatureSampleStatus::Ok);
}

// ---------------------------------------------------------------------------
// SIM-30-03 SingleDevice-Bus (4a Punkt 6, 4b)
// ---------------------------------------------------------------------------

void test_single_device_bus_zero_one_two_and_overflow() {
    Fixture f;
    f.cycle();
    assertStatus(f.r(kProduct), TemperatureSampleStatus::MissingSample);
    assertIdentity(f.r(kProduct), std::nullopt);

    f.bus1.addDevice(kRomA, 30.0);
    f.cycle();
    assertStatus(f.r(kProduct), TemperatureSampleStatus::Ok);
    assertIdentity(f.r(kProduct), kRomA);

    f.bus1.addDevice(kRomB, 31.0);
    f.cycle();
    assertStatus(f.r(kProduct), TemperatureSampleStatus::BusFault);
    assertIdentity(f.r(kProduct), kRomA);  // 4b: zuletzt gesehenes ROM
    const uint32_t conversionsBefore = f.bus1.conversionCount();
    f.cycle();
    TEST_ASSERT_EQUAL_UINT32(conversionsBefore, f.bus1.conversionCount());

    f.bus1.addDevice(kRomC, 1.0);
    f.bus1.addDevice(kRomD, 1.0);
    f.bus1.addDevice(kRomE, 1.0);
    f.cycle();
    assertStatus(f.r(kProduct), TemperatureSampleStatus::BusFault);
    TEST_ASSERT_TRUE(f.engine.report().singleDeviceBus.enumeration.overflow);
}

void test_single_device_absence_keeps_last_seen_rom_and_recovers() {
    Fixture f;
    f.bus1.addDevice(kRomA, 30.0);
    f.cycle();
    f.bus1.removeDevice(kRomA);
    f.cycle();
    assertStatus(f.r(kProduct), TemperatureSampleStatus::MissingSample);
    assertIdentity(f.r(kProduct), kRomA);
    // Wiedererscheinen: 12 Bit wird erneut gesetzt, erste frische Probe ist Ok.
    const uint32_t resolutionBefore = f.bus1.resolutionCalls();
    f.bus1.addDevice(kRomA, 31.0);
    f.cycle();
    assertStatus(f.r(kProduct), TemperatureSampleStatus::Ok);
    TEST_ASSERT_EQUAL_DOUBLE(31.0, f.r(kProduct).celsius().value());
    TEST_ASSERT_EQUAL_UINT32(resolutionBefore + 1U, f.bus1.resolutionCalls());
}

void test_single_device_rom_change_reports_the_new_identity() {
    Fixture f;
    f.bus1.addDevice(kRomA, 30.0);
    f.cycle();
    f.bus1.removeDevice(kRomA);
    f.cycle();
    f.bus1.addDevice(kRomB, 25.0);
    f.cycle();
    assertStatus(f.r(kProduct), TemperatureSampleStatus::Ok);
    assertIdentity(f.r(kProduct), kRomB);
    const auto report = f.engine.report();
    TEST_ASSERT_TRUE(report.singleDeviceLastSeen.has_value());
    TEST_ASSERT_EQUAL_UINT64(kRomB, *report.singleDeviceLastSeen);
}

// ---------------------------------------------------------------------------
// SIM-30-04 Zeit, Frische
// ---------------------------------------------------------------------------

void test_cycle_timing_freshness_and_monotonic_timestamps() {
    Fixture f;
    f.bind(kRomA, kRomB);
    f.bus0.addDevice(kRomA, 20.0);
    f.bus0.addDevice(kRomB, 5.0);
    f.bus1.addDevice(kRomC, 30.0);
    // Vor dem ersten Schritt: kein Wert, Zeitstempel 0.
    assertStatus(f.r(0U), TemperatureSampleStatus::MissingSample);
    TEST_ASSERT_EQUAL_UINT64(0U, f.r(0U).monotonicTimestampMs());

    TEST_ASSERT_EQUAL_UINT64(0U, f.engine.nextDueMillis());
    f.engine.step();  // beginnt den Zyklus bei t=0 (Konvertierung gestartet)
    TEST_ASSERT_EQUAL_UINT64(Ds18b20SamplingEngine::kConversionWaitMs,
                             f.engine.nextDueMillis());
    // Vor Ablauf der Wartezeit entsteht keine Probe und wird nichts gelesen.
    f.time.advanceMonotonicMillis(Ds18b20SamplingEngine::kConversionWaitMs -
                                  1U);
    f.engine.step();
    TEST_ASSERT_EQUAL_UINT32(0U, f.bus0.readCount());
    TEST_ASSERT_EQUAL_UINT64(0U, f.engine.completedCycles());
    f.time.advanceMonotonicMillis(1U);
    f.engine.step();
    TEST_ASSERT_EQUAL_UINT64(1U, f.engine.completedCycles());
    TEST_ASSERT_EQUAL_UINT64(Ds18b20SamplingEngine::kConversionWaitMs,
                             f.r(0U).monotonicTimestampMs());
    // Naechster Zyklus nach 2000 ms ab Beginn; ein zweiter step() am selben
    // Zeitpunkt veroeffentlicht nichts erneut.
    TEST_ASSERT_EQUAL_UINT64(Ds18b20SamplingEngine::kCyclePeriodMs,
                             f.engine.nextDueMillis());
    f.engine.step();
    TEST_ASSERT_EQUAL_UINT32(3U, f.bus0.readCount() + f.bus1.readCount());

    uint64_t previous = f.r(0U).monotonicTimestampMs();
    for (int i = 0; i < 10; ++i) {
        f.cycle();
        TEST_ASSERT_TRUE(f.r(0U).monotonicTimestampMs() > previous);
        previous = f.r(0U).monotonicTimestampMs();
    }
    // Nie ein Lesen ohne frische Konvertierung.
    TEST_ASSERT_EQUAL_UINT32(0U, f.bus0.staleReadCount());
    TEST_ASSERT_EQUAL_UINT32(0U, f.bus1.staleReadCount());
}

void test_late_scheduling_does_not_burst_cycles() {
    Fixture f;
    f.bind(kRomA, kRomB);
    f.bus0.addDevice(kRomA, 20.0);
    f.bus0.addDevice(kRomB, 5.0);
    f.cycle();
    f.time.advanceMonotonicMillis(60'000U);  // Task war lange nicht geschedult
    f.engine.step();                         // genau ein neuer Zyklus beginnt
    f.time.advanceMonotonicMillis(Ds18b20SamplingEngine::kConversionWaitMs);
    f.engine.step();
    TEST_ASSERT_EQUAL_UINT64(2U, f.engine.completedCycles());
    TEST_ASSERT_TRUE(f.engine.nextDueMillis() >= f.time.monotonicMillis());
}

void test_engine_never_stepped_is_fail_closed_and_invalid_channel_is_missing() {
    Fixture f;
    f.bind(kRomA, kRomB);
    f.bus0.addDevice(kRomA, 20.0);
    for (uint8_t channel = 0U; channel < Ds18b20SamplingEngine::kChannelCount;
         ++channel) {
        assertStatus(f.r(channel), TemperatureSampleStatus::MissingSample);
        assertIdentity(f.r(channel), std::nullopt);
    }
    assertStatus(f.r(7U), TemperatureSampleStatus::MissingSample);
    const Ds18b20ChannelSource source(f.engine, 0U);
    assertStatus(source.read(), TemperatureSampleStatus::MissingSample);
}

void test_channel_source_returns_the_engine_reading() {
    Fixture f;
    f.bind(kRomA, kRomB);
    f.bus0.addDevice(kRomA, 20.0);
    f.bus0.addDevice(kRomB, 5.0);
    f.cycle();
    const Ds18b20ChannelSource source0(f.engine, 0U);
    const Ds18b20ChannelSource source1(f.engine, 1U);
    TEST_ASSERT_EQUAL_DOUBLE(20.0, source0.read().celsius().value());
    TEST_ASSERT_EQUAL_DOUBLE(5.0, source1.read().celsius().value());
}

// ---------------------------------------------------------------------------
// SIM-30-05 Zusammenspiel mit der echten SensorQualityPipeline (4b)
// ---------------------------------------------------------------------------

// Fuehrt einen Zyklus aus und speist den SingleDevice-Kanal in die Pipeline.
void cycleAndIngest(Fixture& f, SensorQualityPipeline& pipeline) {
    f.cycle();
    (void)pipeline.ingest(f.r(kProduct), f.time.monotonicMillis());
}

enum class Gap { Absent, BusFault, CrcFault, MultiDevice };

// Bringt den Kanal fuer einen Zyklus in den Luckenzustand (Ok(A) -> Gap).
void enterGap(Fixture& f, Gap gap) {
    switch (gap) {
        case Gap::Absent:
            f.bus1.removeDevice(kRomA);
            break;
        case Gap::BusFault:
            f.bus1.overridePresence(Ds18b20DriverResult::Timeout);
            break;
        case Gap::CrcFault:
            f.bus1.setReadResult(kRomA, Ds18b20DriverResult::InvalidCrc);
            break;
        case Gap::MultiDevice:
            f.bus1.addDevice(kRomE, 1.0);
            break;
    }
}

void leaveGapToSensor(Fixture& f, Gap gap, OneWireRom rom, double celsius) {
    f.bus1.overridePresence(std::nullopt);
    f.bus1.setReadResult(kRomA, Ds18b20DriverResult::Ok);
    f.bus1.removeDevice(kRomE);
    if (rom != kRomA) f.bus1.removeDevice(kRomA);
    if (gap != Gap::Absent || rom != kRomA)
        f.bus1.addDevice(rom, celsius);
    else
        f.bus1.addDevice(kRomA, celsius);
    if (rom == kRomA) f.bus1.setCelsius(kRomA, celsius);
}

void run_rom_change_after_gap(Gap gap, bool calibrateNewRom) {
    Fixture f;
    SensorQualityPipeline pipeline(makeConfig());
    SensorQualityPipeline reference(makeConfig());
    const auto calibration = calibrateNewRom ? makeCalibration(kRomB, -0.75)
                                             : makeCalibration(kRomA, 0.5);
    pipeline.setCalibration(calibration);
    reference.setCalibration(calibration);
    f.bus1.addDevice(kRomA, 20.0);
    for (int i = 0; i < 4; ++i) cycleAndIngest(f, pipeline);
    TEST_ASSERT_TRUE(pipeline.snapshot(f.time.monotonicMillis()).quality ==
                     SensorQuality::Valid);

    enterGap(f, gap);
    cycleAndIngest(f, pipeline);
    assertIdentity(f.r(kProduct),
                   kRomA);  // 4b: Identitaet bleibt in der Luecke

    leaveGapToSensor(f, gap, kRomB, 30.0);
    cycleAndIngest(f, pipeline);
    auto snap = pipeline.snapshot(f.time.monotonicMillis());
    // Erste B-Probe = ROM-Wechsel: Filter-Reset, Recovery beginnt neu.
    TEST_ASSERT_TRUE(snap.lastFaultReason ==
                     SensorFaultReason::IdentityMismatch);
    TEST_ASSERT_EQUAL_UINT16(0U, snap.recoveryProgressCount);
    TEST_ASSERT_FALSE(snap.filteredCelsius.has_value());

    for (int i = 0; i < 3; ++i) cycleAndIngest(f, pipeline);
    snap = pipeline.snapshot(f.time.monotonicMillis());
    // Von Hand gebaute Referenzfolge ohne Adapter: Ok(A) x4, Luecke(A), Ok(B)
    // x4.
    for (int cycle = 0; cycle < 4; ++cycle) {
        refIngest(reference, cycle, TemperatureSampleStatus::Ok, kRomA, 20.0);
    }
    refIngest(reference, 4, gapStatus(static_cast<int>(gap)), kRomA,
              std::nullopt);
    for (int cycle = 5; cycle < 9; ++cycle) {
        refIngest(reference, cycle, TemperatureSampleStatus::Ok, kRomB, 30.0);
    }
    assertSnapshotsEqual(snap, reference.snapshot(f.time.monotonicMillis()));
    TEST_ASSERT_TRUE(snap.identity.has_value());
    TEST_ASSERT_EQUAL_UINT64(kRomB, snap.identity->value());
    TEST_ASSERT_TRUE(snap.filteredCelsius.has_value());
    // Kein Filterwert von A (20 Grad): der Filter startet bei B.
    const double expectedOffset = calibrateNewRom ? -0.75 : 0.0;
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 30.0 + expectedOffset,
                              *snap.filteredCelsius);
    // Der Offset von A wird nie auf B angewandt.
    TEST_ASSERT_EQUAL(calibrateNewRom, snap.appliedOffset.has_value());
    if (calibrateNewRom) {
        TEST_ASSERT_DOUBLE_WITHIN(1e-9, -0.75, *snap.appliedOffset);
    }
}

void test_ok_a_gap_ok_b_resets_filter_for_every_gap_kind() {
    for (const Gap gap :
         {Gap::Absent, Gap::BusFault, Gap::CrcFault, Gap::MultiDevice}) {
        run_rom_change_after_gap(gap, /*calibrateNewRom=*/false);
        run_rom_change_after_gap(gap, /*calibrateNewRom=*/true);
    }
}

void run_same_rom_after_gap(Gap gap) {
    Fixture f;
    SensorQualityPipeline pipeline(makeConfig());
    pipeline.setCalibration(makeCalibration(kRomA, 0.5));
    // Von Hand gebaute Referenzfolge ohne Adapter.
    SensorQualityPipeline reference(makeConfig());
    reference.setCalibration(makeCalibration(kRomA, 0.5));
    f.bus1.addDevice(kRomA, 20.0);
    for (int i = 0; i < 4; ++i) cycleAndIngest(f, pipeline);
    enterGap(f, gap);
    cycleAndIngest(f, pipeline);
    leaveGapToSensor(f, gap, kRomA, 22.0);
    for (int i = 0; i < 4; ++i) cycleAndIngest(f, pipeline);
    const auto snap = pipeline.snapshot(f.time.monotonicMillis());
    for (int cycle = 0; cycle < 4; ++cycle) {
        refIngest(reference, cycle, TemperatureSampleStatus::Ok, kRomA, 20.0);
    }
    refIngest(reference, 4, gapStatus(static_cast<int>(gap)), kRomA,
              std::nullopt);
    for (int cycle = 5; cycle < 9; ++cycle) {
        refIngest(reference, cycle, TemperatureSampleStatus::Ok, kRomA, 22.0);
    }
    assertSnapshotsEqual(snap, reference.snapshot(f.time.monotonicMillis()));
    // Kein Mismatch; derselbe Sensor behaelt Filterverlauf und Offset.
    TEST_ASSERT_TRUE(snap.lastFaultReason !=
                     SensorFaultReason::IdentityMismatch);
    TEST_ASSERT_TRUE(snap.appliedOffset.has_value());
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 0.5, *snap.appliedOffset);
    TEST_ASSERT_TRUE(snap.filteredCelsius.has_value());
    // Der Filter laeuft aus der A-Historie weiter (zwischen 20,5 und 22,5).
    TEST_ASSERT_TRUE(*snap.filteredCelsius > 20.5);
    TEST_ASSERT_TRUE(*snap.filteredCelsius < 22.5);
}

void test_ok_a_gap_ok_a_keeps_filter_history_and_offset() {
    for (const Gap gap :
         {Gap::Absent, Gap::BusFault, Gap::CrcFault, Gap::MultiDevice}) {
        run_same_rom_after_gap(gap);
    }
}

void test_boot_missing_without_history_then_first_sensor_is_normal_start() {
    Fixture f;
    SensorQualityPipeline pipeline(makeConfig());
    cycleAndIngest(f, pipeline);  // kein Geraet: Missing(leer)
    assertIdentity(f.r(kProduct), std::nullopt);
    f.bus1.addDevice(kRomA, 20.0);
    for (int i = 0; i < 3; ++i) cycleAndIngest(f, pipeline);
    const auto snap = pipeline.snapshot(f.time.monotonicMillis());
    TEST_ASSERT_TRUE(snap.lastFaultReason !=
                     SensorFaultReason::IdentityMismatch);
    TEST_ASSERT_TRUE(snap.identity.has_value());
}

void test_non_ok_readings_never_lose_the_known_identity() {
    Fixture f;
    f.bind(kRomA, kRomB);
    f.bus0.addDevice(kRomA, 20.0);
    f.bus0.addDevice(kRomB, 5.0);
    f.bus1.addDevice(kRomC, 30.0);
    f.cycle();
    const std::array<Ds18b20DriverResult, 3> faults{
        {Ds18b20DriverResult::Timeout, Ds18b20DriverResult::NotFound,
         Ds18b20DriverResult::Other}};
    for (const auto fault : faults) {
        f.bus0.overridePresence(fault);
        f.bus1.overridePresence(fault);
        f.cycle();
        assertIdentity(f.r(0U), kRomA);
        assertIdentity(f.r(1U), kRomB);
        assertIdentity(f.r(kProduct), kRomC);
    }
}

void test_task_stall_leaves_a_non_valid_pipeline_state() {
    Fixture f;
    SensorQualityPipeline pipeline(makeConfig());
    f.bus1.addDevice(kRomA, 20.0);
    for (int i = 0; i < 4; ++i) cycleAndIngest(f, pipeline);
    TEST_ASSERT_TRUE(pipeline.snapshot(f.time.monotonicMillis()).quality ==
                     SensorQuality::Valid);
    // Der Task laeuft nicht mehr: keine neue Probe, die Zeit schreitet voran.
    f.time.advanceMonotonicMillis(30'000U);
    const auto stalled = pipeline.snapshot(f.time.monotonicMillis());
    TEST_ASSERT_TRUE(stalled.quality != SensorQuality::Valid);
}

}  // namespace

void setUp() {}
void tearDown() {}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_unbound_expected_rom_channels_never_ok_and_have_no_identity);
    RUN_TEST(test_invalid_bindings_stay_unbound);
    RUN_TEST(
        test_first_ok_after_verified_binding_fresh_conversion_and_resolution);
    RUN_TEST(test_channel_mapping_follows_rom_not_enumeration_order);
    RUN_TEST(test_missing_expected_rom_affects_only_its_channel);
    RUN_TEST(test_unknown_extra_rom_is_binding_conflict_for_both_channels);
    RUN_TEST(test_more_than_four_devices_is_a_binding_conflict);
    RUN_TEST(test_bus_wide_failures_hit_both_expected_rom_channels);
    RUN_TEST(test_single_sensor_faults_do_not_affect_the_other_channel);
    RUN_TEST(test_power_on_value_is_never_a_measurement);
    RUN_TEST(test_start_conversion_and_set_resolution_failures_are_typed);
    RUN_TEST(test_bus_faults_are_independent_between_the_two_buses);
    RUN_TEST(test_single_device_bus_zero_one_two_and_overflow);
    RUN_TEST(test_single_device_absence_keeps_last_seen_rom_and_recovers);
    RUN_TEST(test_single_device_rom_change_reports_the_new_identity);
    RUN_TEST(test_cycle_timing_freshness_and_monotonic_timestamps);
    RUN_TEST(test_late_scheduling_does_not_burst_cycles);
    RUN_TEST(
        test_engine_never_stepped_is_fail_closed_and_invalid_channel_is_missing);
    RUN_TEST(test_channel_source_returns_the_engine_reading);
    RUN_TEST(test_ok_a_gap_ok_b_resets_filter_for_every_gap_kind);
    RUN_TEST(test_ok_a_gap_ok_a_keeps_filter_history_and_offset);
    RUN_TEST(
        test_boot_missing_without_history_then_first_sensor_is_normal_start);
    RUN_TEST(test_non_ok_readings_never_lose_the_known_identity);
    RUN_TEST(test_task_stall_leaves_a_non_valid_pipeline_state);
    return UNITY_END();
}
