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

// Non-copyable, non-movable proof that the exactly 19 known configuration
// and bootstrap keys were already read as NotFound under the current
// mutation lease and service revision, in this single recovery attempt. Its
// only purpose is to let the first-ever factory initialization skip the
// otherwise redundant re-scan of slots already proven empty by that oracle;
// it never carries or reuses any read payload.
class FactoryNoveltyProof {
   public:
    FactoryNoveltyProof(const FactoryNoveltyProof&) = delete;
    FactoryNoveltyProof& operator=(const FactoryNoveltyProof&) = delete;
    FactoryNoveltyProof(FactoryNoveltyProof&&) = delete;
    FactoryNoveltyProof& operator=(FactoryNoveltyProof&&) = delete;
    ~FactoryNoveltyProof() = default;

   private:
    friend class ConfigurationRecoveryService;
    FactoryNoveltyProof() = default;
};

}  // namespace fermentation
