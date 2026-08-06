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

SensorQualityPipeline::RawFailureConditions
SensorQualityPipeline::computeRawFailureConditions(
    uint64_t referenceTimeMs) const {
    const bool ageFailed =
        lastValidTimestampMs_.has_value() &&
        saturatingAgeMs(referenceTimeMs, *lastValidTimestampMs_) >
            config_.maxStaleAgeMs();
    const bool countFailed =
        consecutiveInvalidCount_ > config_.maxConsecutiveInvalid();
    return RawFailureConditions{ageFailed, countFailed};
}

bool SensorQualityPipeline::isRecoveryComplete() const {
    if (recoveryProgressCount_ < config_.minConsecutiveValidSamples() ||
        !recoveryStreakStartTimestampMs_.has_value() ||
        !recoveryStreakLastTimestampMs_.has_value()) {
        return false;
    }
    if (*recoveryStreakLastTimestampMs_ < *recoveryStreakStartTimestampMs_) {
        return false;
    }
    return *recoveryStreakLastTimestampMs_ - *recoveryStreakStartTimestampMs_ >=
           config_.minRecoveryStabilityDurationMs();
}

SensorQualityPipeline::DerivedQuality SensorQualityPipeline::deriveQuality(
    uint64_t referenceTimeMs) const {
    const RawFailureConditions raw =
        computeRawFailureConditions(referenceTimeMs);
    const bool recoveryComplete = isRecoveryComplete();

    SensorQuality quality;
    // Failed-durch-Alter und Failed-durch-Zaehler haben unbedingten Vorrang:
    // ein laengst verstummter oder dauerhaft fehlerhafter Sensor muss auch
    // dann als Failed erkannt werden, wenn zufaellig gleichzeitig eine
    // abgeschlossene Wiedererkennungsfolge vorliegen wuerde (Abschnitt 9a).
    if (raw.ageFailed || raw.countFailed) {
        quality = SensorQuality::Failed;
    } else if (recoveryComplete) {
        quality = SensorQuality::Valid;
    } else if (failedLatched_) {
        // Nachkorrektur PR #95 (Abschnitt 8 "erneute ungueltige Probe
        // waehrend Wiedererkennung ... bleibt ... Failed"): ohne dieses
        // minimale, rein gelesene Merkbit wuerde eine einzelne gueltige
        // Probe waehrend einer noch UNVOLLSTAENDIGEN Wiedererkennung den
        // Zaehler bereits so weit zuruecksetzen, dass ein anschliessender
        // einzelner erneuter Fehler faelschlich nur noch Stale ergibt.
        // failedLatched_ wird ausschliesslich in ingest() geschrieben (bei
        // neu erreichtem Failed bzw. bei vollstaendiger Wiedererkennung),
        // hier nur gelesen - keine zweite Qualitaetswahrheit.
        quality = SensorQuality::Failed;
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
    recoveryStreakLastTimestampMs_ = monotonicTimestampMs;
    recoveryProgressCount_ = saturatingIncrement(recoveryProgressCount_);
    // Aenderungsratenreferenz (10.2): diese Probe IST jetzt der neue
    // "unmittelbar vorherige gueltige Wert".
    rateReferenceCelsius_ = celsius;
    rateReferenceTimestampMs_ = monotonicTimestampMs;
    // Das Failed-Merkbit wird bewusst NICHT hier freigegeben. ingest() wertet
    // die einzige Wiedererkennungsbedingung nach diesem Aufruf erneut aus;
    // diese haengt nur von den Erfassungszeitstempeln der aktuellen Folge ab,
    // nicht vom Zustellzeitpunkt.
}

void SensorQualityPipeline::recordInvalidSample(SensorFaultReason reason) {
    consecutiveInvalidCount_ = saturatingIncrement(consecutiveInvalidCount_);
    if (consecutiveInvalidCount_ > config_.maxConsecutiveInvalid()) {
        failedLatched_ = true;
    }
    recoveryProgressCount_ = 0U;
    recoveryStreakStartTimestampMs_ = std::nullopt;
    recoveryStreakLastTimestampMs_ = std::nullopt;
    // Jede akzeptierte, aber ungueltige Probe bricht die "unmittelbar
    // vorherige gueltige Wert"-Eigenschaft fuer die naechste Aenderungsraten-
    // pruefung (10.2, Nachkorrektur PR #95).
    rateReferenceCelsius_ = std::nullopt;
    rateReferenceTimestampMs_ = std::nullopt;
    lastFaultReason_ = reason;
}

SampleDisposition SensorQualityPipeline::ingest(
    const TemperatureReading& sample, uint64_t nowMonotonicMs) {
    const SampleDisposition disposition =
        determineDisposition(sample, nowMonotonicMs);
    if (disposition != SampleDisposition::Accepted) {
        return disposition;
    }

    // VERBINDLICHE REIHENFOLGE (Abschnitt 9a, Korrektur Runde 5, nachgeschaerft
    // im Nachreview zu PR #95): der Vorzustand wird aus nowMonotonicMs UND den
    // noch UNVERAENDERTEN gespeicherten Groessen ermittelt, BEVOR irgendein
    // Zaehler/Zeitstempel dieser Probe aktualisiert wird - sonst wuerde ein
    // laengst altersbedingt erreichtes Failed durch die erste neue Probe
    // verschleiert. Bewusst NUR die UNMITTELBAREN (Alters-/Zaehler-) Failed-
    // Bedingungen, NICHT failedLatched_ selbst: waehrend einer noch
    // laufenden, unvollstaendigen Wiedererkennung ist failedLatched_ bereits
    // true, OHNE dass die aktuelle Probe daran etwas Neues aendert - ein
    // Reset an dieser Stelle wuerde eine laufende Folge durch ihre eigene
    // naechste Probe immer wieder auf 0 zuruecksetzen und Wiedererkennung
    // strukturell unmoeglich machen.
    const RawFailureConditions preState =
        computeRawFailureConditions(nowMonotonicMs);
    if (preState.ageFailed || preState.countFailed) {
        // Eine liegen gebliebene Teil-Wiedererkennungsfolge aus der Zeit vor
        // diesem (ggf. rein altersbedingt, ganz ohne weiteren ingest()-Aufruf
        // erreichten) Failed darf nicht unbemerkt fortbestehen - diese Probe
        // beginnt noetigenfalls eine neue Folge bei 1 (Abschnitt 8: "Failed ->
        // Valid: identische Bedingung wie Stale -> Valid").
        recoveryProgressCount_ = 0U;
        recoveryStreakStartTimestampMs_ = std::nullopt;
        failedLatched_ = true;
        // Kein "unmittelbar vorheriger gueltiger Wert" mehr im Sinne von
        // Abschnitt 10.2.
        rateReferenceCelsius_ = std::nullopt;
        rateReferenceTimestampMs_ = std::nullopt;
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
    // Firmwarefeste Aussengrenze (10.2) ...
    if (celsius < sensor_limits::kAbsoluteMinCelsius ||
        celsius > sensor_limits::kAbsoluteMaxCelsius) {
        recordInvalidSample(SensorFaultReason::OutOfRange);
        return SampleDisposition::Accepted;
    }
    // ... UND zusaetzlich das konfigurierte, ggf. engere Plausibilitaetsband
    // dieser Sensorrolle (SensorQualityConfig::minPlausibleCelsius()/
    // maxPlausibleCelsius(), Abschnitt 10.0; durch create() bereits
    // innerhalb der Aussengrenze validiert - beide Pruefungen bleiben
    // trotzdem getrennt bestehen, Nachkorrektur PR #95).
    if (celsius < config_.minPlausibleCelsius() ||
        celsius > config_.maxPlausibleCelsius()) {
        recordInvalidSample(SensorFaultReason::OutOfRange);
        return SampleDisposition::Accepted;
    }

    if (rateReferenceCelsius_.has_value() &&
        rateReferenceTimestampMs_.has_value()) {
        // RejectedTimestampConflict-Proben werden nie akzeptiert (9b), daher
        // ist dieser Zeitunterschied durch Konstruktion immer echt > 0.
        const double dtSeconds =
            static_cast<double>(sample.monotonicTimestampMs() -
                                *rateReferenceTimestampMs_) /
            1000.0;
        const double rate =
            std::fabs(celsius - *rateReferenceCelsius_) / dtSeconds;
        changeRateCelsiusPerSecond_ = rate;
        if (rate > config_.maxRateOfChangeCelsiusPerSecond()) {
            recordInvalidSample(SensorFaultReason::RateOfChangeExceeded);
            return SampleDisposition::Accepted;
        }
    } else {
        // Kein unmittelbar vorheriger gueltiger Wert (erste Probe, Luecke
        // durch eine ungueltige Probe, bereits erreichtes Failed oder
        // ROM-Wechsel) - keine erfundene Bewertung.
        changeRateCelsiusPerSecond_ = std::nullopt;
    }

    recordValidSample(celsius, sample.monotonicTimestampMs());
    // Failed-Merkbit-Freigabe (Abschnitt 8) ueber dieselbe einzige Formel wie
    // in deriveQuality(), ausschliesslich anhand der Erfassungszeitstempel
    // der aktuellen gueltigen Folge.
    if (isRecoveryComplete()) {
        failedLatched_ = false;
    }
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
