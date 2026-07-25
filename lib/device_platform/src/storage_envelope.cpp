#include "storage_envelope.hpp"

#include <cstring>
#include <limits>

#include "big_endian_codec.hpp"
#include "byte_buffer.hpp"
#include "checked_size.hpp"
#include "crc32.hpp"

namespace device_platform {

namespace {

constexpr char kMagic[4] = {'D', 'P', 'R', 'F'};

// Feldgroessen vor dem CRC-Feld, ohne optionale UTC-Sekunden:
// Magic(4) + EnvelopeVersion(2) + RecordTypeId(2) + SchemaVersion(4) +
// StorageEpoch(8) + VersionValue(8) + Payloadlaenge(4) + UtcTag(1) = 33 Bytes.
constexpr std::size_t kHeaderBeforeCrcWithoutUtc = 33U;
constexpr std::size_t kUtcValueSize = 8U;
constexpr std::size_t kCrcSize = 4U;

// Zwischenergebnis der Kernfelder zwischen RecordTypeId und Payloadlaenge
// (siehe Envelope-Feldreihenfolge). Ausgelagert, um `decodeEnvelope()` klein
// und die kognitive Komplexitaet gering zu halten.
struct CoreFieldsResult {
    EnvelopeDecodeStatus status{EnvelopeDecodeStatus::LengthMismatch};
    uint16_t recordTypeRaw{0U};
    uint32_t schemaVersion{0U};
    uint64_t storageEpochRaw{0U};
    uint64_t versionValue{0U};
    uint32_t payloadLength{0U};
};

CoreFieldsResult readCoreFields(ByteReader& reader) {
    CoreFieldsResult result;
    if (!big_endian::readUint16(reader, result.recordTypeRaw)) {
        return result;
    }
    if (result.recordTypeRaw == 0U) {
        result.status = EnvelopeDecodeStatus::InvalidRecordType;
        return result;
    }
    if (!big_endian::readUint32(reader, result.schemaVersion)) {
        return result;
    }
    if (result.schemaVersion == 0U) {
        result.status = EnvelopeDecodeStatus::InvalidSchemaVersion;
        return result;
    }
    if (!big_endian::readUint64(reader, result.storageEpochRaw)) {
        return result;
    }
    if (result.storageEpochRaw == 0U) {
        result.status = EnvelopeDecodeStatus::InvalidStorageEpoch;
        return result;
    }
    if (!big_endian::readUint64(reader, result.versionValue)) {
        return result;
    }
    if (result.versionValue == 0U) {
        result.status = EnvelopeDecodeStatus::InvalidVersionValue;
        return result;
    }
    if (!big_endian::readUint32(reader, result.payloadLength)) {
        return result;
    }
    result.status = EnvelopeDecodeStatus::Success;
    return result;
}

struct UtcFieldResult {
    EnvelopeDecodeStatus status{EnvelopeDecodeStatus::LengthMismatch};
    std::optional<int64_t> utcUnixSeconds;
};

UtcFieldResult readUtcField(ByteReader& reader) {
    UtcFieldResult result;
    bool hasUtc = false;
    if (!big_endian::readOptionalTag(reader, hasUtc)) {
        result.status = EnvelopeDecodeStatus::InvalidUtcTag;
        return result;
    }
    if (hasUtc) {
        int64_t value = 0;
        if (!big_endian::readInt64(reader, value)) {
            return result;
        }
        result.utcUnixSeconds = value;
    }
    result.status = EnvelopeDecodeStatus::Success;
    return result;
}

// Validiert Magic, Version, Kernfelder, UTC-Feld, Laenge und CRC und liefert
// die Kernfelder samt Payload-Offset - ohne die Payload zu kopieren. Der CRC
// wird direkt ueber den Header- und den Payloadausschnitt des Eingabepuffers
// berechnet. Gemeinsame Grundlage von `decodeEnvelope()` (materialisiert die
// Payload) und `decodeEnvelopeMetadata()` (materialisiert sie nicht).
struct ValidatedEnvelope {
    EnvelopeDecodeStatus status{EnvelopeDecodeStatus::LengthMismatch};
    CoreFieldsResult core;
    std::optional<int64_t> utcUnixSeconds;
    // Offset der Payload im Eingabepuffer; nur bei `status == Success` gueltig.
    std::size_t payloadOffset{0U};
};

ValidatedEnvelope validateEnvelope(const std::string& bytes) {
    ValidatedEnvelope out;
    if (bytes.size() < kHeaderBeforeCrcWithoutUtc + kCrcSize) {
        return out;
    }

    ByteReader reader(bytes);
    char magic[sizeof(kMagic)];
    if (!reader.readBytes(magic, sizeof(magic))) {
        return out;
    }
    if (std::memcmp(magic, kMagic, sizeof(kMagic)) != 0) {
        out.status = EnvelopeDecodeStatus::InvalidMagic;
        return out;
    }

    uint16_t envelopeVersion = 0U;
    if (!big_endian::readUint16(reader, envelopeVersion)) {
        return out;
    }
    if (envelopeVersion != kStorageEnvelopeVersion1) {
        out.status = EnvelopeDecodeStatus::UnknownEnvelopeVersion;
        return out;
    }

    out.core = readCoreFields(reader);
    if (out.core.status != EnvelopeDecodeStatus::Success) {
        out.status = out.core.status;
        return out;
    }
    const auto utc = readUtcField(reader);
    if (utc.status != EnvelopeDecodeStatus::Success) {
        out.status = utc.status;
        return out;
    }
    out.utcUnixSeconds = utc.utcUnixSeconds;

    // Position hier ist exakt die Headerlaenge vor dem CRC-Feld.
    const std::size_t headerBeforeCrcLen = reader.position();
    if (reader.remaining() < kCrcSize) {
        out.status = EnvelopeDecodeStatus::LengthMismatch;
        return out;
    }
    if (reader.remaining() - kCrcSize != out.core.payloadLength) {
        out.status = EnvelopeDecodeStatus::LengthMismatch;
        return out;
    }

    uint32_t storedCrc = 0U;
    if (!big_endian::readUint32(reader, storedCrc)) {
        out.status = EnvelopeDecodeStatus::LengthMismatch;
        return out;
    }
    const std::size_t payloadOffset = headerBeforeCrcLen + kCrcSize;

    // CRC direkt ueber den Header- und den Payloadausschnitt des vorhandenen
    // Eingabepuffers, ohne die Payload zu materialisieren. `bytes.data()` ist
    // nie `nullptr` (Mindestlaenge oben geprueft); `bytes.data() +
    // payloadOffset` ist bei `payloadLength == 0` ein gueltiger
    // Ein-hinter-Ende-Zeiger, den `update()` mit Laenge 0 nicht dereferenziert.
    Crc32IsoHdlc crcAccumulator;
    if (!crcAccumulator.update(bytes.data(), headerBeforeCrcLen) ||
        !crcAccumulator.update(bytes.data() + payloadOffset,
                               out.core.payloadLength)) {
        out.status = EnvelopeDecodeStatus::LengthMismatch;
        return out;
    }
    if (crcAccumulator.finalize() != storedCrc) {
        out.status = EnvelopeDecodeStatus::CrcMismatch;
        return out;
    }

    out.status = EnvelopeDecodeStatus::Success;
    out.payloadOffset = payloadOffset;
    return out;
}

}  // namespace

EnvelopeSizeCheckResult checkEnvelopeEncodedSize(std::size_t payloadSize,
                                                 bool hasUtc,
                                                 std::size_t maxTotalBytes) {
    // Eine Payloadgroesse, die nicht in das 32-Bit-Wire-Laengenfeld passt,
    // ist eine technische Kapazitaetsgrenze des Wireformats, kein fachlicher
    // Feldfehler - konsistent `CapacityExceeded` statt `InvalidField`.
    if (payloadSize >
        static_cast<std::size_t>(std::numeric_limits<uint32_t>::max())) {
        return EnvelopeSizeCheckResult{EnvelopeEncodeStatus::CapacityExceeded,
                                       0U};
    }

    // Jede Teiladdition ueberlaufsicher und gegen `maxTotalBytes` geprueft,
    // bevor irgendetwas reserviert wird: `std::size_t` ist plattformabhaengig
    // (ESP32: 32 Bit) und darf nicht ungeprueft ueberlaufen.
    std::size_t headerBeforeCrc = kHeaderBeforeCrcWithoutUtc;
    if (hasUtc && !checkedAddSize(headerBeforeCrc, kUtcValueSize, maxTotalBytes,
                                  headerBeforeCrc)) {
        return EnvelopeSizeCheckResult{EnvelopeEncodeStatus::CapacityExceeded,
                                       0U};
    }
    std::size_t headerWithCrc = 0U;
    if (!checkedAddSize(headerBeforeCrc, kCrcSize, maxTotalBytes,
                        headerWithCrc)) {
        return EnvelopeSizeCheckResult{EnvelopeEncodeStatus::CapacityExceeded,
                                       0U};
    }
    std::size_t totalSize = 0U;
    if (!checkedAddSize(headerWithCrc, payloadSize, maxTotalBytes, totalSize)) {
        return EnvelopeSizeCheckResult{EnvelopeEncodeStatus::CapacityExceeded,
                                       0U};
    }
    return EnvelopeSizeCheckResult{EnvelopeEncodeStatus::Success, totalSize};
}

EnvelopeEncodeStatus encodeEnvelope(const StorageEnvelope& envelope,
                                    std::string& outBytes,
                                    std::size_t maxTotalBytes) {
    if (envelope.recordTypeId.value() == 0U || envelope.schemaVersion == 0U ||
        envelope.storageEpoch.value() == 0U || envelope.versionValue == 0U) {
        return EnvelopeEncodeStatus::InvalidField;
    }

    const auto sizeCheck = checkEnvelopeEncodedSize(
        envelope.payload.size(), envelope.utcUnixSeconds.has_value(),
        maxTotalBytes);
    if (sizeCheck.status != EnvelopeEncodeStatus::Success) {
        return sizeCheck.status;
    }
    const std::size_t totalSize = sizeCheck.totalSize;
    std::size_t headerBeforeCrc = kHeaderBeforeCrcWithoutUtc;
    if (envelope.utcUnixSeconds.has_value()) {
        headerBeforeCrc += kUtcValueSize;
    }

    ByteWriter header(headerBeforeCrc);
    bool ok = header.writeBytes(kMagic, sizeof(kMagic));
    ok = ok && big_endian::writeUint16(header, kStorageEnvelopeVersion1);
    ok = ok && big_endian::writeUint16(header, envelope.recordTypeId.value());
    ok = ok && big_endian::writeUint32(header, envelope.schemaVersion);
    ok = ok && big_endian::writeUint64(header, envelope.storageEpoch.value());
    ok = ok && big_endian::writeUint64(header, envelope.versionValue);
    ok = ok && big_endian::writeUint32(
                   header, static_cast<uint32_t>(envelope.payload.size()));
    ok = ok && big_endian::writeOptionalTag(
                   header, envelope.utcUnixSeconds.has_value());
    if (ok && envelope.utcUnixSeconds.has_value()) {
        ok = big_endian::writeInt64(header, *envelope.utcUnixSeconds);
    }
    if (!ok) {
        return EnvelopeEncodeStatus::CapacityExceeded;
    }

    // CRC inkrementell ueber Header und Payload, ohne einen zusaetzlichen
    // `header + payload`-Hilfspuffer anzulegen (siehe
    // docs/CONFIGURATION_PERSISTENCE.md, Abschnitt "Ressourcenvertrag").
    // Beide Aufrufe uebergeben `std::string`-Daten (nie `nullptr`), koennen
    // also nach ihrem Vertrag nie `false` liefern. Der Rueckgabewert wird
    // trotzdem explizit behandelt, damit eine spaetere Aenderung dieser
    // Vorbedingung nicht unbemerkt einen CRC aus Teilinput veroeffentlicht.
    Crc32IsoHdlc crcAccumulator;
    if (!crcAccumulator.update(header.bytes()) ||
        !crcAccumulator.update(envelope.payload)) {
        return EnvelopeEncodeStatus::CapacityExceeded;
    }
    const uint32_t crc = crcAccumulator.finalize();

    ByteWriter finalWriter(totalSize);
    ok = finalWriter.writeBytes(header.bytes().data(), header.bytes().size());
    ok = ok && big_endian::writeUint32(finalWriter, crc);
    ok = ok && finalWriter.writeBytes(envelope.payload.data(),
                                      envelope.payload.size());
    if (!ok) {
        return EnvelopeEncodeStatus::CapacityExceeded;
    }
    // Veroeffentlicht das Ergebnis erst nach vollstaendigem Erfolg per
    // `swap()`: hoechstens ein zusaetzlicher, neu aufgebauter vollstaendiger
    // Recordpuffer (`encoded`) entsteht, keine Vollkopie. Ein von der
    // aufrufenden Anwendung bereits gehaltener alter Wert in `outBytes`
    // bleibt bis zu genau dieser Zeile vollstaendig bestehen und wird erst
    // hier durch den neuen Wert ersetzt - siehe
    // docs/CONFIGURATION_PERSISTENCE.md, Abschnitt "Ressourcenvertrag" fuer
    // die praezise, nicht absolute Formulierung dieser Garantie.
    auto encoded = finalWriter.takeBytes();
    outBytes.swap(encoded);
    return EnvelopeEncodeStatus::Success;
}

EnvelopeDecodeResult decodeEnvelope(const std::string& bytes) {
    const auto validated = validateEnvelope(bytes);
    if (validated.status != EnvelopeDecodeStatus::Success) {
        return {validated.status, std::nullopt};
    }
    const auto& core = validated.core;

    // Einzige Materialisierung der Payload: eine Kopie aus dem bereits
    // validierten Eingabepuffer in das Ergebnis.
    std::string payload(core.payloadLength, '\0');
    if (core.payloadLength > 0U) {
        std::memcpy(payload.data(), bytes.data() + validated.payloadOffset,
                    core.payloadLength);
    }

    StorageEnvelope result;
    result.recordTypeId = RecordTypeId(core.recordTypeRaw);
    result.schemaVersion = core.schemaVersion;
    result.storageEpoch = StorageEpoch(core.storageEpochRaw);
    result.versionValue = core.versionValue;
    result.utcUnixSeconds = validated.utcUnixSeconds;
    result.payload = std::move(payload);
    return {EnvelopeDecodeStatus::Success, std::move(result)};
}

EnvelopeMetadataResult decodeEnvelopeMetadata(const std::string& bytes) {
    const auto validated = validateEnvelope(bytes);
    if (validated.status != EnvelopeDecodeStatus::Success) {
        return {validated.status, std::nullopt};
    }
    const auto& core = validated.core;

    EnvelopeMetadata metadata;
    metadata.recordTypeId = RecordTypeId(core.recordTypeRaw);
    metadata.schemaVersion = core.schemaVersion;
    metadata.storageEpoch = StorageEpoch(core.storageEpochRaw);
    metadata.versionValue = core.versionValue;
    metadata.utcUnixSeconds = validated.utcUnixSeconds;
    metadata.payloadLength = core.payloadLength;
    return {EnvelopeDecodeStatus::Success, metadata};
}

}  // namespace device_platform
