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
    // Lesefehler des Speicherports (`StateStoreReadStatus`).
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
    // Nachweislich unerreichbarer Fallback: an der einzigen Aufrufstelle
    // kann `toSlotIssueKind()` nie mit `Success` aufgerufen werden (siehe
    // storage_slot_candidates.cpp). Existiert nur, damit der `switch` ueber
    // `StateStoreReadStatus`/`EnvelopeDecodeStatus` ohne `default` vollstaendig
    // bleibt, und darf niemals still mit einem echten Lese- oder
    // Envelope-Fehler (z. B. `ReadError`/`LengthMismatch`) verwechselt werden.
    UnexpectedStatus,
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
//
// Vorbedingung: `slotKeys.size() <= UINT32_MAX`, da jeder Index als `SlotId`
// (`uint32_t`-getaggt) dargestellt wird. Realistische Slotzahlen sind sehr
// klein (siehe docs/CONFIGURATION_PERSISTENCE.md); ein Aufruf mit mehr als
// `UINT32_MAX` Eintraegen ist technisch nicht sinnvoll unterstuetzbar und
// liegt ausserhalb des Vertrags dieser Funktion.
[[nodiscard]] SlotScanResult scanTechnicalSlotCandidates(
    const IStateStore& store, const std::vector<StateStoreKey>& slotKeys,
    RecordTypeId expectedRecordType, uint32_t expectedSchemaVersion,
    StorageEpoch expectedStorageEpoch, std::size_t maxEnvelopeBytes);

enum class NextSlotStatus : uint8_t {
    Success,
    // `slotCount == 0` oder technisch nicht darstellbar (`> UINT32_MAX`).
    // `SlotId(0)` ist ein vollkommen gueltiger physischer Slot - ohne diesen
    // eigenen Status waere eine erfolgreiche Rotation zu Slot 0 nicht von
    // einer ungueltigen Slotanzahl unterscheidbar.
    InvalidSlotCount,
};

struct NextSlotResult {
    NextSlotStatus status{NextSlotStatus::InvalidSlotCount};
    // Nur bei `Success` gueltig; sonst `std::nullopt`.
    std::optional<SlotId> slot;
};

// Rein technische Rotation zwischen `slotCount` Slots (naechster Index nach
// `lastWrittenSlot`, modulo `slotCount`). Kennt keine Schutzmenge; die
// aufrufende Anwendung muss vor dem Schreiben selbst pruefen, dass der
// gewaehlte Slot nicht Teil ihrer aktuellen Schutzmenge ist. `slotCount == 0`
// oder ein technisch nicht darstellbares `slotCount > UINT32_MAX` liefern
// `InvalidSlotCount` ohne Slot - nie einen scheinbar gueltigen `SlotId(0)`.
// Ueberlaufsicher unabhaengig von der `size_t`-Breite der Zielplattform:
// `lastWrittenSlot` wird zuerst modulo `slotCount` reduziert, bevor eins
// addiert wird, damit `lastWrittenSlot.value() + 1` nie in der Naehe von
// `UINT32_MAX` ueberlaufen kann.
[[nodiscard]] NextSlotResult nextSlotRoundRobin(SlotId lastWrittenSlot,
                                                std::size_t slotCount);

}  // namespace device_platform
