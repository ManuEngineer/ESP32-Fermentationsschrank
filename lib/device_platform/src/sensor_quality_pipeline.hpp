#pragma once

#include <cstdint>
#include <optional>

#include "sensor_calibration.hpp"
#include "sensor_lowpass_filter.hpp"
#include "sensor_median_filter.hpp"
#include "sensor_quality.hpp"
#include "sensor_quality_config.hpp"
#include "sensor_quality_snapshot.hpp"
#include "temperature_source.hpp"

// Orchestrator einer einzelnen Sensorqualitaets-/Plausibilitaetspipeline.
// Siehe docs/tasks/issue-20-sensor-quality-filtering-plan.md, Abschnitt 8-12,
// fuer die vollstaendige fachliche Herleitung. Slice 1 (dieser Stand) deckt
// Zeitstempel-/Dispositionspruefung (9b), Transport-/Wertebereichs-/
// Aenderungsratenpruefung (10.1/10.2), Median-/Offset-/Tiefpassintegration
// (10.3-10.5) und die Zustandsmaschine (8/9a) ab.
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

    // Ersetzt die bisher gesetzte Kalibrierung vollstaendig. Die neue
    // Kalibrierung wirkt erst auf die naechste plausible Probe.
    void setCalibration(std::optional<SensorCalibration> calibration);

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

    // Die zwei UNMITTELBAREN (nicht mit failedLatched_ kombinierten)
    // Failed-Bedingungen (Abschnitt 8): Alters- und Zaehlerueberschreitung.
    // Getrennt von deriveQuality() nutzbar, damit ingest() zwischen "Failed
    // ist gerade FRISCH (durch Alter/Zaehler) erreicht - eine liegen
    // gebliebene Teil-Wiedererkennungsfolge muss verworfen werden" und
    // "Failed besteht nur, WEIL eine Wiedererkennung noch unvollstaendig ist
    // (failedLatched_) - eine laufende Folge darf nicht durch ihre eigene
    // Zwischenprobe zurueckgesetzt werden" unterscheiden kann (Nachkorrektur
    // PR #95).
    struct RawFailureConditions {
        bool ageFailed;
        bool countFailed;
    };

    [[nodiscard]] SampleDisposition determineDisposition(
        const TemperatureReading& sample, uint64_t nowMonotonicMs) const;
    [[nodiscard]] RawFailureConditions computeRawFailureConditions(
        uint64_t referenceTimeMs) const;
    // Einzige Implementierung der Wiedererkennungsbedingung (Abschnitt 8:
    // kMinConsecutiveValidSamples plus Stabilitaetsdauer zwischen der ersten
    // und letzten gueltigen Probe der aktuellen Folge). Sie wird von
    // deriveQuality() und ingest() gemeinsam genutzt und haengt niemals vom
    // Zustell-/snapshot-Zeitpunkt ab.
    [[nodiscard]] bool isRecoveryComplete() const;
    [[nodiscard]] DerivedQuality deriveQuality(uint64_t referenceTimeMs) const;
    void recordValidSample(double celsius, uint64_t monotonicTimestampMs);
    void recordInvalidSample(SensorFaultReason reason);
    void resetFilterState();
    [[nodiscard]] std::optional<double> effectiveOffsetFor(
        const TemperatureReading& sample) const;

    SensorQualityConfig config_;
    MedianFilter medianFilter_;
    LowPassFilter lowPassFilter_;
    std::optional<SensorCalibration> calibration_;
    std::optional<double> correctedCelsius_;
    std::optional<double> appliedOffset_;
    // Interner Offset fuer die Tiefpass-Rechnung. Anders als
    // appliedOffset_ ist 0.0 auch bei fehlender Kalibrierung ein gueltiger
    // Rechenwert.
    std::optional<double> lastFilterEffectiveOffsetCelsius_;

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
    // - AUSSCHLIESSLICH fuer Diagnose (lastValidSampleAgeMs) und die
    // Stale-Altersschwelle (Abschnitt 8) bestimmt; bleibt gemaess Abschnitt 8
    // ("letzter gueltiger Wert ... bleibt fuer Diagnose erhalten") auch
    // waehrend Stale/Failed unveraendert stehen. Bewusst getrennt von
    // rawCelsius_/lastAcceptedTimestampMs_ (Korrektur Runde 3: ein extremer,
    // unplausibler Rohwert bleibt in rawCelsius_ sichtbar, darf aber NICHT
    // zur neuen Aenderungsratenreferenz werden) UND von
    // rateReferenceCelsius_/rateReferenceTimestampMs_ unten (Nachkorrektur
    // PR #95: die Aenderungsratenpruefung braucht den UNMITTELBAR
    // vorherigen gueltigen Wert, nicht bloss irgendeinen zuletzt gueltigen
    // Diagnosewert).
    std::optional<double> lastValidRawCelsius_;
    std::optional<uint64_t> lastValidTimestampMs_;

    // Referenz fuer die Aenderungsratenpruefung (10.2) allein - anders als
    // lastValidRawCelsius_/lastValidTimestampMs_ wird dieses Paar verworfen,
    // sobald der "unmittelbar vorherige gueltige Wert" nicht mehr gilt:
    // akzeptierte-aber-ungueltige Probe, bereits erreichtes Failed
    // (Abschnitt 9a-Vorzustand) oder bestaetigter ROM-Wechsel (der intern
    // ebenfalls eine ungueltige Probe ist). So wird die naechste plausible
    // Probe nach einer Luecke nie gegen einen veralteten oder fremden Wert
    // geprueft (Nachkorrektur PR #95).
    std::optional<double> rateReferenceCelsius_;
    std::optional<uint64_t> rateReferenceTimestampMs_;

    // Aufeinanderfolgende ungueltige bzw. gueltige Proben seit dem jeweils
    // letzten Gegenereignis - exakte Komplemente: eine gueltige Probe setzt
    // consecutiveInvalidCount_ auf 0 und erhoeht recoveryProgressCount_
    // (und umgekehrt bei einer ungueltigen Probe), siehe Abschnitt 8/9a.
    uint16_t consecutiveInvalidCount_{0};
    uint16_t recoveryProgressCount_{0};
    // Zeitstempel der ERSTEN Probe der aktuellen ununterbrochenen gueltigen
    // Folge - Grundlage fuer die Stabilitaetszeitpruefung (Abschnitt 8).
    std::optional<uint64_t> recoveryStreakStartTimestampMs_;
    // Zeitstempel der LETZTEN gueltigen Probe derselben Folge. Nur die Spanne
    // zwischen diesem und recoveryStreakStartTimestampMs_ darf die
    // Wiedererkennung abschliessen; snapshot(now) kann sie nicht fortschreiben.
    std::optional<uint64_t> recoveryStreakLastTimestampMs_;

    // Minimale rohe Zustandsinformation (KEINE zweite, unabhaengig
    // gepflegte Qualitaetszustandsmaschine): haelt fest, dass Failed bereits
    // erreicht wurde und die vollstaendige Wiedererkennungsbedingung
    // (Abschnitt 8: kMinConsecutiveValidSamples +
    // kMinRecoveryStabilityDurationMs) seither noch nicht vollstaendig erfuellt
    // wurde. Wird ausschliesslich in deriveQuality() GELESEN und
    // ausschliesslich in ingest() (bei neu erreichtem Failed bzw. bei
    // vollstaendiger Wiedererkennung) geschrieben - snapshot() bleibt dadurch
    // weiterhin rein lesend (Nachkorrektur PR #95, siehe Abschnitt 8 "erneute
    // ungueltige Probe waehrend Wiedererkennung ... bleibt ... Failed").
    bool failedLatched_{false};

    SensorFaultReason lastFaultReason_{SensorFaultReason::None};
    std::optional<double> changeRateCelsiusPerSecond_;
};

}  // namespace device_platform
