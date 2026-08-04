#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "run_persistence_contract.hpp"

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
[[nodiscard]] RunPersistenceDecodeResult decodeRunPersistenceSnapshot(
    const std::string& payload);

}  // namespace fermentation
