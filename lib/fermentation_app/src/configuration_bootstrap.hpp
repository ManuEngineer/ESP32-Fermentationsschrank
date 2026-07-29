#pragma once

#include <cstdint>

#include "storage_types.hpp"

namespace fermentation {

namespace detail {
struct ConfigurationBootstrapSequenceTag {};
struct ConfigurationStorageFormatVersionTag {};
}  // namespace detail

using ConfigurationBootstrapSequence =
    device_platform::StrongId<detail::ConfigurationBootstrapSequenceTag,
                              std::uint64_t>;
using ConfigurationStorageFormatVersion =
    device_platform::StrongId<detail::ConfigurationStorageFormatVersionTag,
                              std::uint32_t>;

enum class ConfigurationBootstrapState : std::uint8_t {
    Initializing = 1U,
    Initialized = 2U,
    Resetting = 3U,
};

struct ConfigurationBootstrapRecord {
    ConfigurationBootstrapSequence sequence;
    ConfigurationStorageFormatVersion storageFormatVersion;
    device_platform::StorageEpoch storageEpoch;
    ConfigurationBootstrapState state{
        ConfigurationBootstrapState::Initializing};
};

inline constexpr ConfigurationStorageFormatVersion
    kConfigurationStorageFormatVersion1{1U};

[[nodiscard]] bool operator==(const ConfigurationBootstrapRecord& left,
                              const ConfigurationBootstrapRecord& right);
[[nodiscard]] inline bool operator!=(
    const ConfigurationBootstrapRecord& left,
    const ConfigurationBootstrapRecord& right) {
    return !(left == right);
}
[[nodiscard]] bool isPlausible(const ConfigurationBootstrapRecord& record);
[[nodiscard]] bool isAllowedBootstrapSuccessor(
    const ConfigurationBootstrapRecord& previous,
    const ConfigurationBootstrapRecord& next);

}  // namespace fermentation
