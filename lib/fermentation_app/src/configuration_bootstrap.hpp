#pragma once

#include <cstdint>
#include <optional>
#include <utility>

#include "configuration_mutation_coordinator.hpp"
#include "state_store.hpp"
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

// Wire values are stable once introduced.  None is the only unbound phase;
// the other values are tied to the immediately preceding configuration reset.
enum class RunEpochHandoffState : std::uint8_t {
    None = 0U,
    Pending = 1U,
    Committed = 2U,
    Consumed = 3U,
};

inline constexpr std::uint32_t kConfigurationBootstrapSchemaVersion1 = 1U;
inline constexpr std::uint32_t kConfigurationBootstrapSchemaVersion2 = 2U;

struct ConfigurationBootstrapRecord {
    ConfigurationBootstrapSequence sequence;
    ConfigurationStorageFormatVersion storageFormatVersion;
    device_platform::StorageEpoch storageEpoch;
    ConfigurationBootstrapState state;
    // The envelope schema is retained in the decoded model so the scanner can
    // enforce the versioned successor matrix.  The default is the current
    // schema for all new in-memory records and aggregate initializers from
    // the pre-schema-2 code remain source-compatible.
    std::uint32_t schemaVersion;
    RunEpochHandoffState handoff;
    std::optional<device_platform::StorageEpoch> previousEpoch;
    std::optional<device_platform::StorageEpoch> currentEpoch;

    ConfigurationBootstrapRecord(
        ConfigurationBootstrapSequence sequenceValue,
        ConfigurationStorageFormatVersion storageFormatVersionValue,
        device_platform::StorageEpoch storageEpochValue,
        ConfigurationBootstrapState stateValue,
        std::uint32_t schemaVersionValue =
            kConfigurationBootstrapSchemaVersion2,
        RunEpochHandoffState handoffValue = RunEpochHandoffState::None,
        std::optional<device_platform::StorageEpoch> previousEpochValue =
            std::nullopt,
        std::optional<device_platform::StorageEpoch> currentEpochValue =
            std::nullopt)
        : sequence(sequenceValue),
          storageFormatVersion(storageFormatVersionValue),
          storageEpoch(storageEpochValue),
          state(stateValue),
          schemaVersion(schemaVersionValue),
          handoff(handoffValue),
          previousEpoch(std::move(previousEpochValue)),
          currentEpoch(std::move(currentEpochValue)) {}
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
// and bootstrap keys were already read as NotFound under a specific store,
// mutation lease, service revision and recovery attempt. It exists only to
// let the first-ever factory initialization skip the otherwise redundant
// re-scan of slots already proven empty by that oracle; it never carries or
// reuses any read payload.
//
// The proof does not just assert that the oracle ran once, somewhere: every
// consumer independently re-validates the exact store, lease, revision and
// attempt it was constructed for, and consumption is phase-ordered
// (graph preparation, then the bootstrap write) and single-use. A mismatch
// or out-of-order use is never a partial success -- consumeFor*() reports
// failure and the caller falls back to its normal, always-correct scanning
// path instead of skipping anything.
class FactoryNoveltyProof {
   public:
    FactoryNoveltyProof(const FactoryNoveltyProof&) = delete;
    FactoryNoveltyProof& operator=(const FactoryNoveltyProof&) = delete;
    FactoryNoveltyProof(FactoryNoveltyProof&&) = delete;
    FactoryNoveltyProof& operator=(FactoryNoveltyProof&&) = delete;
    ~FactoryNoveltyProof() = default;

    // Validates the binding for the graph-preparation step and marks it
    // used. Any mismatch (wrong store/lease/revision/attempt) or wrong
    // phase order permanently burns the proof to Failed -- there is no
    // retry with corrected parameters after a failed attempt.
    [[nodiscard]] bool consumeForGraphPreparation(
        const device_platform::IStateStore& store,
        const ConfigurationMutationLease& mutationLease,
        std::uint64_t serviceStateRevision,
        std::uint64_t recoveryGeneration) const noexcept {
        const bool ok = phase_ == Phase::Unused &&
                        matches(store, mutationLease, serviceStateRevision,
                                recoveryGeneration);
        phase_ = ok ? Phase::GraphPrepared : Phase::Failed;
        return ok;
    }

    // Validates the binding for the bootstrap-write step and marks it
    // consumed. Only succeeds immediately after a matching
    // consumeForGraphPreparation(); any mismatch or wrong phase order
    // (including calling this before graph preparation, or a second time)
    // permanently burns the proof to Failed.
    [[nodiscard]] bool consumeForBootstrapWrite(
        const device_platform::IStateStore& store,
        const ConfigurationMutationLease& mutationLease,
        std::uint64_t serviceStateRevision,
        std::uint64_t recoveryGeneration) const noexcept {
        const bool ok = phase_ == Phase::GraphPrepared &&
                        matches(store, mutationLease, serviceStateRevision,
                                recoveryGeneration);
        phase_ = ok ? Phase::Consumed : Phase::Failed;
        return ok;
    }

   private:
    friend class ConfigurationRecoveryService;
    friend class FactoryNoveltyProofTestAccess;
    enum class Phase : std::uint8_t { Unused, GraphPrepared, Consumed, Failed };

    FactoryNoveltyProof(const device_platform::IStateStore& store,
                        const ConfigurationMutationLease& mutationLease,
                        std::uint64_t serviceStateRevision,
                        std::uint64_t recoveryGeneration) noexcept
        : store_(&store),
          mutationLease_(&mutationLease),
          serviceStateRevision_(serviceStateRevision),
          recoveryGeneration_(recoveryGeneration) {}

    [[nodiscard]] bool matches(
        const device_platform::IStateStore& store,
        const ConfigurationMutationLease& mutationLease,
        std::uint64_t serviceStateRevision,
        std::uint64_t recoveryGeneration) const noexcept {
        return store_ == &store && mutationLease_ == &mutationLease &&
               mutationLease.valid() &&
               serviceStateRevision_ == serviceStateRevision &&
               recoveryGeneration_ == recoveryGeneration;
    }

    const device_platform::IStateStore* store_{nullptr};
    const ConfigurationMutationLease* mutationLease_{nullptr};
    std::uint64_t serviceStateRevision_{0U};
    std::uint64_t recoveryGeneration_{0U};
    mutable Phase phase_{Phase::Unused};
};

}  // namespace fermentation
