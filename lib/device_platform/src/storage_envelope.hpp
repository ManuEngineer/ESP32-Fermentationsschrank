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

// Lehnt RecordTypeId, Schema-Version, StorageEpoch und VersionValue 0 ab.
// `maxTotalBytes` begrenzt Header und Payload gemeinsam; bei Ueberschreitung
// oder einer Payloadgroesse, die nicht in das 32-Bit-Laengenfeld passt, wird
// `CapacityExceeded` geliefert und `outBytes` nicht veraendert.
[[nodiscard]] EnvelopeEncodeStatus encodeEnvelope(
    const StorageEnvelope& envelope, std::string& outBytes,
    std::size_t maxTotalBytes);

// Prueft Grenzen, Laengen und CRC vor jeder Rueckgabe von Payloadbytes.
// Unbekannte Envelope-Version wird abgelehnt; unbekannte ChangeOrigin-/
// ChangeOperation-Wire-IDs werden roh erhalten, nicht abgelehnt.
[[nodiscard]] EnvelopeDecodeResult decodeEnvelope(const std::string& bytes);

}  // namespace device_platform
