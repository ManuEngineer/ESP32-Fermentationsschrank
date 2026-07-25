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
// StorageEpoch(8) + VersionValue(8) + Payloadlaenge(4) + ChangeOrigin(2) +
// ChangeOperation(2) + UtcTag(1) = 37 Bytes.
constexpr std::size_t kHeaderBeforeCrcWithoutUtc = 37U;
constexpr std::size_t kUtcValueSize = 8U;
constexpr std::size_t kCrcSize = 4U;

// Zwischenergebnis der Felder zwischen RecordTypeId und ChangeOperation
// (siehe Envelope-Feldreihenfolge). Ausgelagert, um `decodeEnvelope()` klein
// und die kognitive Komplexitaet gering zu halten.
struct CoreFieldsResult {
    EnvelopeDecodeStatus status{EnvelopeDecodeStatus::LengthMismatch};
    uint16_t recordTypeRaw{0U};
    uint32_t schemaVersion{0U};
    uint64_t storageEpochRaw{0U};
    uint64_t versionValue{0U};
    uint32_t payloadLength{0U};
    uint16_t changeOriginWireId{0U};
    uint16_t changeOperationWireId{0U};
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
    if (!big_endian::readUint32(reader, result.payloadLength) ||
        !big_endian::readUint16(reader, result.changeOriginWireId) ||
        !big_endian::readUint16(reader, result.changeOperationWireId)) {
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
    ok = ok && big_endian::writeUint16(header, envelope.changeOriginWireId);
    ok = ok && big_endian::writeUint16(header, envelope.changeOperationWireId);
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
    // also nie `false` liefern.
    Crc32IsoHdlc crcAccumulator;
    static_cast<void>(crcAccumulator.update(header.bytes()));
    static_cast<void>(crcAccumulator.update(envelope.payload));
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
    if (bytes.size() < kHeaderBeforeCrcWithoutUtc + kCrcSize) {
        return {EnvelopeDecodeStatus::LengthMismatch, std::nullopt};
    }

    ByteReader reader(bytes);
    char magic[sizeof(kMagic)];
    if (!reader.readBytes(magic, sizeof(magic))) {
        return {EnvelopeDecodeStatus::LengthMismatch, std::nullopt};
    }
    if (std::memcmp(magic, kMagic, sizeof(kMagic)) != 0) {
        return {EnvelopeDecodeStatus::InvalidMagic, std::nullopt};
    }

    uint16_t envelopeVersion = 0U;
    if (!big_endian::readUint16(reader, envelopeVersion)) {
        return {EnvelopeDecodeStatus::LengthMismatch, std::nullopt};
    }
    if (envelopeVersion != kStorageEnvelopeVersion1) {
        return {EnvelopeDecodeStatus::UnknownEnvelopeVersion, std::nullopt};
    }

    const auto core = readCoreFields(reader);
    if (core.status != EnvelopeDecodeStatus::Success) {
        return {core.status, std::nullopt};
    }
    const auto utc = readUtcField(reader);
    if (utc.status != EnvelopeDecodeStatus::Success) {
        return {utc.status, std::nullopt};
    }

    // Position hier ist exakt die Headerlaenge vor dem CRC-Feld.
    const std::size_t headerBeforeCrcLen = reader.position();

    if (reader.remaining() < kCrcSize) {
        return {EnvelopeDecodeStatus::LengthMismatch, std::nullopt};
    }
    if (reader.remaining() - kCrcSize != core.payloadLength) {
        return {EnvelopeDecodeStatus::LengthMismatch, std::nullopt};
    }

    uint32_t storedCrc = 0U;
    if (!big_endian::readUint32(reader, storedCrc)) {
        return {EnvelopeDecodeStatus::LengthMismatch, std::nullopt};
    }

    std::string payload(core.payloadLength, '\0');
    if (core.payloadLength > 0U &&
        !reader.readBytes(payload.data(), core.payloadLength)) {
        return {EnvelopeDecodeStatus::LengthMismatch, std::nullopt};
    }

    // CRC direkt ueber den Headerausschnitt der vorhandenen Eingabe und die
    // bereits materialisierte Payload, ohne einen zusaetzlichen
    // `header + payload`-Hilfspuffer anzulegen (kein `substr`-Vollkopie;
    // siehe docs/CONFIGURATION_PERSISTENCE.md, Abschnitt
    // "Ressourcenvertrag"). `bytes.data()` ist hier nie `nullptr`
    // (`bytes.size() >= kHeaderBeforeCrcWithoutUtc + kCrcSize` bereits oben
    // geprueft), `headerBeforeCrcLen` ist immer > 0 - beide Aufrufe koennen
    // also nie `false` liefern.
    Crc32IsoHdlc crcAccumulator;
    static_cast<void>(crcAccumulator.update(bytes.data(), headerBeforeCrcLen));
    static_cast<void>(crcAccumulator.update(payload));
    if (crcAccumulator.finalize() != storedCrc) {
        return {EnvelopeDecodeStatus::CrcMismatch, std::nullopt};
    }

    StorageEnvelope result;
    result.recordTypeId = RecordTypeId(core.recordTypeRaw);
    result.schemaVersion = core.schemaVersion;
    result.storageEpoch = StorageEpoch(core.storageEpochRaw);
    result.versionValue = core.versionValue;
    result.changeOriginWireId = core.changeOriginWireId;
    result.changeOperationWireId = core.changeOperationWireId;
    result.utcUnixSeconds = utc.utcUnixSeconds;
    result.payload = std::move(payload);
    return {EnvelopeDecodeStatus::Success, std::move(result)};
}

}  // namespace device_platform
