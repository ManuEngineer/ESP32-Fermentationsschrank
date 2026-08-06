#include "sensor_quality_pipeline.hpp"

#include <cmath>
#include <limits>

#include "sensor_limits.hpp"

namespace device_platform {

namespace {

uint16_t saturatingIncrement(uint16_t value) {
    if (value == std::numeric_limits<uint16_t>::max()) {
        return value;
    }
    return static_cast<uint16_t>(value + 1U);
}

// Saettigt auf 0 statt vorzeichenlos zu unterlaufen, falls referenceTimeMs
// (aus welchem Grund auch immer) vor pastTimestampMs liegt. Deckt außerdem
// den Robustheitsfall naher UINT64_MAX-Zeitstempel korrekt ab.
uint64_t saturatingAgeMs(uint64_t referenceTimeMs, uint64_t pastTimestampMs) {
    if (referenceTimeMs < pastTimestampMs) {
        return 0U;
    }
    return referenceTimeMs - pastTimestampMs;
}

bool celsiusValuesEqual(std::optional<double> a, std::optional<double> b) {
    if (a.has_value() != b.has_value()) {
        return false;
    }
    if (!a.has_value()) {
        return true;
    }
    // NaN == NaN ist nach IEEE 754 false; zwei NaN-Proben zum selben
    // Zeitstempel sind semantisch "derselbe defekte Messwert nochmal
    // gesendet", kein neuer Widerspruch (Abschnitt 9b).
    if (std::isnan(*a) && std::isnan(*b)) {
        return true;
    }
    return *a == *b;
}

bool sampleValuesEqual(const TemperatureReading& a,
                       const TemperatureReading& b) {
    return a.identity() == b.identity() && a.status() == b.status() &&
           celsiusValuesEqual(a.celsius(), b.celsius());
}

SensorFaultReason faultReasonForStatus(TemperatureSampleStatus status) {
    switch (status) {
        case TemperatureSampleStatus::BusFault:
            return SensorFaultReason::BusFault;
        case TemperatureSampleStatus::CrcFault:
            return SensorFaultReason::CrcFault;
        case TemperatureSampleStatus::MissingSample:
            return SensorFaultReason::MissingSample;
        case TemperatureSampleStatus::KnownInvalidMeasurement:
            return SensorFaultReason::KnownInvalidMeasurement;
        case TemperatureSampleStatus::Ok:
            break;
    }
    // Nur fuer status != Ok aufgerufen (Aufrufer garantiert dies); Ok hat
    // keine Fehlerursache.
    return SensorFaultReason::None;
}

}  // namespace

SensorQualityPipeline::SensorQualityPipeline(SensorQualityConfig config)
    : config_(config) {}

SampleDisposition SensorQualityPipeline::determineDisposition(
    const TemperatureReading& sample, uint64_t nowMonotonicMs) const {
    if (!lastAccepted_.has_value()) {
        // Erste Probe ueberhaupt: Duplikat-/Konflikt-/Retrograde-Regeln
        // setzen einen Vergleichspartner voraus und sind daher nicht
        // anwendbar (Abschnitt 9b).
        return sample.monotonicTimestampMs() > nowMonotonicMs
                   ? SampleDisposition::RejectedFuture
                   : SampleDisposition::Accepted;
    }
    const uint64_t lastTimestampMs = lastAccepted_->monotonicTimestampMs();
    if (sample.monotonicTimestampMs() == lastTimestampMs) {
        return sampleValuesEqual(sample, *lastAccepted_)
                   ? SampleDisposition::DuplicateIgnored
                   : SampleDisposition::RejectedTimestampConflict;
    }
    if (sample.monotonicTimestampMs() < lastTimestampMs) {
        return SampleDisposition::RejectedRetrograde;
    }
    if (sample.monotonicTimestampMs() > nowMonotonicMs) {
        return SampleDisposition::RejectedFuture;
    }
    return SampleDisposition::Accepted;
}

SensorQualityPipeline::DerivedQuality SensorQualityPipeline::deriveQuality(
    uint64_t referenceTimeMs) const {
    SensorQuality quality;
    // Failed-durch-Alter hat unbedingten Vorrang: ein laengst verstummter
    // Sensor muss auch dann als Failed erkannt werden, wenn er vor seinem
    // Verstummen bereits eine abgeschlossene Wiedererkennungsfolge hatte
    // (Abschnitt 9a - sonst wuerde die Valid-Ableitung unten faelschlich
    // unbegrenzt fortbestehen, nur weil referenceTimeMs weiterlaeuft).
    if (lastValidTimestampMs_.has_value() &&
        saturatingAgeMs(referenceTimeMs, *lastValidTimestampMs_) >
            config_.maxStaleAgeMs()) {
        quality = SensorQuality::Failed;
    } else if (consecutiveInvalidCount_ > config_.maxConsecutiveInvalid()) {
        quality = SensorQuality::Failed;
    } else if (recoveryProgressCount_ >= config_.minConsecutiveValidSamples() &&
               recoveryStreakStartTimestampMs_.has_value() &&
               saturatingAgeMs(referenceTimeMs,
                                *recoveryStreakStartTimestampMs_) >=
                   config_.minRecoveryStabilityDurationMs()) {
        quality = SensorQuality::Valid;
    } else {
        quality = SensorQuality::Stale;
    }

    std::optional<uint64_t> lastAcceptedAgeMs;
    if (lastAcceptedTimestampMs_.has_value()) {
        lastAcceptedAgeMs =
            saturatingAgeMs(referenceTimeMs, *lastAcceptedTimestampMs_);
    }
    std::optional<uint64_t> lastValidAgeMs;
    if (lastValidTimestampMs_.has_value()) {
        lastValidAgeMs =
            saturatingAgeMs(referenceTimeMs, *lastValidTimestampMs_);
    }
    return DerivedQuality{quality, lastAcceptedAgeMs, lastValidAgeMs};
}

void SensorQualityPipeline::recordValidSample(double celsius,
                                              uint64_t monotonicTimestampMs) {
    lastValidRawCelsius_ = celsius;
    lastValidTimestampMs_ = monotonicTimestampMs;
    consecutiveInvalidCount_ = 0U;
    lastFaultReason_ = SensorFaultReason::None;
    if (recoveryProgressCount_ == 0U) {
        recoveryStreakStartTimestampMs_ = monotonicTimestampMs;
    }
    recoveryProgressCount_ = saturatingIncrement(recoveryProgressCount_);
}

void SensorQualityPipeline::recordInvalidSample(SensorFaultReason reason) {
    consecutiveInvalidCount_ = saturatingIncrement(consecutiveInvalidCount_);
    recoveryProgressCount_ = 0U;
    recoveryStreakStartTimestampMs_ = std::nullopt;
    lastFaultReason_ = reason;
}

SampleDisposition SensorQualityPipeline::ingest(
    const TemperatureReading& sample, uint64_t nowMonotonicMs) {
    const SampleDisposition disposition =
        determineDisposition(sample, nowMonotonicMs);
    if (disposition != SampleDisposition::Accepted) {
        return disposition;
    }

    // Disposition-Ebene: unabhaengig von der spaeteren Plausibilitaet
    // (Abschnitt 12).
    lastAcceptedTimestampMs_ = sample.monotonicTimestampMs();
    if (sample.status() == TemperatureSampleStatus::Ok) {
        rawCelsius_ = sample.celsius();
    }
    if (sample.identity().has_value()) {
        lastKnownIdentity_ = sample.identity();
    }

    // ROM-Wechsel-Erkennung (9b): identity() dieser Probe gegen die der
    // VORHERIGEN akzeptierten Probe, BEVOR lastAccepted_ ueberschrieben
    // wird. Nur ausgewertet, wenn beide Identitaeten bekannt sind (fehlende
    // Evidenz sonst).
    const bool identityMismatch =
        lastAccepted_.has_value() && lastAccepted_->identity().has_value() &&
        sample.identity().has_value() &&
        (*lastAccepted_->identity() != *sample.identity());

    lastAccepted_ = sample;

    if (identityMismatch) {
        recordInvalidSample(SensorFaultReason::IdentityMismatch);
        return SampleDisposition::Accepted;
    }

    // 10.1 Transport-/Messstatus.
    if (sample.status() != TemperatureSampleStatus::Ok) {
        recordInvalidSample(faultReasonForStatus(sample.status()));
        return SampleDisposition::Accepted;
    }

    // 10.2 Physikalischer Wertebereich und Aenderungsrate. status() == Ok
    // garantiert durch TemperatureReading-Konstruktion celsius().has_value().
    const double celsius = *sample.celsius();
    if (!std::isfinite(celsius)) {
        recordInvalidSample(SensorFaultReason::NonFinite);
        return SampleDisposition::Accepted;
    }
    if (celsius < sensor_limits::kAbsoluteMinCelsius ||
        celsius > sensor_limits::kAbsoluteMaxCelsius) {
        recordInvalidSample(SensorFaultReason::OutOfRange);
        return SampleDisposition::Accepted;
    }

    if (lastValidRawCelsius_.has_value() && lastValidTimestampMs_.has_value()) {
        // RejectedTimestampConflict-Proben werden nie akzeptiert (9b), daher
        // ist dieser Zeitunterschied durch Konstruktion immer echt > 0.
        const double dtSeconds =
            static_cast<double>(sample.monotonicTimestampMs() -
                                 *lastValidTimestampMs_) /
            1000.0;
        const double rate =
            std::fabs(celsius - *lastValidRawCelsius_) / dtSeconds;
        changeRateCelsiusPerSecond_ = rate;
        if (rate > config_.maxRateOfChangeCelsiusPerSecond()) {
            recordInvalidSample(SensorFaultReason::RateOfChangeExceeded);
            return SampleDisposition::Accepted;
        }
    } else {
        // Kein unmittelbar vorheriger gueltiger Wert (z. B. erste Probe oder
        // Wiederaufnahme nach einer Luecke) - keine erfundene Bewertung.
        changeRateCelsiusPerSecond_ = std::nullopt;
    }

    recordValidSample(celsius, sample.monotonicTimestampMs());
    return SampleDisposition::Accepted;
}

SensorQualitySnapshot SensorQualityPipeline::snapshot(
    uint64_t nowMonotonicMs) const {
    const DerivedQuality derived = deriveQuality(nowMonotonicMs);

    SensorQualitySnapshot result;
    result.identity = lastKnownIdentity_;
    result.quality = derived.quality;
    result.rawCelsius = rawCelsius_;
    // Slice 1: kein Medianfilter/Tiefpass/Kalibrierung -> stets nullopt
    // (siehe sensor_quality_snapshot.hpp).
    result.correctedCelsius = std::nullopt;
    result.filteredCelsius = std::nullopt;
    result.appliedOffset = std::nullopt;
    result.lastAcceptedSampleAgeMs = derived.lastAcceptedSampleAgeMs;
    result.lastValidSampleAgeMs = derived.lastValidSampleAgeMs;
    result.lastFaultReason = lastFaultReason_;
    result.consecutiveInvalidCount = consecutiveInvalidCount_;
    result.recoveryProgressCount = recoveryProgressCount_;
    result.changeRateCelsiusPerSecond = changeRateCelsiusPerSecond_;
    return result;
}

}  // namespace device_platform
