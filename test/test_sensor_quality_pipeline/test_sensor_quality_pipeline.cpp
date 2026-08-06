#include <unity.h>

#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>

#include "sensor_identity.hpp"
#include "sensor_limits.hpp"
#include "sensor_quality.hpp"
#include "sensor_quality_config.hpp"
#include "sensor_quality_pipeline.hpp"
#include "sensor_quality_snapshot.hpp"
#include "temperature_source.hpp"

// Slice 1 (docs/tasks/issue-20-sensor-quality-filtering-plan.md, Abschnitt
// 20): deckt Start/Normalbetrieb, Transport-/Messfehler, Zeit/Alter/
// Disposition, Wertebereich, Zustandsmaschine/Wiedererkennung und
// Robustheit ab. Median-/Tiefpass-/Kalibrierungsintegration, die
// vollstaendige "verspaetete Probe erkennt Failed innerhalb von ingest()"-
// Filterreset-Feinunterscheidung sowie die vollstaendige ROM-Wechsel-
// Filterreset-Verifikation folgen mit Slice 2 (Abschnitt 20, Schritt 18:
// "Filterreset-Feinunterscheidung" als expliziter Slice-2-Testzusatz).

namespace {

using device_platform::SampleDisposition;
using device_platform::SensorFaultReason;
using device_platform::SensorIdentity;
using device_platform::SensorQuality;
using device_platform::SensorQualityConfig;
using device_platform::SensorQualityPipeline;
using device_platform::SensorQualitySnapshot;
using device_platform::TemperatureReading;
using device_platform::TemperatureSampleStatus;

// Lokale, nicht exportierte Testhilfe fuer geskriptete TemperatureReading-
// Folgen (Abschnitt 17a) - bewusst nur hier definiert, kein eigenes
// device_platform_test_support-Modul (KISS/YAGNI, siehe Plan).

SensorQualityConfig makeTestConfig() {
    return SensorQualityConfig::create(
               /*medianWindowSize=*/3U, /*lowPassTauSeconds=*/5.0,
               /*minPlausibleCelsius=*/-20.0, /*maxPlausibleCelsius=*/80.0,
               /*maxRateOfChangeCelsiusPerSecond=*/5.0,
               /*maxStaleAgeMs=*/10'000U, /*maxConsecutiveInvalid=*/3U,
               /*minConsecutiveValidSamples=*/2U,
               /*minRecoveryStabilityDurationMs=*/2'000U)
        .config.value();
}

TemperatureReading okReading(std::optional<SensorIdentity> identity,
                             uint64_t timestampMs, double celsius) {
    return TemperatureReading::create(identity, timestampMs,
                                      TemperatureSampleStatus::Ok, celsius)
        .reading.value();
}

TemperatureReading faultReading(std::optional<SensorIdentity> identity,
                                uint64_t timestampMs,
                                TemperatureSampleStatus status) {
    return TemperatureReading::create(identity, timestampMs, status,
                                      std::nullopt)
        .reading.value();
}

// Zwei valide, aufeinanderfolgende Proben im Abstand der konfigurierten
// Stabilitaetszeit - erreicht Valid unter makeTestConfig(). Enthaelt bewusst
// KEINE TEST_ASSERT-Aufrufe (Unity-Makros duerfen nicht ueber Stackrahmen
// mit nicht-trivialen Destruktoren hinweg longjmp'en).
SensorQualityPipeline warmedUpValidPipeline() {
    SensorQualityPipeline pipeline(makeTestConfig());
    (void)pipeline.ingest(okReading(std::nullopt, 0U, 20.0), 0U);
    (void)pipeline.ingest(okReading(std::nullopt, 2000U, 21.0), 2000U);
    return pipeline;
}

}  // namespace

// ---------------------------------------------------------------------
// Start/Normalbetrieb
// ---------------------------------------------------------------------

void test_start_without_any_sample_is_stale_with_no_derived_values() {
    const SensorQualityPipeline pipeline(makeTestConfig());

    const SensorQualitySnapshot snapshot = pipeline.snapshot(0U);

    TEST_ASSERT_TRUE(snapshot.quality == SensorQuality::Stale);
    TEST_ASSERT_FALSE(snapshot.identity.has_value());
    TEST_ASSERT_FALSE(snapshot.rawCelsius.has_value());
    TEST_ASSERT_FALSE(snapshot.correctedCelsius.has_value());
    TEST_ASSERT_FALSE(snapshot.filteredCelsius.has_value());
    TEST_ASSERT_FALSE(snapshot.appliedOffset.has_value());
    TEST_ASSERT_FALSE(snapshot.lastAcceptedSampleAgeMs.has_value());
    TEST_ASSERT_FALSE(snapshot.lastValidSampleAgeMs.has_value());
    TEST_ASSERT_FALSE(snapshot.changeRateCelsiusPerSecond.has_value());
    TEST_ASSERT_TRUE(snapshot.lastFaultReason == SensorFaultReason::None);
    TEST_ASSERT_EQUAL_UINT16(0U, snapshot.consecutiveInvalidCount);
    TEST_ASSERT_EQUAL_UINT16(0U, snapshot.recoveryProgressCount);
}

void test_first_valid_sample_stays_stale_but_updates_raw_evidence() {
    SensorQualityPipeline pipeline(makeTestConfig());

    const auto disposition =
        pipeline.ingest(okReading(std::nullopt, 0U, 20.0), 0U);

    TEST_ASSERT_TRUE(disposition == SampleDisposition::Accepted);
    const auto snapshot = pipeline.snapshot(0U);
    TEST_ASSERT_TRUE(snapshot.quality == SensorQuality::Stale);
    TEST_ASSERT_TRUE(snapshot.rawCelsius.has_value());
    TEST_ASSERT_EQUAL_DOUBLE(20.0, snapshot.rawCelsius.value());
    TEST_ASSERT_TRUE(snapshot.lastAcceptedSampleAgeMs.has_value());
    TEST_ASSERT_EQUAL_UINT64(0U, snapshot.lastAcceptedSampleAgeMs.value());
    TEST_ASSERT_FALSE(snapshot.correctedCelsius.has_value());
    TEST_ASSERT_FALSE(snapshot.filteredCelsius.has_value());
}

void test_min_consecutive_valid_samples_reach_valid() {
    const SensorQualityPipeline pipeline = warmedUpValidPipeline();

    TEST_ASSERT_TRUE(pipeline.snapshot(2000U).quality == SensorQuality::Valid);
}

void test_valid_remains_stable_over_continued_cycle() {
    SensorQualityPipeline pipeline = warmedUpValidPipeline();

    (void)pipeline.ingest(okReading(std::nullopt, 4000U, 21.5), 4000U);
    (void)pipeline.ingest(okReading(std::nullopt, 6000U, 22.0), 6000U);

    TEST_ASSERT_TRUE(pipeline.snapshot(6000U).quality == SensorQuality::Valid);
}

// ---------------------------------------------------------------------
// Transport-/Messfehler
// ---------------------------------------------------------------------

void test_single_crc_fault_drops_to_stale_not_immediately_failed() {
    SensorQualityPipeline pipeline = warmedUpValidPipeline();

    (void)pipeline.ingest(
        faultReading(std::nullopt, 4000U, TemperatureSampleStatus::CrcFault),
        4000U);

    const auto snapshot = pipeline.snapshot(4000U);
    TEST_ASSERT_TRUE(snapshot.quality == SensorQuality::Stale);
    TEST_ASSERT_TRUE(snapshot.lastFaultReason == SensorFaultReason::CrcFault);
    TEST_ASSERT_EQUAL_UINT16(1U, snapshot.consecutiveInvalidCount);
}

void test_consecutive_crc_faults_beyond_ceiling_reach_failed() {
    SensorQualityPipeline pipeline(makeTestConfig());

    (void)pipeline.ingest(
        faultReading(std::nullopt, 1000U, TemperatureSampleStatus::CrcFault),
        1000U);
    (void)pipeline.ingest(
        faultReading(std::nullopt, 2000U, TemperatureSampleStatus::CrcFault),
        2000U);
    (void)pipeline.ingest(
        faultReading(std::nullopt, 3000U, TemperatureSampleStatus::CrcFault),
        3000U);
    TEST_ASSERT_TRUE(pipeline.snapshot(3000U).quality != SensorQuality::Failed);

    (void)pipeline.ingest(
        faultReading(std::nullopt, 4000U, TemperatureSampleStatus::CrcFault),
        4000U);

    const auto snapshot = pipeline.snapshot(4000U);
    TEST_ASSERT_TRUE(snapshot.quality == SensorQuality::Failed);
    TEST_ASSERT_EQUAL_UINT16(4U, snapshot.consecutiveInvalidCount);
}

void test_bus_fault_is_recorded_with_its_own_reason() {
    SensorQualityPipeline pipeline = warmedUpValidPipeline();

    (void)pipeline.ingest(
        faultReading(std::nullopt, 4000U, TemperatureSampleStatus::BusFault),
        4000U);

    TEST_ASSERT_TRUE(pipeline.snapshot(4000U).lastFaultReason ==
                     SensorFaultReason::BusFault);
}

void test_missing_sample_is_recorded_with_its_own_reason() {
    SensorQualityPipeline pipeline = warmedUpValidPipeline();

    (void)pipeline.ingest(faultReading(std::nullopt, 4000U,
                                       TemperatureSampleStatus::MissingSample),
                          4000U);

    TEST_ASSERT_TRUE(pipeline.snapshot(4000U).lastFaultReason ==
                     SensorFaultReason::MissingSample);
}

void test_known_invalid_measurement_is_recorded_with_its_own_reason() {
    SensorQualityPipeline pipeline = warmedUpValidPipeline();

    (void)pipeline.ingest(
        faultReading(std::nullopt, 4000U,
                     TemperatureSampleStatus::KnownInvalidMeasurement),
        4000U);

    TEST_ASSERT_TRUE(pipeline.snapshot(4000U).lastFaultReason ==
                     SensorFaultReason::KnownInvalidMeasurement);
}

void test_nonfinite_celsius_is_nonfinite_not_out_of_range() {
    SensorQualityPipeline pipeline = warmedUpValidPipeline();

    (void)pipeline.ingest(okReading(std::nullopt, 4000U,
                                    std::numeric_limits<double>::quiet_NaN()),
                          4000U);

    TEST_ASSERT_TRUE(pipeline.snapshot(4000U).lastFaultReason ==
                     SensorFaultReason::NonFinite);
}

// ---------------------------------------------------------------------
// Zeit und Alter / Disposition
// ---------------------------------------------------------------------

void test_retrograde_timestamp_is_rejected_and_state_unchanged() {
    SensorQualityPipeline pipeline(makeTestConfig());
    (void)pipeline.ingest(okReading(std::nullopt, 1000U, 20.0), 1000U);

    const auto disposition =
        pipeline.ingest(okReading(std::nullopt, 500U, 30.0), 1000U);

    TEST_ASSERT_TRUE(disposition == SampleDisposition::RejectedRetrograde);
    const auto snapshot = pipeline.snapshot(1000U);
    TEST_ASSERT_EQUAL_DOUBLE(20.0, snapshot.rawCelsius.value());
    TEST_ASSERT_EQUAL_UINT16(0U, snapshot.consecutiveInvalidCount);
}

void test_future_timestamp_is_rejected() {
    SensorQualityPipeline pipeline(makeTestConfig());
    (void)pipeline.ingest(okReading(std::nullopt, 1000U, 20.0), 1000U);

    const auto disposition =
        pipeline.ingest(okReading(std::nullopt, 5000U, 21.0), 2000U);

    TEST_ASSERT_TRUE(disposition == SampleDisposition::RejectedFuture);
    TEST_ASSERT_EQUAL_UINT16(0U,
                             pipeline.snapshot(2000U).consecutiveInvalidCount);
}

void test_exact_duplicate_is_ignored() {
    SensorQualityPipeline pipeline(makeTestConfig());
    (void)pipeline.ingest(okReading(std::nullopt, 1000U, 20.0), 1000U);

    const auto disposition =
        pipeline.ingest(okReading(std::nullopt, 1000U, 20.0), 1000U);

    TEST_ASSERT_TRUE(disposition == SampleDisposition::DuplicateIgnored);
    TEST_ASSERT_EQUAL_UINT16(0U,
                             pipeline.snapshot(1000U).consecutiveInvalidCount);
}

void test_same_timestamp_different_value_is_conflict() {
    SensorQualityPipeline pipeline(makeTestConfig());
    (void)pipeline.ingest(okReading(std::nullopt, 1000U, 20.0), 1000U);

    const auto disposition =
        pipeline.ingest(okReading(std::nullopt, 1000U, 21.0), 1000U);

    TEST_ASSERT_TRUE(disposition ==
                     SampleDisposition::RejectedTimestampConflict);
}

void test_same_timestamp_same_value_different_identity_is_conflict_not_duplicate() {
    SensorQualityPipeline pipeline(makeTestConfig());
    const auto identityA = SensorIdentity::create(1U).identity;
    const auto identityB = SensorIdentity::create(2U).identity;
    (void)pipeline.ingest(okReading(identityA, 1000U, 20.0), 1000U);

    const auto disposition =
        pipeline.ingest(okReading(identityB, 1000U, 20.0), 1000U);

    TEST_ASSERT_TRUE(disposition ==
                     SampleDisposition::RejectedTimestampConflict);
}

void test_same_timestamp_both_nan_is_duplicate() {
    SensorQualityPipeline pipeline(makeTestConfig());
    const double nan = std::numeric_limits<double>::quiet_NaN();
    (void)pipeline.ingest(okReading(std::nullopt, 1000U, nan), 1000U);

    const auto disposition =
        pipeline.ingest(okReading(std::nullopt, 1000U, nan), 1000U);

    TEST_ASSERT_TRUE(disposition == SampleDisposition::DuplicateIgnored);
}

void test_same_timestamp_nan_versus_finite_is_conflict() {
    SensorQualityPipeline pipeline(makeTestConfig());
    (void)pipeline.ingest(okReading(std::nullopt, 1000U,
                                    std::numeric_limits<double>::quiet_NaN()),
                          1000U);

    const auto disposition =
        pipeline.ingest(okReading(std::nullopt, 1000U, 5.0), 1000U);

    TEST_ASSERT_TRUE(disposition ==
                     SampleDisposition::RejectedTimestampConflict);
}

void test_very_first_sample_is_accepted() {
    SensorQualityPipeline pipeline(makeTestConfig());

    const auto disposition =
        pipeline.ingest(okReading(std::nullopt, 500U, 20.0), 900U);

    TEST_ASSERT_TRUE(disposition == SampleDisposition::Accepted);
}

void test_very_first_sample_from_the_future_is_rejected() {
    SensorQualityPipeline pipeline(makeTestConfig());

    const auto disposition =
        pipeline.ingest(okReading(std::nullopt, 5000U, 20.0), 1000U);

    TEST_ASSERT_TRUE(disposition == SampleDisposition::RejectedFuture);
    TEST_ASSERT_FALSE(pipeline.snapshot(1000U).rawCelsius.has_value());
}

void test_large_gap_transitions_to_failed_via_snapshot_age() {
    SensorQualityPipeline pipeline = warmedUpValidPipeline();
    TEST_ASSERT_TRUE(pipeline.snapshot(2000U).quality == SensorQuality::Valid);

    // Keine weiteren ingest()-Aufrufe - reiner snapshot(now)-Vorlauf weit
    // jenseits von kMaxStaleAgeMs (10 s) seit der letzten gueltigen Probe.
    const auto snapshot = pipeline.snapshot(999'999U);

    TEST_ASSERT_TRUE(snapshot.quality == SensorQuality::Failed);
}

void test_resumption_after_gap_is_not_evaluated_as_a_jump() {
    SensorQualityPipeline pipeline(makeTestConfig());
    (void)pipeline.ingest(okReading(std::nullopt, 0U, 20.0), 0U);

    // Grosse Luecke (1 Stunde), danach ein deutlich anderer, aber immer noch
    // absolut plausibler Wert - darf NICHT als Sprung (RateOfChangeExceeded)
    // bewertet werden, da kein "unmittelbar vorheriger" Wert im Sinne einer
    // kurzen Zeitspanne existiert (Abschnitt 9b).
    const uint64_t oneHourMs = 3'600'000U;
    const auto disposition =
        pipeline.ingest(okReading(std::nullopt, oneHourMs, 60.0), oneHourMs);

    TEST_ASSERT_TRUE(disposition == SampleDisposition::Accepted);
    const auto snapshot = pipeline.snapshot(oneHourMs);
    TEST_ASSERT_TRUE(snapshot.lastFaultReason == SensorFaultReason::None);
    // Verschaerfung (Nachkorrektur PR #95): nicht nur "kein Sprung erkannt",
    // sondern explizit belegt, dass ueberhaupt KEINE Aenderungsrate
    // berechnet wurde - eine Stunde Luecke liegt weit jenseits kMaxStaleAgeMs
    // und macht diese Probe zum altersbedingten Vorzustand Failed (9a), der
    // die Aenderungsratenreferenz verwirft (10.2), statt zufaellig eine kleine
    // Rate ueber die grosse Zeitspanne zu berechnen.
    TEST_ASSERT_FALSE(snapshot.changeRateCelsiusPerSecond.has_value());
}

void test_first_plausible_sample_after_single_fault_has_no_rate() {
    SensorQualityPipeline pipeline(makeTestConfig());
    (void)pipeline.ingest(okReading(std::nullopt, 0U, 20.0), 0U);
    (void)pipeline.ingest(
        faultReading(std::nullopt, 1000U, TemperatureSampleStatus::CrcFault),
        1000U);

    // Der naechste gueltige Wert darf NICHT gegen den Wert vor der Luecke
    // geprueft werden - die "unmittelbar vorherige gueltige Probe"-
    // Eigenschaft ist durch die dazwischenliegende ungueltige Probe
    // gebrochen (Abschnitt 10.2). 20.0 -> 75.0 in 200 ms waere weit
    // jenseits von kMaxRateOfChangeCelsiusPerSecond, wenn geprueft.
    (void)pipeline.ingest(okReading(std::nullopt, 1200U, 75.0), 1200U);

    const auto snapshot = pipeline.snapshot(1200U);
    TEST_ASSERT_FALSE(snapshot.changeRateCelsiusPerSecond.has_value());
    TEST_ASSERT_TRUE(snapshot.lastFaultReason == SensorFaultReason::None);
}

void test_first_plausible_sample_after_identity_change_has_no_rate() {
    SensorQualityPipeline pipeline(makeTestConfig());
    const auto identityA = SensorIdentity::create(1U).identity;
    const auto identityB = SensorIdentity::create(2U).identity;
    (void)pipeline.ingest(okReading(identityA, 0U, 20.0), 0U);
    (void)pipeline.ingest(okReading(identityB, 1000U, 20.5), 1000U);
    TEST_ASSERT_TRUE(pipeline.snapshot(1000U).lastFaultReason ==
                     SensorFaultReason::IdentityMismatch);

    // Erste plausible Probe des neuen (ROM-gewechselten) Sensors darf nicht
    // gegen den Wert des alten Sensors geprueft werden.
    (void)pipeline.ingest(okReading(identityB, 1200U, 75.0), 1200U);

    TEST_ASSERT_FALSE(
        pipeline.snapshot(1200U).changeRateCelsiusPerSecond.has_value());
}

// ---------------------------------------------------------------------
// Wertebereich und Dynamik
// ---------------------------------------------------------------------

void test_value_below_absolute_minimum_is_out_of_range() {
    SensorQualityPipeline pipeline(makeTestConfig());

    (void)pipeline.ingest(
        okReading(std::nullopt, 0U,
                  device_platform::sensor_limits::kAbsoluteMinCelsius - 1.0),
        0U);

    TEST_ASSERT_TRUE(pipeline.snapshot(0U).lastFaultReason ==
                     SensorFaultReason::OutOfRange);
}

void test_value_above_absolute_maximum_is_out_of_range() {
    SensorQualityPipeline pipeline(makeTestConfig());

    (void)pipeline.ingest(
        okReading(std::nullopt, 0U,
                  device_platform::sensor_limits::kAbsoluteMaxCelsius + 1.0),
        0U);

    TEST_ASSERT_TRUE(pipeline.snapshot(0U).lastFaultReason ==
                     SensorFaultReason::OutOfRange);
}

void test_value_within_absolute_but_outside_configured_range_is_rejected() {
    SensorQualityPipeline pipeline(makeTestConfig());

    // makeTestConfig(): minPlausibleCelsius=-20, maxPlausibleCelsius=80 -
    // enger als die firmwarefeste Aussengrenze [-40, 150]. 100.0 liegt
    // innerhalb der Aussengrenze, aber ausserhalb des konfigurierten Bands.
    (void)pipeline.ingest(okReading(std::nullopt, 0U, 100.0), 0U);

    TEST_ASSERT_TRUE(pipeline.snapshot(0U).lastFaultReason ==
                     SensorFaultReason::OutOfRange);
}

void test_same_absolute_delta_is_judged_differently_over_short_versus_long_interval() {
    SensorQualityPipeline shortInterval(makeTestConfig());
    (void)shortInterval.ingest(okReading(std::nullopt, 0U, 20.0), 0U);
    (void)shortInterval.ingest(okReading(std::nullopt, 200U, 23.0),
                               200U);  // 15 C/s
    TEST_ASSERT_TRUE(shortInterval.snapshot(200U).lastFaultReason ==
                     SensorFaultReason::RateOfChangeExceeded);

    SensorQualityPipeline longInterval(makeTestConfig());
    (void)longInterval.ingest(okReading(std::nullopt, 0U, 20.0), 0U);
    (void)longInterval.ingest(okReading(std::nullopt, 1000U, 23.0),
                              1000U);  // 3 C/s
    TEST_ASSERT_TRUE(longInterval.snapshot(1000U).lastFaultReason ==
                     SensorFaultReason::None);
}

void test_rate_exceeding_sample_does_not_stop_permanently() {
    SensorQualityPipeline pipeline(makeTestConfig());
    (void)pipeline.ingest(okReading(std::nullopt, 0U, 20.0), 0U);
    (void)pipeline.ingest(okReading(std::nullopt, 200U, 23.0),
                          200U);  // exceeds rate

    TEST_ASSERT_TRUE(pipeline.snapshot(200U).quality != SensorQuality::Failed);
}

// ---------------------------------------------------------------------
// Zustandsmaschine und Wiedererkennung
// ---------------------------------------------------------------------

void test_single_invalid_sample_drops_valid_to_stale() {
    SensorQualityPipeline pipeline = warmedUpValidPipeline();

    (void)pipeline.ingest(
        faultReading(std::nullopt, 4000U, TemperatureSampleStatus::CrcFault),
        4000U);

    TEST_ASSERT_TRUE(pipeline.snapshot(4000U).quality == SensorQuality::Stale);
}

void test_failed_stays_failed_during_incomplete_recovery() {
    SensorQualityPipeline pipeline(makeTestConfig());
    (void)pipeline.ingest(
        faultReading(std::nullopt, 1000U, TemperatureSampleStatus::CrcFault),
        1000U);
    (void)pipeline.ingest(
        faultReading(std::nullopt, 2000U, TemperatureSampleStatus::CrcFault),
        2000U);
    (void)pipeline.ingest(
        faultReading(std::nullopt, 3000U, TemperatureSampleStatus::CrcFault),
        3000U);
    (void)pipeline.ingest(
        faultReading(std::nullopt, 4000U, TemperatureSampleStatus::CrcFault),
        4000U);
    TEST_ASSERT_TRUE(pipeline.snapshot(4000U).quality == SensorQuality::Failed);

    // Eine einzelne gueltige Probe beginnt die Wiedererkennungsfolge, erfuellt
    // aber weder kMinConsecutiveValidSamples (2) noch
    // kMinRecoveryStabilityDurationMs (2 s) - die oeffentliche Qualitaet muss
    // Failed bleiben, nicht auf Stale zurueckfallen (Abschnitt 8).
    (void)pipeline.ingest(okReading(std::nullopt, 5000U, 20.0), 5000U);

    TEST_ASSERT_TRUE(pipeline.snapshot(5000U).quality == SensorQuality::Failed);
}

void test_first_sample_after_aged_failed_is_not_immediately_valid() {
    SensorQualityPipeline pipeline(makeTestConfig());
    (void)pipeline.ingest(
        faultReading(std::nullopt, 1000U, TemperatureSampleStatus::CrcFault),
        1000U);
    (void)pipeline.ingest(
        faultReading(std::nullopt, 2000U, TemperatureSampleStatus::CrcFault),
        2000U);
    (void)pipeline.ingest(
        faultReading(std::nullopt, 3000U, TemperatureSampleStatus::CrcFault),
        3000U);
    (void)pipeline.ingest(
        faultReading(std::nullopt, 4000U, TemperatureSampleStatus::CrcFault),
        4000U);
    TEST_ASSERT_TRUE(pipeline.snapshot(4000U).quality == SensorQuality::Failed);

    // Ein einzelnes gueltiges Sample beginnt eine Wiedererkennungsfolge
    // (recoveryProgressCount == 1), erreicht die geforderte Mindestanzahl (2)
    // noch nicht.
    (void)pipeline.ingest(okReading(std::nullopt, 5000U, 20.0), 5000U);
    TEST_ASSERT_TRUE(pipeline.snapshot(5000U).quality != SensorQuality::Valid);

    // Lange Stille (weit ueber kMaxStaleAgeMs), danach die naechste Probe:
    // OHNE die Vorzustandskorrektur (Abschnitt 9a, Nachkorrektur PR #95)
    // wuerde recoveryProgressCount_ von 1 auf 2 steigen und - weil
    // recoveryStreakStartTimestampMs_ (5000) laengst mehr als
    // kMinRecoveryStabilityDurationMs zurueckliegt - faelschlich SOFORT Valid
    // ergeben, obwohl der Sensor zwischenzeitlich laengst altersbedingt
    // Failed war.
    (void)pipeline.ingest(okReading(std::nullopt, 200'000U, 20.5), 200'000U);

    TEST_ASSERT_TRUE(pipeline.snapshot(200'000U).quality !=
                     SensorQuality::Valid);
    TEST_ASSERT_EQUAL_UINT16(1U,
                             pipeline.snapshot(200'000U).recoveryProgressCount);
}

void test_delayed_ordered_sample_detects_prior_failed_pre_state() {
    SensorQualityPipeline pipeline =
        warmedUpValidPipeline();  // Valid, letzte gueltige Probe bei t=2000

    // Zeitstempel 5000 ist geordnet (> letzter akzeptierter Zeitstempel
    // 2000), die Probe wird aber erst bei nowMonotonicMs = 100000 zugestellt
    // - weit jenseits kMaxStaleAgeMs (10 s) seit der letzten gueltigen Probe.
    // Der Vorzustand MUSS aus nowMonotonicMs, NICHT aus
    // sample.monotonicTimestampMs(), ermittelt werden (Abschnitt 9a,
    // Korrektur Runde 5). Mit dem falschen Referenzzeitpunkt (5000) waere das
    // Alter seit t=2000 nur 3000 ms - noch nicht Failed.
    (void)pipeline.ingest(okReading(std::nullopt, 5000U, 20.5), 100'000U);

    const auto snapshot = pipeline.snapshot(100'000U);
    TEST_ASSERT_TRUE(snapshot.quality != SensorQuality::Valid);
    TEST_ASSERT_EQUAL_UINT16(1U, snapshot.recoveryProgressCount);
}

void test_failed_recovers_to_valid_under_the_same_condition_as_stale() {
    SensorQualityPipeline pipeline(makeTestConfig());
    (void)pipeline.ingest(
        faultReading(std::nullopt, 1000U, TemperatureSampleStatus::CrcFault),
        1000U);
    (void)pipeline.ingest(
        faultReading(std::nullopt, 2000U, TemperatureSampleStatus::CrcFault),
        2000U);
    (void)pipeline.ingest(
        faultReading(std::nullopt, 3000U, TemperatureSampleStatus::CrcFault),
        3000U);
    (void)pipeline.ingest(
        faultReading(std::nullopt, 4000U, TemperatureSampleStatus::CrcFault),
        4000U);
    TEST_ASSERT_TRUE(pipeline.snapshot(4000U).quality == SensorQuality::Failed);

    (void)pipeline.ingest(okReading(std::nullopt, 5000U, 20.0), 5000U);
    (void)pipeline.ingest(okReading(std::nullopt, 7000U, 20.5), 7000U);

    TEST_ASSERT_TRUE(pipeline.snapshot(7000U).quality == SensorQuality::Valid);
}

void test_failed_latch_clears_using_now_not_sample_timestamp_on_delayed_completion() {
    // Regression (Nachreview PR #95): recoveryComplete() durfte nur EINMAL
    // implementiert und ausschliesslich mit nowMonotonicMs ausgewertet
    // werden. Eine zweite Kopie mit sample.monotonicTimestampMs() als
    // Referenzzeitpunkt haette das Failed-Merkbit bei einer verspaetet
    // zugestellten, die Folge abschliessenden Probe NICHT freigegeben,
    // obwohl deriveQuality() (mit nowMonotonicMs) bereits Valid meldet - ein
    // nachfolgender einzelner Fehler waere dann faelschlich wieder Failed
    // statt Stale gewesen.
    SensorQualityPipeline pipeline(makeTestConfig());
    (void)pipeline.ingest(
        faultReading(std::nullopt, 1000U, TemperatureSampleStatus::CrcFault),
        1000U);
    (void)pipeline.ingest(
        faultReading(std::nullopt, 2000U, TemperatureSampleStatus::CrcFault),
        2000U);
    (void)pipeline.ingest(
        faultReading(std::nullopt, 3000U, TemperatureSampleStatus::CrcFault),
        3000U);
    (void)pipeline.ingest(
        faultReading(std::nullopt, 4000U, TemperatureSampleStatus::CrcFault),
        4000U);
    TEST_ASSERT_TRUE(pipeline.snapshot(4000U).quality == SensorQuality::Failed);

    (void)pipeline.ingest(okReading(std::nullopt, 5000U, 20.0), 5000U);
    // Zeitstempel 6000, aber erst bei nowMonotonicMs = 8000 zugestellt: die
    // Folgedauer seit Streakbeginn (5000) betraegt am Probenzeitstempel
    // (6000-5000=1000 ms) noch NICHT die geforderten 2000 ms, wohl aber am
    // tatsaechlichen Zustellzeitpunkt (8000-5000=3000 ms >= 2000 ms).
    (void)pipeline.ingest(okReading(std::nullopt, 6000U, 20.1), 8000U);
    TEST_ASSERT_TRUE(pipeline.snapshot(8000U).quality == SensorQuality::Valid);

    (void)pipeline.ingest(
        faultReading(std::nullopt, 9000U, TemperatureSampleStatus::CrcFault),
        9000U);

    // War die Wiedererkennung tatsaechlich (und einheitlich) abgeschlossen,
    // faellt ein einzelner erneuter Fehler auf Stale zurueck - nicht auf
    // Failed (Abschnitt 8: "einzelne ungueltige Probe (Valid-Zustand) ->
    // Stale").
    TEST_ASSERT_TRUE(pipeline.snapshot(9000U).quality == SensorQuality::Stale);
}

void test_renewed_invalid_sample_during_recovery_resets_progress() {
    SensorQualityPipeline pipeline(makeTestConfig());
    (void)pipeline.ingest(
        faultReading(std::nullopt, 1000U, TemperatureSampleStatus::CrcFault),
        1000U);
    (void)pipeline.ingest(
        faultReading(std::nullopt, 2000U, TemperatureSampleStatus::CrcFault),
        2000U);
    (void)pipeline.ingest(
        faultReading(std::nullopt, 3000U, TemperatureSampleStatus::CrcFault),
        3000U);
    (void)pipeline.ingest(
        faultReading(std::nullopt, 4000U, TemperatureSampleStatus::CrcFault),
        4000U);
    TEST_ASSERT_TRUE(pipeline.snapshot(4000U).quality == SensorQuality::Failed);

    // Ein Sample der begonnenen Wiedererkennungsfolge, dann erneut ein
    // Fehler - Fortschritt und Startzeit muessen zurueckgesetzt werden.
    (void)pipeline.ingest(okReading(std::nullopt, 5000U, 20.0), 5000U);
    (void)pipeline.ingest(
        faultReading(std::nullopt, 6000U, TemperatureSampleStatus::CrcFault),
        6000U);

    const auto snapshot = pipeline.snapshot(6000U);
    TEST_ASSERT_EQUAL_UINT16(0U, snapshot.recoveryProgressCount);
    // Failed, NICHT Stale (Nachkorrektur PR #95, Abschnitt 8: "erneute
    // ungueltige Probe waehrend Wiedererkennung ... bleibt ... Failed"): das
    // minimale, ausschliesslich in ingest() geschriebene Merkbit
    // failedLatched_ haelt fest, dass Failed bereits erreicht wurde und die
    // volle Wiedererkennungsbedingung seither nicht erfuellt ist - eine
    // einzelne unvollstaendige Wiedererkennungsprobe darf das nicht
    // verschleiern.
    TEST_ASSERT_TRUE(snapshot.quality == SensorQuality::Failed);

    // Danach muss die VOLLE Folge erneut ab Null erfuellt werden - ein
    // einzelnes weiteres gueltiges Sample nach dem Rueckfall reicht nicht.
    (void)pipeline.ingest(okReading(std::nullopt, 7000U, 20.0), 7000U);
    TEST_ASSERT_TRUE(pipeline.snapshot(7000U).quality != SensorQuality::Valid);
}

void test_last_valid_value_remains_visible_during_stale_and_failed() {
    SensorQualityPipeline pipeline = warmedUpValidPipeline();

    (void)pipeline.ingest(
        faultReading(std::nullopt, 4000U, TemperatureSampleStatus::CrcFault),
        4000U);

    const auto snapshot = pipeline.snapshot(4000U);
    TEST_ASSERT_TRUE(snapshot.quality == SensorQuality::Stale);
    TEST_ASSERT_TRUE(snapshot.rawCelsius.has_value());
    TEST_ASSERT_EQUAL_DOUBLE(21.0, snapshot.rawCelsius.value());
}

void test_snapshot_far_in_the_future_without_further_ingest_detects_failed() {
    const SensorQualityPipeline pipeline = warmedUpValidPipeline();
    TEST_ASSERT_TRUE(pipeline.snapshot(2000U).quality == SensorQuality::Valid);

    const auto snapshot = pipeline.snapshot(2000U + 999'999U);

    TEST_ASSERT_TRUE(snapshot.quality == SensorQuality::Failed);
}

void test_identity_change_between_two_known_identities_is_identity_mismatch() {
    SensorQualityPipeline pipeline(makeTestConfig());
    const auto identityA = SensorIdentity::create(11U).identity;
    const auto identityB = SensorIdentity::create(22U).identity;
    (void)pipeline.ingest(okReading(identityA, 0U, 20.0), 0U);

    (void)pipeline.ingest(okReading(identityB, 1000U, 20.5), 1000U);

    const auto snapshot = pipeline.snapshot(1000U);
    TEST_ASSERT_TRUE(snapshot.lastFaultReason ==
                     SensorFaultReason::IdentityMismatch);
    TEST_ASSERT_TRUE(snapshot.quality == SensorQuality::Stale);
    TEST_ASSERT_EQUAL_UINT16(1U, snapshot.consecutiveInvalidCount);
}

void test_identity_transition_from_or_to_unknown_is_not_identity_mismatch() {
    SensorQualityPipeline pipeline(makeTestConfig());
    const auto identityA = SensorIdentity::create(11U).identity;
    (void)pipeline.ingest(okReading(std::nullopt, 0U, 20.0), 0U);

    (void)pipeline.ingest(okReading(identityA, 1000U, 20.5), 1000U);

    TEST_ASSERT_TRUE(pipeline.snapshot(1000U).lastFaultReason !=
                     SensorFaultReason::IdentityMismatch);
}

void test_two_independent_pipelines_do_not_influence_each_other() {
    SensorQualityPipeline air(makeTestConfig());
    SensorQualityPipeline product(makeTestConfig());

    (void)air.ingest(okReading(std::nullopt, 0U, 20.0), 0U);
    (void)product.ingest(
        okReading(std::nullopt, 0U,
                  device_platform::sensor_limits::kAbsoluteMaxCelsius + 1.0),
        0U);

    TEST_ASSERT_TRUE(air.snapshot(0U).lastFaultReason ==
                     SensorFaultReason::None);
    TEST_ASSERT_TRUE(product.snapshot(0U).lastFaultReason ==
                     SensorFaultReason::OutOfRange);
    // "Keine rollenuebergreifende Verdachtsuebertragung" (Abschnitt 17) ist
    // hier zusaetzlich durch fehlende API belegt: SensorQualityPipeline
    // besitzt keine Methode, die eine Instanz mit einer anderen verknuepfen
    // koennte - zwei Instanzen sind zwangslaeufig unabhaengig.
}

// ---------------------------------------------------------------------
// Robustheit
// ---------------------------------------------------------------------

void test_consecutive_invalid_count_saturates_instead_of_wrapping() {
    SensorQualityPipeline pipeline(makeTestConfig());
    uint64_t timestampMs = 1U;
    for (uint32_t i = 0; i < 65'537U; ++i) {
        (void)pipeline.ingest(faultReading(std::nullopt, timestampMs,
                                           TemperatureSampleStatus::CrcFault),
                              timestampMs);
        ++timestampMs;
    }

    TEST_ASSERT_EQUAL_UINT16(
        std::numeric_limits<uint16_t>::max(),
        pipeline.snapshot(timestampMs).consecutiveInvalidCount);
}

void test_recovery_progress_count_saturates_instead_of_wrapping() {
    SensorQualityPipeline pipeline(makeTestConfig());
    uint64_t timestampMs = 0U;
    for (uint32_t i = 0; i < 65'537U; ++i) {
        (void)pipeline.ingest(okReading(std::nullopt, timestampMs, 20.0),
                              timestampMs);
        timestampMs += 100U;
    }

    TEST_ASSERT_EQUAL_UINT16(
        std::numeric_limits<uint16_t>::max(),
        pipeline.snapshot(timestampMs).recoveryProgressCount);
}

void test_timestamp_near_uint64_max_ages_correctly_without_overflow() {
    SensorQualityPipeline pipeline(makeTestConfig());
    const uint64_t nearMax = std::numeric_limits<uint64_t>::max() - 100U;
    (void)pipeline.ingest(okReading(std::nullopt, nearMax, 20.0), nearMax);

    const auto snapshot = pipeline.snapshot(nearMax + 50U);

    TEST_ASSERT_TRUE(snapshot.lastAcceptedSampleAgeMs.has_value());
    TEST_ASSERT_EQUAL_UINT64(50U, snapshot.lastAcceptedSampleAgeMs.value());
}

void test_identical_input_sequence_yields_identical_snapshot() {
    SensorQualityPipeline first(makeTestConfig());
    SensorQualityPipeline second(makeTestConfig());

    const auto identity = SensorIdentity::create(3U).identity;
    for (auto* pipeline : {&first, &second}) {
        (void)pipeline->ingest(okReading(identity, 0U, 20.0), 0U);
        (void)pipeline->ingest(okReading(identity, 2000U, 21.0), 2000U);
        (void)pipeline->ingest(
            faultReading(identity, 4000U, TemperatureSampleStatus::CrcFault),
            4000U);
    }

    const auto snapshotA = first.snapshot(4000U);
    const auto snapshotB = second.snapshot(4000U);
    TEST_ASSERT_TRUE(snapshotA.quality == snapshotB.quality);
    TEST_ASSERT_EQUAL_DOUBLE(snapshotA.rawCelsius.value(),
                             snapshotB.rawCelsius.value());
    TEST_ASSERT_EQUAL_UINT64(snapshotA.lastAcceptedSampleAgeMs.value(),
                             snapshotB.lastAcceptedSampleAgeMs.value());
    TEST_ASSERT_TRUE(snapshotA.lastFaultReason == snapshotB.lastFaultReason);
    TEST_ASSERT_EQUAL_UINT16(snapshotA.consecutiveInvalidCount,
                             snapshotB.consecutiveInvalidCount);
    TEST_ASSERT_EQUAL_UINT16(snapshotA.recoveryProgressCount,
                             snapshotB.recoveryProgressCount);
}

int main() {
    UNITY_BEGIN();

    RUN_TEST(test_start_without_any_sample_is_stale_with_no_derived_values);
    RUN_TEST(test_first_valid_sample_stays_stale_but_updates_raw_evidence);
    RUN_TEST(test_min_consecutive_valid_samples_reach_valid);
    RUN_TEST(test_valid_remains_stable_over_continued_cycle);

    RUN_TEST(test_single_crc_fault_drops_to_stale_not_immediately_failed);
    RUN_TEST(test_consecutive_crc_faults_beyond_ceiling_reach_failed);
    RUN_TEST(test_bus_fault_is_recorded_with_its_own_reason);
    RUN_TEST(test_missing_sample_is_recorded_with_its_own_reason);
    RUN_TEST(test_known_invalid_measurement_is_recorded_with_its_own_reason);
    RUN_TEST(test_nonfinite_celsius_is_nonfinite_not_out_of_range);

    RUN_TEST(test_retrograde_timestamp_is_rejected_and_state_unchanged);
    RUN_TEST(test_future_timestamp_is_rejected);
    RUN_TEST(test_exact_duplicate_is_ignored);
    RUN_TEST(test_same_timestamp_different_value_is_conflict);
    RUN_TEST(
        test_same_timestamp_same_value_different_identity_is_conflict_not_duplicate);
    RUN_TEST(test_same_timestamp_both_nan_is_duplicate);
    RUN_TEST(test_same_timestamp_nan_versus_finite_is_conflict);
    RUN_TEST(test_very_first_sample_is_accepted);
    RUN_TEST(test_very_first_sample_from_the_future_is_rejected);
    RUN_TEST(test_large_gap_transitions_to_failed_via_snapshot_age);
    RUN_TEST(test_resumption_after_gap_is_not_evaluated_as_a_jump);
    RUN_TEST(test_first_plausible_sample_after_single_fault_has_no_rate);
    RUN_TEST(test_first_plausible_sample_after_identity_change_has_no_rate);
    RUN_TEST(test_delayed_ordered_sample_detects_prior_failed_pre_state);

    RUN_TEST(test_value_below_absolute_minimum_is_out_of_range);
    RUN_TEST(test_value_above_absolute_maximum_is_out_of_range);
    RUN_TEST(
        test_value_within_absolute_but_outside_configured_range_is_rejected);
    RUN_TEST(
        test_same_absolute_delta_is_judged_differently_over_short_versus_long_interval);
    RUN_TEST(test_rate_exceeding_sample_does_not_stop_permanently);

    RUN_TEST(test_single_invalid_sample_drops_valid_to_stale);
    RUN_TEST(test_failed_stays_failed_during_incomplete_recovery);
    RUN_TEST(test_first_sample_after_aged_failed_is_not_immediately_valid);
    RUN_TEST(test_failed_recovers_to_valid_under_the_same_condition_as_stale);
    RUN_TEST(
        test_failed_latch_clears_using_now_not_sample_timestamp_on_delayed_completion);
    RUN_TEST(test_renewed_invalid_sample_during_recovery_resets_progress);
    RUN_TEST(test_last_valid_value_remains_visible_during_stale_and_failed);
    RUN_TEST(
        test_snapshot_far_in_the_future_without_further_ingest_detects_failed);
    RUN_TEST(
        test_identity_change_between_two_known_identities_is_identity_mismatch);
    RUN_TEST(
        test_identity_transition_from_or_to_unknown_is_not_identity_mismatch);
    RUN_TEST(test_two_independent_pipelines_do_not_influence_each_other);

    RUN_TEST(test_consecutive_invalid_count_saturates_instead_of_wrapping);
    RUN_TEST(test_recovery_progress_count_saturates_instead_of_wrapping);
    RUN_TEST(test_timestamp_near_uint64_max_ages_correctly_without_overflow);
    RUN_TEST(test_identical_input_sequence_yields_identical_snapshot);

    return UNITY_END();
}
