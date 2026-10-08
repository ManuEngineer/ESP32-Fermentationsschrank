#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "ds18b20_bus.hpp"
#include "ds18b20_sampling_engine.hpp"
#include "sensor_calibration.hpp"
#include "sensor_offset.hpp"

// Persistierter Sensor-Inbetriebnahmedatensatz (Issue #30, Plan Abschnitt 5.3):
// ROM, Rolle (durch die Position) und Offset je ROM. Referenzmessgeraet,
// Referenztemperatur, Datum und Bedienquelle gehoeren zu #34/#28 und sind
// nicht Teil dieses Datensatzes. Die fachlichen Rollen (Schrankluft,
// Kuehlkoerper, Produkt) und ihre Abbildung auf die technischen Kanaele der
// Plattform liegen ausschliesslich hier (ADR-013, Plan Abschnitt 4c).
namespace fermentation {

// Technische Kanalindizes der DS18B20-Engine (device_platform).
inline constexpr uint8_t kChamberAirChannel = 0U;
inline constexpr uint8_t kHeatsinkChannel = 1U;
inline constexpr uint8_t kProductChannel =
    device_platform::Ds18b20SamplingEngine::kSingleDeviceChannel;

// Entwurfsgrenze fuer eine begrenzte Payload (kein Hardwarebudget).
inline constexpr std::size_t kMaximumKnownProductProbes = 4U;

struct SensorRomOffset {
    device_platform::OneWireRom rom{0U};
    device_platform::SensorOffset offset;
};

enum class SensorCommissioningStatus : uint8_t {
    Success,
    MissingFixedRole,
    ZeroRom,
    DuplicateRom,
    TooManyProductProbes,
};

struct SensorCommissioningRecord {
    std::optional<SensorRomOffset> chamberAir;
    std::optional<SensorRomOffset> heatsink;
    std::vector<SensorRomOffset> productProbes;

    // Kalibrierung (Identitaet + Offset) fuer ein ROM des Datensatzes;
    // nullopt fuer ein unbekanntes ROM (dann gilt kein Offset).
    [[nodiscard]] std::optional<device_platform::SensorCalibration>
    calibrationFor(device_platform::OneWireRom rom) const;
};

// Gueltig nur, wenn beide festen Rollen gesetzt sind, alle ROMs != 0 und
// paarweise verschieden sind (auch gegen die Produktfuehler) und hoechstens
// kMaximumKnownProductProbes Produktfuehler vorliegen.
[[nodiscard]] SensorCommissioningStatus validateSensorCommissioning(
    const SensorCommissioningRecord& record);

[[nodiscard]] bool operator==(const SensorRomOffset& left,
                              const SensorRomOffset& right);
[[nodiscard]] bool operator==(const SensorCommissioningRecord& left,
                              const SensorCommissioningRecord& right);

// Abbildung auf die technische Kanalbindung der Plattform: Schrankluft ->
// Kanal 0, Kuehlkoerper -> Kanal 1. Ein leerer oder ungueltiger Datensatz
// ergibt eine leere Bindung (die Engine bleibt ungebunden und liefert nie Ok).
[[nodiscard]] std::array<
    std::optional<device_platform::Ds18b20ChannelBinding>,
    device_platform::Ds18b20SamplingEngine::kExpectedRomChannelCount>
toChannelBindings(const std::optional<SensorCommissioningRecord>& record);

// Kommando-Parser fuer den bring-up-only Schreibpfad (Plan 5.3/5.4). Rein, ohne
// Seiteneffekte: bildet eine Eingabezeile auf eine Aktion ab oder lehnt sie ab.
enum class SensorCommissioningCommandKind : uint8_t {
    Rejected,
    Report,
    Bind,
    Offset,
    Clear,
};

enum class SensorCommissioningTarget : uint8_t {
    ChamberAir,
    Heatsink,
    Product
};

struct SensorCommissioningCommand {
    SensorCommissioningCommandKind kind{
        SensorCommissioningCommandKind::Rejected};
    // Bind
    std::optional<device_platform::OneWireRom> chamberAirRom;
    std::optional<device_platform::OneWireRom> heatsinkRom;
    // Offset
    SensorCommissioningTarget target{SensorCommissioningTarget::ChamberAir};
    std::optional<device_platform::OneWireRom> offsetRom;  // nur Produkt
    std::optional<int32_t> offsetMilliCelsius;
};

[[nodiscard]] SensorCommissioningCommand parseSensorCommissioningCommand(
    std::string_view line);

// Wendet ein geparstes Kommando auf den aktuellen Datensatz an und liefert
// den neuen Datensatz (nullopt = Datensatz loeschen) oder keinen Wert, wenn
// das Ergebnis ungueltig waere (Report/Rejected aendern nichts).
struct SensorCommissioningApplyResult {
    bool accepted{false};
    // true: der Datensatz soll gesetzt/ersetzt werden; false + accepted:
    // loeschen.
    bool hasRecord{false};
    SensorCommissioningRecord record;
};

[[nodiscard]] SensorCommissioningApplyResult applySensorCommissioningCommand(
    const std::optional<SensorCommissioningRecord>& current,
    const SensorCommissioningCommand& command);

}  // namespace fermentation
