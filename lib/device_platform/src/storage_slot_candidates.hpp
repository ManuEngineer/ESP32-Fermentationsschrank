#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "state_store.hpp"
#include "state_store_key.hpp"
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

// Vereinheitlichte, anwendungsneutrale Einordnung eines uebersprungenen
// Slots. Unterscheidet insbesondere `NotFound` (Slot nie geschrieben, z. B.
// fabrikleerer Speicher) von jeder Form von Fehler oder Integritaetsproblem -
// beides darf beim Bootstrap/Recovery in #56/#57 niemals gleich behandelt
// werden.
enum class SlotIssueKind : uint8_t {
    // Lesefehler des Speicherports (`StateStoreStatus`).
    NotFound,
    ReadError,
    CapacityError,
    // Envelope strukturell ungueltig (`EnvelopeDecodeStatus`).
    InvalidMagic,
    UnknownEnvelopeVersion,
    InvalidRecordType,
    InvalidSchemaVersion,
    InvalidStorageEpoch,
    InvalidVersionValue,
    InvalidUtcTag,
    LengthMismatch,
    CrcMismatch,
    // Envelope technisch gueltig, aber RecordType, Schema-Version oder
    // StorageEpoch entsprechen nicht den Erwartungen.
    RecordIdentityMismatch,
};

struct SlotIssue {
    SlotId slot;
    SlotIssueKind kind;
};

struct SlotScanResult {
    // Technisch gueltige Kandidaten, absteigend nach `versionValue` sortiert
    // (Tiebreak: aufsteigende Slot-ID, da `std::sort` nicht stabil ist).
    std::vector<SlotCandidate> candidates;
    // Jeder uebersprungene Slot, in Scanreihenfolge, mit Grund. Geht nicht
    // verloren: eine leere `candidates`-Liste allein sagt nicht aus, ob der
    // Speicher fabrikleer oder vollstaendig unlesbar/korrupt ist - dafuer
    // muss `issues` geprueft werden.
    std::vector<SlotIssue> issues;
};

// Liest jeden `slotKeys`-Eintrag und dekodiert dessen Envelope rein
// technisch. Jeder Slot, der nicht zu einem technisch gueltigen und
// passenden Kandidaten fuehrt, erscheint mit einer eindeutigen `SlotIssue`
// im Ergebnis - nichts wird stillschweigend verworfen. `ReadError` wird nie
// wie `NotFound` behandelt. Kennt keine Bootstrap-, Root- oder konkrete
// Recovery-Logik; das bleibt Aufgabe der aufrufenden Anwendung (#56/#57).
[[nodiscard]] SlotScanResult scanTechnicalSlotCandidates(
    const IStateStore& store, const std::vector<StateStoreKey>& slotKeys,
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
