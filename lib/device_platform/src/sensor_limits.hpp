#pragma once

#include <cstddef>
#include <cstdint>

namespace device_platform::sensor_limits {

// Obergrenze fuer die Medianfenstergroesse (Abschnitt 10.3). Begrenzt
// Speicherbedarf und Sortieraufwand pro Probe auf einen festen, kleinen
// Ringpuffer, bleibt aber weit oberhalb jeder plausiblen Tuningwahl (wenige
// Werte). Muss nicht selbst ungerade sein - sie ist nur eine Obergrenze, die
// mindestens einen gueltigen ungeraden Konfigurationswert zulassen muss.
inline constexpr std::size_t kMaxMedianWindowSize = 15U;

// Generische physikalische Aussenkante (Abschnitt 10.2) fuer JEDE
// Temperaturfuehlerrolle dieses Geraetetyps und verwandter zukuenftiger
// Geraete (ADR-013). Enthaelt bewusst KEINE sensor-/treiberspezifische
// Kenntnis (z. B. keinen DS18B20-Einschalt- oder Diskonnektwert) - das ist
// eine reine Sicherheitsaussenkante, kein Tuningwert. Die eigentliche, enger
// gefasste Plausibilitaet je Rolle bleibt SensorQualityConfig
// (TBD_COMMISSIONING).
inline constexpr double kAbsoluteMinCelsius = -40.0;
inline constexpr double kAbsoluteMaxCelsius = 150.0;

// Obergrenze fuer den Betrag eines Kalibrier-Offsets (Abschnitt 13a). Ein
// realer ROM-Kalibrierwert liegt im Bereich weniger Grad; dieser Wert bleibt
// grosszuegig darueber, verhindert aber einen offensichtlich fehlerhaften
// Kalibrierwert (z. B. aus einem defekten Persistenzdatensatz).
inline constexpr double kMaxAbsoluteOffsetCelsius = 10.0;

// Obergrenze fuer das zulaessige Alter des letzten gueltigen Werts, bevor
// STALE spaetestens nach FAILED uebergeht (Abschnitt 8/10.0). Verhindert eine
// versehentlich "praktisch unendliche" Konfiguration; die tatsaechliche
// Tuningschwelle (Sekunden bis wenige Minuten bei einem ~2-s-Regelzyklus)
// bleibt TBD_COMMISSIONING und liegt weit darunter.
inline constexpr uint64_t kMaxStaleAgeCeilingMs = 86'400'000ULL;  // 24 h

// Obergrenze fuer die Anzahl aufeinanderfolgender ungueltiger Proben, die
// waehrend VALID toleriert werden, bevor FAILED erreicht wird (Abschnitt 8/
// 10.0). 1000 Proben sind selbst beim nominalen ~2-s-Regelzyklus
// (SENSOR_TUNING_COMMISSIONING.md) weit ueber jede plausible Tuningwahl
// hinaus, bleiben aber innerhalb von uint16_t weit unterhalb eines
// Ueberlaufrisikos.
inline constexpr uint16_t kMaxConsecutiveInvalidCeiling = 1000U;

// Obergrenze fuer die geforderte Stabilitaetszeit einer Wiedererkennungsfolge
// (Abschnitt 10.0). Verhindert eine versehentlich praktisch nie erfuellbare
// Wiedererkennungsbedingung; die tatsaechliche Tuningschwelle bleibt
// TBD_COMMISSIONING und liegt deutlich darunter.
inline constexpr uint64_t kMaxRecoveryStabilityDurationCeilingMs =
    3'600'000ULL;  // 1 h

// Obergrenze fuer die Anzahl aufeinanderfolgender gueltiger Proben, die fuer
// eine Wiedererkennung (Stale/Failed -> Valid, Abschnitt 8/10.0) verlangt
// werden duerfen. Dasselbe Prinzip wie kMaxConsecutiveInvalidCeiling oben -
// derselbe Wertebereich (Anzahl aufeinanderfolgender Proben), daher derselbe
// Zahlenwert - verhindert eine versehentlich praktisch nie erfuellbare
// Wiedererkennungsbedingung; die tatsaechliche Tuningschwelle bleibt
// TBD_COMMISSIONING und liegt weit darunter.
inline constexpr uint16_t kMaxConsecutiveValidSamplesCeiling = 1000U;

}  // namespace device_platform::sensor_limits
