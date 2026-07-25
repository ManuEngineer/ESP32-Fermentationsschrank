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

// Nur die Metadaten eines Kandidaten, ohne die Payload: der Scan haelt zu
// keinem Zeitpunkt mehr als einen Recordpuffer, und die Payload wird erst
// spaeter gezielt ueber `loadSlotPayload()` materialisiert.
struct SlotCandidate {
    SlotId slot;
    // Rohes VersionValue aus dem Envelope; Bedeutung legt der Aufrufer fest.
    uint64_t versionValue{0U};
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
    // Fallback fuer den an der Aufrufstelle nicht erreichbaren
    // `Success`-Zweig der `toSlotIssueKind()`-Mapper; haelt deren `switch` ohne
    // `default` vollstaendig. Nie mit einem echten Lese- oder Envelope-Fehler
    // (z. B. `ReadError`/`LengthMismatch`) zu verwechseln.
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

// Liest jeden `slotKeys`-Eintrag und validiert dessen Envelope rein
// technisch, ohne die Payload zu materialisieren (`decodeEnvelopeMetadata()`).
// Zu keinem Zeitpunkt liegt mehr als ein Recordpuffer im Speicher, und das
// Ergebnis enthaelt nur Kandidatenmetadaten - der Spitzenspeicherbedarf ist
// damit unabhaengig von Slotanzahl und Kandidatenzahl. Die Payload eines
// gewaehlten Kandidaten wird erst spaeter ueber `loadSlotPayload()` geladen.
//
// Jeder Slot, der nicht zu einem technisch gueltigen und passenden Kandidaten
// fuehrt, erscheint mit einer eindeutigen `SlotIssue` im Ergebnis - nichts
// wird stillschweigend verworfen. `ReadError` wird nie wie `NotFound`
// behandelt. Kennt keine Bootstrap-, Root- oder konkrete Recovery-Logik; das
// bleibt Aufgabe der aufrufenden Anwendung.
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

enum class SlotPayloadLoadStatus : uint8_t {
    Success,
    NotFound,
    ReadError,
    CapacityError,
    // Envelope strukturell ungueltig oder CRC-Fehler.
    InvalidEnvelope,
    // Envelope technisch gueltig, aber RecordType, Schema-Version oder
    // StorageEpoch passen nicht zu den erwarteten Werten.
    RecordIdentityMismatch,
    // Der jetzt gelesene `versionValue` weicht vom beim Scan gesehenen Wert
    // ab: der Slot wurde zwischen Scan und Laden veraendert.
    VersionValueMismatch,
};

struct SlotPayloadResult {
    SlotPayloadLoadStatus status{SlotPayloadLoadStatus::NotFound};
    // Nur bei `status == Success` gueltig.
    std::string payload;
    std::optional<int64_t> utcUnixSeconds;
};

// Laedt die Payload eines beim Scan ausgewaehlten Slots und validiert sie
// vollstaendig neu: CRC und Record-Identitaet (RecordType, Schema-Version,
// StorageEpoch) wie beim Scan, zusaetzlich `versionValue` gegen den beim Scan
// gesehenen `expectedVersionValue`. Erst damit ist der Weg vom Kandidaten zur
// tatsaechlich verwendeten Payload durchgehend geprueft; ein zwischen Scan und
// Laden veraenderter Slot wird nicht unbemerkt uebernommen. Materialisiert
// genau eine Payload (die des gewaehlten Slots).
[[nodiscard]] SlotPayloadResult loadSlotPayload(
    const IStateStore& store, const StateStoreKey& slotKey,
    RecordTypeId expectedRecordType, uint32_t expectedSchemaVersion,
    StorageEpoch expectedStorageEpoch, uint64_t expectedVersionValue,
    std::size_t maxEnvelopeBytes);

enum class NextSlotStatus : uint8_t {
    Success,
    // `slotCount == 0` oder technisch nicht darstellbar (`> UINT32_MAX`).
    // `SlotId(0)` ist ein vollkommen gueltiger physischer Slot - ohne diesen
    // eigenen Status waere eine erfolgreiche Rotation zu Slot 0 nicht von
    // einer ungueltigen Slotanzahl unterscheidbar.
    InvalidSlotCount,
    // `lastWrittenSlot` liegt ausserhalb des gueltigen Bereichs
    // (`>= slotCount`), z. B. aus korruptem Speicher. Wird beobachtbar
    // abgelehnt statt still per Modulo in einen gueltigen Wert verwandelt -
    // und ist von `InvalidSlotCount` unterscheidbar.
    InvalidLastSlot,
};

struct NextSlotResult {
    NextSlotStatus status{NextSlotStatus::InvalidSlotCount};
    // Nur bei `Success` gueltig; sonst `std::nullopt`.
    std::optional<SlotId> slot;
};

// Rein technische Rotation zwischen `slotCount` Slots (naechster Index nach
// `lastWrittenSlot`). Kennt keine Schutzmenge; die aufrufende Anwendung muss
// vor dem Schreiben selbst pruefen, dass der gewaehlte Slot nicht Teil ihrer
// aktuellen Schutzmenge ist. `slotCount == 0` oder ein technisch nicht
// darstellbares `slotCount > UINT32_MAX` liefern `InvalidSlotCount`; ein
// `lastWrittenSlot >= slotCount` liefert `InvalidLastSlot` - beide ohne Slot,
// nie einen scheinbar gueltigen `SlotId(0)`. Bei gueltigen Eingaben gilt
// `lastWrittenSlot < slotCount <= UINT32_MAX`, daher kann
// `lastWrittenSlot.value() + 1` unabhaengig von der `size_t`-Breite der
// Zielplattform nie ueberlaufen.
[[nodiscard]] NextSlotResult nextSlotRoundRobin(SlotId lastWrittenSlot,
                                                std::size_t slotCount);

}  // namespace device_platform
