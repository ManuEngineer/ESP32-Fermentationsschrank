#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

#include "storage_types.hpp"

// Generischer Envelope Version 1. Anwendungsneutraler Speicherrahmen ohne
// Kenntnis konkreter Dokumente, Manifestbedeutung oder fachlicher
// ChangeOrigin-/ChangeOperation-Bedeutung. Siehe
// docs/CONFIGURATION_PERSISTENCE.md, Abschnitt "Envelope-Version 1".
namespace device_platform {

inline constexpr uint16_t kStorageEnvelopeVersion1 = 1U;

struct StorageEnvelope {
    RecordTypeId recordTypeId;
    uint32_t schemaVersion{0U};
    StorageEpoch storageEpoch;
    // Rohes VersionValue; wird je Record-Type vom Aufrufer als Revision,
    // Generation oder RecordSequence interpretiert (siehe storage_types.hpp).
    uint64_t versionValue{0U};
    // Rohe Wire-IDs ohne Interpretation durch device_platform.
    uint16_t changeOriginWireId{0U};
    uint16_t changeOperationWireId{0U};
    std::optional<int64_t> utcUnixSeconds;
    std::string payload;
};

enum class EnvelopeEncodeStatus : uint8_t {
    Success,
    InvalidField,
    CapacityExceeded,
};

enum class EnvelopeDecodeStatus : uint8_t {
    Success,
    InvalidMagic,
    UnknownEnvelopeVersion,
    InvalidRecordType,
    InvalidSchemaVersion,
    InvalidStorageEpoch,
    InvalidVersionValue,
    InvalidUtcTag,
    LengthMismatch,
    CrcMismatch,
};

struct EnvelopeDecodeResult {
    EnvelopeDecodeStatus status{EnvelopeDecodeStatus::LengthMismatch};
    std::optional<StorageEnvelope> envelope;
};

// Technischer, anwendungsneutraler Baustein: liefert dieselbe
// Groessenentscheidung wie `encodeEnvelope()` (einschliesslich der
// 32-Bit-Laengenfeldgrenze fuer die Payload), ohne einen Puffer anzulegen.
// `encodeEnvelope()` nutzt dies intern als einzige Quelle der Wahrheit;
// Tests koennen damit Grenzfaelle nahe `UINT32_MAX`/`maxTotalBytes` pruefen,
// ohne eine reale Payload dieser Groesse zu allokieren (`payloadSize` ist nur
// ein Zahlenwert, keine tatsaechlichen Daten).
struct EnvelopeSizeCheckResult {
    EnvelopeEncodeStatus status{EnvelopeEncodeStatus::CapacityExceeded};
    // Nur bei `Success` gueltig: Gesamtgroesse des kodierten Records.
    std::size_t totalSize{0U};
};

[[nodiscard]] EnvelopeSizeCheckResult checkEnvelopeEncodedSize(
    std::size_t payloadSize, bool hasUtc, std::size_t maxTotalBytes);

// Lehnt RecordTypeId, Schema-Version, StorageEpoch und VersionValue 0 als
// `InvalidField` ab. Eine Payloadgroesse, die nicht in das 32-Bit-Laengenfeld
// passt, sowie jede Ueberschreitung von `maxTotalBytes` (Header und Payload
// gemeinsam) wird konsistent als `CapacityExceeded` geliefert - beides ist
// eine technische Kapazitaetsgrenze des Wireformats, kein fachlicher
// Feldfehler. `outBytes` bleibt bei jeder Ablehnung vollstaendig unveraendert
// (weder Groesse noch Inhalt).
//
// Ressourcenvertrag (siehe docs/CONFIGURATION_PERSISTENCE.md, Abschnitt
// "Ressourcenvertrag" fuer die vollstaendige Herleitung): der Encoder baut
// hoechstens einen zusaetzlichen, neu aufgebauten vollstaendigen
// Recordpuffer auf und veroeffentlicht ihn per `swap()`, nie per Vollkopie.
// Das ist keine absolute Aussage ueber die gesamte Aufrufdauer: haelt die
// aufrufende Anwendung in `outBytes` bereits einen alten vollstaendigen
// Record, bleibt dieser bis zur erfolgreichen `swap()`-Zeile bestehen -
// waehrend dieses kurzen Zeitraums existieren alter und neuer Puffer
// gleichzeitig. Die staerkere, absolute "waehrend eines Commits existiert
// global hoechstens ein vollstaendiger Recordpuffer"-Garantie ist Aufgabe
// des aufrufenden Commit-Workflows (#56/#57), nicht dieser Funktion allein.
[[nodiscard]] EnvelopeEncodeStatus encodeEnvelope(
    const StorageEnvelope& envelope, std::string& outBytes,
    std::size_t maxTotalBytes);

// Prueft Grenzen, Laengen und CRC vor jeder Rueckgabe von Payloadbytes.
// Unbekannte Envelope-Version wird abgelehnt; unbekannte ChangeOrigin-/
// ChangeOperation-Wire-IDs werden roh erhalten, nicht abgelehnt.
[[nodiscard]] EnvelopeDecodeResult decodeEnvelope(const std::string& bytes);

}  // namespace device_platform
