#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "state_store.hpp"
#include "storage_types.hpp"

// Generische, rein technische Slot- und Kandidatenmechanik. Kennt keine
// konkrete Slotanzahl-Policy, Schutzmenge oder Recordbedeutung; das legt die
// aufrufende Anwendung fest (siehe docs/CONFIGURATION_PERSISTENCE.md,
// Abschnitt "Technisch gueltige Kandidaten und kanonischer Root").
namespace device_platform {

struct SlotCandidate {
    SlotId slot;
    // Rohes VersionValue aus dem Envelope; Bedeutung legt der Aufrufer fest.
    uint64_t versionValue{0U};
    std::string payload;
    std::optional<int64_t> utcUnixSeconds;
};

// Liest jeden `slotKeys`-Eintrag, dekodiert dessen Envelope rein technisch
// und behaelt nur Kandidaten, deren Envelope technisch gueltig ist und deren
// RecordType, Schema-Version und StorageEpoch exakt den Erwartungen
// entsprechen. Fehlende, fehlerhafte oder nicht passende Slots werden
// stillschweigend ausgelassen, nicht als Fehler gemeldet - Aufrufer, die eine
// leere Kandidatenliste als Problem behandeln wollen, pruefen dies selbst.
// Ergebnis absteigend nach `versionValue` sortiert.
[[nodiscard]] std::vector<SlotCandidate> technicalCandidatesDescending(
    const IStateStore& store, const std::vector<std::string>& slotKeys,
    RecordTypeId expectedRecordType, uint32_t expectedSchemaVersion,
    StorageEpoch expectedStorageEpoch, std::size_t maxEnvelopeBytes);

// Rein technische Rotation zwischen `slotCount` Slots (naechster Index nach
// `lastWrittenSlot`, modulo `slotCount`). Kennt keine Schutzmenge; die
// aufrufende Anwendung muss vor dem Schreiben selbst pruefen, dass der
// gewaehlte Slot nicht Teil ihrer aktuellen Schutzmenge ist. `slotCount == 0`
// liefert `SlotId(0)`.
[[nodiscard]] SlotId nextSlotRoundRobin(SlotId lastWrittenSlot,
                                        std::size_t slotCount);

}  // namespace device_platform
