#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "run_persistence_contract.hpp"
#include "storage_types.hpp"

namespace fermentation {

enum class RunPersistenceCodecStatus : std::uint8_t {
    Success,
    InvalidSnapshot,
    CapacityExceeded,
    Truncated,
    TrailingBytes,
    InvalidWireValue,
    UnsupportedSchema,
};

struct RunPersistenceDecodeResult {
    RunPersistenceCodecStatus status{RunPersistenceCodecStatus::Truncated};
    std::optional<RunPersistenceSnapshot> snapshot;
};

[[nodiscard]] RunPersistenceCodecStatus encodeRunPersistenceSnapshot(
    const RunPersistenceSnapshot& snapshot, std::string& out);
// `schemaVersion` gates the #21 sensor-selection field (introduced in
// schema 2): a schema-1 payload never carries it, so schemaVersion must be
// the version the enclosing envelope actually declared, not always the
// current one.
[[nodiscard]] RunPersistenceDecodeResult decodeRunPersistenceSnapshot(
    const std::string& payload, std::uint32_t schemaVersion);

// Schema-1 record codecs are deliberately separate from the coordinator.  The
// coordinator supplies transaction ordering; these functions own only stable
// envelope/reference bytes and their validation.
[[nodiscard]] std::optional<std::string> encodeRunPersistenceHead(
    const RunPersistenceHead& head, device_platform::StorageEpoch epoch);
[[nodiscard]] std::optional<RunPersistenceHead> decodeRunPersistenceHead(
    const std::string& bytes, device_platform::StorageEpoch epoch);
[[nodiscard]] std::optional<RunPersistenceRawRecord> decodeRunPersistenceRecord(
    const std::string& bytes, device_platform::StorageEpoch epoch);
[[nodiscard]] bool runCheckpointReferenceMatches(
    const RunCheckpointReference& reference,
    const RunPersistenceRawRecord& record, std::size_t slot);
[[nodiscard]] RunCheckpointReference makeRunCheckpointReference(
    std::size_t slot, const RunPersistenceRawRecord& record,
    device_platform::StorageEpoch epoch);

}  // namespace fermentation
