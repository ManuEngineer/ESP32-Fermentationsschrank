#pragma once

#include <cstdint>
#include <optional>

#include "sensor_quality.hpp"
#include "sensor_quality_config.hpp"
#include "sensor_quality_snapshot.hpp"
#include "temperature_source.hpp"

// Orchestrator einer einzelnen Sensorqualitaets-/Plausibilitaetspipeline.
// Siehe docs/tasks/issue-20-sensor-quality-filtering-plan.md, Abschnitt 8-12,
// fuer die vollstaendige fachliche Herleitung. Slice 1 (dieser Stand) deckt
// Zeitstempel-/Dispositionspruefung (9b), Transport-/Wertebereichs-/
// Aenderungsratenpruefung (10.1/10.2) und die Zustandsmaschine (8/9a) ab;
// Median, Kalibrier-Offset und Tiefpass folgen in Slice 2
// (correctedCelsius/filteredCelsius/appliedOffset bleiben bis dahin stets
// std::nullopt).
namespace device_platform {

// Disposition einer einzelnen eingehenden Probe (Abschnitt 9b) - reine
// Zeitstempel-/Zufuhranomalie der Zustellung, KEINE Sensorfehlerevidenz.
// Bewusst getrennt von SensorFaultReason (sensor_quality_snapshot.hpp).
enum class SampleDisposition : uint8_t {
    Accepted,
    DuplicateIgnored,
    RejectedTimestampConflict,
    RejectedRetrograde,
    RejectedFuture,
};

class SensorQualityPipeline {
   public:
    explicit SensorQualityPipeline(SensorQualityConfig config);

    // Verarbeitet eine einzelne Probe. `nowMonotonicMs` ist derselbe
    // monotone Uhrwert, den der Aufrufer zum Zeitpunkt dieses Aufrufs
    // gelesen hat; er dient der Zukunftspruefung und als Referenzzeitpunkt
    // fuer die interne Vorzustandsermittlung, wird selbst nicht
    // gespeichert (Abschnitt 9a).
    [[nodiscard]] SampleDisposition ingest(const TemperatureReading& sample,
                                            uint64_t nowMonotonicMs);

    // Liefert Qualitaet und Altersfelder relativ zum explizit uebergebenen
    // `nowMonotonicMs`, nicht relativ zu einer intern gespeicherten Uhr
    // (die Pipeline haelt keine ITimeSource-Referenz, Abschnitt 9a/15).
    [[nodiscard]] SensorQualitySnapshot snapshot(uint64_t nowMonotonicMs) const;

   private:
    // Ergebnis der einzigen internen Ableitungsfunktion deriveQuality()
    // (Abschnitt 9a) - wird sowohl von snapshot() als auch (ab Slice 2) von
    // ingest() fuer die Filterreset-Entscheidung konsumiert. Rein intern,
    // daher als private verschachtelter Typ.
    struct DerivedQuality {
        SensorQuality quality;
        std::optional<uint64_t> lastAcceptedSampleAgeMs;
        std::optional<uint64_t> lastValidSampleAgeMs;
    };

    [[nodiscard]] SampleDisposition determineDisposition(
        const TemperatureReading& sample, uint64_t nowMonotonicMs) const;
    [[nodiscard]] DerivedQuality deriveQuality(uint64_t referenceTimeMs) const;
    void recordValidSample(double celsius, uint64_t monotonicTimestampMs);
    void recordInvalidSample(SensorFaultReason reason);

    SensorQualityConfig config_;

    // Letzte AKZEPTIERTE Probe (jeder Disposition-Ausgang ausser Accepted
    // aendert diesen Zustand nicht) - Grundlage fuer Duplikat-/Konflikt-/
    // Retrograde-Vergleich (9b) und ROM-Wechsel-Erkennung (paarweiser
    // Vergleich zweier aufeinanderfolgender akzeptierter Proben).
    std::optional<TemperatureReading> lastAccepted_;
    // Zuletzt BEKANNTE (nicht-nullopt) Identitaet - bewusst getrennt von
    // lastAccepted_->identity(): eine spaetere Probe mit unbekannter
    // Identitaet darf diesen Snapshot-Wert nicht auf nullopt zuruecksetzen
    // (Abschnitt 12: "zuletzt bekannte Identitaet").
    std::optional<SensorIdentity> lastKnownIdentity_;

    // Wert der letzten akzeptierten Probe MIT Messwert (status == Ok),
    // unabhaengig von Plausibilitaet (Abschnitt 12).
    std::optional<double> rawCelsius_;
    // Alter der letzten akzeptierten Probe wird hieraus abgeleitet.
    std::optional<uint64_t> lastAcceptedTimestampMs_;

    // Letzter tatsaechlich PLAUSIBLE (10.1/10.2-gueltige) Rohwert/Zeitstempel
    // - Referenz fuer die Aenderungsratenpruefung (10.2) und die
    // Stale-Altersschwelle (Abschnitt 8). Bewusst getrennt von rawCelsius_/
    // lastAcceptedTimestampMs_ (Korrektur Runde 3: ein extremer, unplausibler
    // Rohwert bleibt in rawCelsius_ sichtbar, darf aber NICHT zur neuen
    // Aenderungsratenreferenz werden).
    std::optional<double> lastValidRawCelsius_;
    std::optional<uint64_t> lastValidTimestampMs_;

    // Aufeinanderfolgende ungueltige bzw. gueltige Proben seit dem jeweils
    // letzten Gegenereignis - exakte Komplemente: eine gueltige Probe setzt
    // consecutiveInvalidCount_ auf 0 und erhoeht recoveryProgressCount_
    // (und umgekehrt bei einer ungueltigen Probe), siehe Abschnitt 8/9a.
    uint16_t consecutiveInvalidCount_{0};
    uint16_t recoveryProgressCount_{0};
    // Zeitstempel der ERSTEN Probe der aktuellen ununterbrochenen gueltigen
    // Folge - Grundlage fuer die Stabilitaetszeitpruefung (Abschnitt 8).
    std::optional<uint64_t> recoveryStreakStartTimestampMs_;

    SensorFaultReason lastFaultReason_{SensorFaultReason::None};
    std::optional<double> changeRateCelsiusPerSecond_;
};

}  // namespace device_platform
