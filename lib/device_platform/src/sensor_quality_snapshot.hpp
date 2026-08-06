#pragma once

#include <cstdint>
#include <optional>

#include "sensor_identity.hpp"
#include "sensor_quality.hpp"

// Ausgabe-/Diagnosevertrag einer SensorQualityPipeline-Instanz. Siehe
// docs/tasks/issue-20-sensor-quality-filtering-plan.md, Abschnitt 12, fuer
// die vollstaendige Herleitung der Feldsemantik.
namespace device_platform {

// Fachliche Sensorfehlerursache (Abschnitt 10/12). Bewusst getrennt von
// SampleDisposition (sensor_quality_pipeline.hpp): SampleDisposition
// beschreibt reine Zeitstempel-/Zufuhranomalien der Probenzustellung, keine
// Sensorfehlerevidenz.
enum class SensorFaultReason : uint8_t {
    None,
    BusFault,
    CrcFault,
    MissingSample,
    KnownInvalidMeasurement,
    NonFinite,
    OutOfRange,
    RateOfChangeExceeded,
    IdentityMismatch,
};

struct SensorQualitySnapshot {
    // Zuletzt bekannte Identitaet; nullopt, falls nie eine akzeptierte Probe
    // mit bekannter Identitaet vorlag.
    std::optional<SensorIdentity> identity;
    SensorQuality quality{SensorQuality::Stale};
    // Wert der letzten AKZEPTIERTEN Probe mit Messwert (status == Ok),
    // unabhaengig von Plausibilitaet; nullopt vor der allerersten solchen
    // Probe. Ein extremer Rohwert bleibt dadurch immer sichtbar, auch wenn
    // er als unplausibel verworfen wurde.
    std::optional<double> rawCelsius;
    // Medianfilter-Ausgang + Offset; bereits ab dem ersten plausiblen
    // Medianbeitrag vorhanden (nicht erst bei vollem Fenster), NICHT
    // rawCelsius + Offset. In Slice 1 stets nullopt (kein Medianfilter).
    std::optional<double> correctedCelsius;
    // Tiefpass-Ausgang; ab demselben ersten Beitrag wie correctedCelsius
    // vorhanden. In Slice 1 stets nullopt (kein Tiefpass).
    std::optional<double> filteredCelsius;
    // nullopt bedeutet "keine zur aktuellen Identitaet passende Kalibrierung
    // angewendet"; hat einen Wert genau dann, wenn eine tatsaechlich
    // passende SensorCalibration angewendet wurde (auch wenn dieser Wert
    // zufaellig 0.0 ist). In Slice 1 stets nullopt (keine Kalibrierung).
    std::optional<double> appliedOffset;
    // Alter der letzten AKZEPTIERTEN Probe; abgelehnte und doppelte Proben
    // (DuplicateIgnored/RejectedTimestampConflict/RejectedRetrograde/
    // RejectedFuture) aktualisieren dieses Feld NICHT.
    std::optional<uint64_t> lastAcceptedSampleAgeMs;
    // Alter der letzten ZUSAETZLICH plausiblen (10.1/10.2-gueltigen) Probe.
    std::optional<uint64_t> lastValidSampleAgeMs;
    SensorFaultReason lastFaultReason{SensorFaultReason::None};
    uint16_t consecutiveInvalidCount{0};
    // Aufeinanderfolgende gueltige Proben waehrend einer Wiedererkennung.
    uint16_t recoveryProgressCount{0};
    // nullopt ohne unmittelbaren gueltigen Vorwert.
    std::optional<double> changeRateCelsiusPerSecond;
};

}  // namespace device_platform
