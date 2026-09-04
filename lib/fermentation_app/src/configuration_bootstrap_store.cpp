#include "configuration_bootstrap_store.hpp"

#include <array>
#include <limits>
#include <utility>

#include "configuration_bootstrap_codec.hpp"
#include "configuration_limits.hpp"
#include "configuration_storage_contract.hpp"
#include "state_store_key.hpp"

namespace fermentation {
namespace {

using device_platform::StateStoreReadStatus;

device_platform::StateStoreKey keyFor(std::size_t slot) {
    auto created = device_platform::StateStoreKey::create(
        configuration_storage_contract::kConfigurationBootstrapSlotKeys[slot]);
    // The indexed values come from the compile-time validated storage contract.
    // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
    return std::move(*created.key);
}

ConfigurationBootstrapScanStatus mapCodec(
    ConfigurationBootstrapCodecStatus status) {
    return status == ConfigurationBootstrapCodecStatus::UnsupportedNewerSchema
               ? ConfigurationBootstrapScanStatus::UnsupportedNewerSchema
               : ConfigurationBootstrapScanStatus::IntegrityFailure;
}

ConfigurationBootstrapWriteStatus mapScan(
    ConfigurationBootstrapScanStatus status) {
    switch (status) {
        case ConfigurationBootstrapScanStatus::ReadError:
            return ConfigurationBootstrapWriteStatus::ReadError;
        case ConfigurationBootstrapScanStatus::CapacityError:
            return ConfigurationBootstrapWriteStatus::CapacityError;
        case ConfigurationBootstrapScanStatus::UnsupportedNewerSchema:
            return ConfigurationBootstrapWriteStatus::UnsupportedNewerSchema;
        case ConfigurationBootstrapScanStatus::IntegrityFailure:
            return ConfigurationBootstrapWriteStatus::IntegrityFailure;
        case ConfigurationBootstrapScanStatus::Empty:
        case ConfigurationBootstrapScanStatus::Available:
            break;
    }
    return ConfigurationBootstrapWriteStatus::IntegrityFailure;
}

}  // namespace

// The complete two-slot history relation is intentionally evaluated together.
// NOLINTNEXTLINE(readability-function-cognitive-complexity)
ConfigurationBootstrapScanResult ConfigurationBootstrapStore::scan() const {
    struct Candidate {
        std::size_t slot;
        ConfigurationBootstrapRecord record;
        std::string bytes;
    };
    std::array<std::optional<Candidate>, 2> candidates;
    for (std::size_t slot = 0U; slot < candidates.size(); ++slot) {
        auto read = store_.read(
            keyFor(slot),
            configuration_limits::kMaximumConfigurationBootstrapEnvelopeBytes);
        if (read.status == StateStoreReadStatus::NotFound) {
            continue;
        }
        if (read.status == StateStoreReadStatus::ReadError) {
            return {ConfigurationBootstrapScanStatus::ReadError, std::nullopt};
        }
        if (read.status == StateStoreReadStatus::CapacityError) {
            return {ConfigurationBootstrapScanStatus::CapacityError,
                    std::nullopt};
        }
        auto decoded = decodeConfigurationBootstrapRecord(read.value);
        if (decoded.status != ConfigurationBootstrapCodecStatus::Success ||
            !decoded.value.has_value()) {
            return {mapCodec(decoded.status), std::nullopt};
        }
        candidates[slot] =
            Candidate{slot, *decoded.value, std::move(read.value)};
    }
    if (!candidates[0].has_value() && !candidates[1].has_value()) {
        return {ConfigurationBootstrapScanStatus::Empty, std::nullopt};
    }
    if (candidates[0].has_value() && candidates[1].has_value()) {
        // Both optionals are proven present by the enclosing condition.
        // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
        const auto& left = *candidates[0];
        // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
        const auto& right = *candidates[1];
        if (left.record.sequence == right.record.sequence) {
            if (left.bytes != right.bytes) {
                return {ConfigurationBootstrapScanStatus::IntegrityFailure,
                        std::nullopt};
            }
            return {ConfigurationBootstrapScanStatus::Available,
                    LoadedConfigurationBootstrap{device_platform::SlotId{0U},
                                                 left.record, left.bytes,
                                                 left.record.sequence, true}};
        }
        const auto& older =
            left.record.sequence < right.record.sequence ? left : right;
        const auto& newer =
            left.record.sequence < right.record.sequence ? right : left;
        if (!isAllowedBootstrapSuccessor(older.record, newer.record)) {
            return {ConfigurationBootstrapScanStatus::IntegrityFailure,
                    std::nullopt};
        }
        return {
            ConfigurationBootstrapScanStatus::Available,
            LoadedConfigurationBootstrap{
                device_platform::SlotId{static_cast<std::uint32_t>(newer.slot)},
                newer.record, newer.bytes, newer.record.sequence, false}};
    }
    // The preceding empty and both-present branches prove exactly one value.
    // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
    const auto& only =
        candidates[0].has_value()
            ? *candidates[0]   // NOLINT(bugprone-unchecked-optional-access)
            : *candidates[1];  // NOLINT(bugprone-unchecked-optional-access)
    return {ConfigurationBootstrapScanStatus::Available,
            LoadedConfigurationBootstrap{
                device_platform::SlotId{static_cast<std::uint32_t>(only.slot)},
                only.record, only.bytes, only.record.sequence, false}};
}

ConfigurationBootstrapWriteResult
ConfigurationBootstrapStore::writeInitialInitializing() {
    const auto current = scan();
    if (current.status != ConfigurationBootstrapScanStatus::Empty) {
        return {current.status == ConfigurationBootstrapScanStatus::Available
                    ? ConfigurationBootstrapWriteStatus::InvalidTransition
                    : mapScan(current.status),
                std::nullopt};
    }
    return writeBound(current, {ConfigurationBootstrapSequence{1U},
                                kConfigurationStorageFormatVersion1,
                                device_platform::StorageEpoch{1U},
                                ConfigurationBootstrapState::Initializing});
}

ConfigurationBootstrapWriteResult
ConfigurationBootstrapStore::writeInitialInitializing(
    const FactoryNoveltyProof& proof,
    const ConfigurationMutationLease& mutationLease,
    std::uint64_t serviceStateRevision, std::uint64_t recoveryGeneration) {
    if (!proof.consumeForBootstrapWrite(
            store_, mutationLease, serviceStateRevision, recoveryGeneration)) {
        return writeInitialInitializing();
    }
    return writeBound(
        {ConfigurationBootstrapScanStatus::Empty, std::nullopt},
        {ConfigurationBootstrapSequence{1U},
         kConfigurationStorageFormatVersion1, device_platform::StorageEpoch{1U},
         ConfigurationBootstrapState::Initializing});
}

ConfigurationBootstrapWriteResult ConfigurationBootstrapStore::writeSuccessor(
    const LoadedConfigurationBootstrap& expected,
    ConfigurationBootstrapState targetState) {
    const auto targetHandoff =
        expected.record.state == ConfigurationBootstrapState::Resetting &&
                targetState == ConfigurationBootstrapState::Initialized
            ? RunEpochHandoffState::Pending
            : RunEpochHandoffState::None;
    return writeSuccessorWithHandoff(expected, targetState, targetHandoff);
}

ConfigurationBootstrapWriteResult
ConfigurationBootstrapStore::writeHandoffSuccessor(
    const LoadedConfigurationBootstrap& expected,
    RunEpochHandoffState targetHandoff) {
    return writeSuccessorWithHandoff(expected,
                                     ConfigurationBootstrapState::Initialized,
                                     targetHandoff);
}

ConfigurationBootstrapWriteResult
ConfigurationBootstrapStore::writeSuccessorWithHandoff(
    const LoadedConfigurationBootstrap& expected,
    ConfigurationBootstrapState targetState,
    RunEpochHandoffState targetHandoff) {
    const auto current = scan();
    if (current.status != ConfigurationBootstrapScanStatus::Available ||
        !current.loaded.has_value()) {
        return {mapScan(current.status), std::nullopt};
    }
    if (current.loaded->record != expected.record ||
        current.loaded->canonicalRecordBytes != expected.canonicalRecordBytes ||
        current.loaded->slot != expected.slot) {
        return {ConfigurationBootstrapWriteStatus::InvalidTransition,
                std::nullopt};
    }
    if (expected.record.sequence.value() ==
        std::numeric_limits<std::uint64_t>::max()) {
        return {ConfigurationBootstrapWriteStatus::CounterOverflow,
                std::nullopt};
    }
    if (targetHandoff != RunEpochHandoffState::None &&
        targetState != ConfigurationBootstrapState::Initialized) {
        return {ConfigurationBootstrapWriteStatus::InvalidTransition,
                std::nullopt};
    }
    auto epoch = expected.record.storageEpoch;
    if (targetState == ConfigurationBootstrapState::Resetting) {
        if (epoch.value() == std::numeric_limits<std::uint64_t>::max()) {
            return {ConfigurationBootstrapWriteStatus::CounterOverflow,
                    std::nullopt};
        }
        epoch = device_platform::StorageEpoch{epoch.value() + 1U};
    }
    std::optional<device_platform::StorageEpoch> previousEpoch;
    std::optional<device_platform::StorageEpoch> currentEpoch;
    if (targetHandoff != RunEpochHandoffState::None) {
        if (expected.record.previousEpoch.has_value() &&
            expected.record.currentEpoch.has_value()) {
            previousEpoch = expected.record.previousEpoch;
            currentEpoch = expected.record.currentEpoch;
        } else if (epoch.value() > 1U) {
            previousEpoch = device_platform::StorageEpoch{epoch.value() - 1U};
            currentEpoch = epoch;
        } else {
            return {ConfigurationBootstrapWriteStatus::InvalidTransition,
                    std::nullopt};
        }
    }
    const ConfigurationBootstrapRecord target{
        ConfigurationBootstrapSequence{expected.record.sequence.value() + 1U},
        expected.record.storageFormatVersion, epoch, targetState,
        kConfigurationBootstrapSchemaVersion2, targetHandoff, previousEpoch,
        currentEpoch};
    if (!isAllowedBootstrapSuccessor(expected.record, target)) {
        return {ConfigurationBootstrapWriteStatus::InvalidTransition,
                std::nullopt};
    }
    return writeBound(current, target);
}

ConfigurationBootstrapWriteResult ConfigurationBootstrapStore::writeBound(
    const ConfigurationBootstrapScanResult& scanResult,
    const ConfigurationBootstrapRecord& target) {
    std::size_t targetSlot = 0U;
    std::optional<std::string> previous;
    if (scanResult.loaded.has_value()) {
        targetSlot = scanResult.loaded->slot.value() == 0U ? 1U : 0U;
        const auto prior = store_.read(
            keyFor(targetSlot),
            configuration_limits::kMaximumConfigurationBootstrapEnvelopeBytes);
        if (prior.status == StateStoreReadStatus::Success) {
            previous = prior.value;
        } else if (prior.status == StateStoreReadStatus::ReadError) {
            return {ConfigurationBootstrapWriteStatus::ReadError, std::nullopt};
        } else if (prior.status == StateStoreReadStatus::CapacityError) {
            return {ConfigurationBootstrapWriteStatus::CapacityError,
                    std::nullopt};
        }
    }
    std::string encoded;
    if (encodeConfigurationBootstrapRecord(target, encoded) !=
        ConfigurationBootstrapCodecStatus::Success) {
        return {ConfigurationBootstrapWriteStatus::IntegrityFailure,
                std::nullopt};
    }
    const auto write = store_.write(keyFor(targetSlot), encoded);
    if (write == device_platform::StateStoreWriteStatus::WriteError) {
        return {ConfigurationBootstrapWriteStatus::WriteError, std::nullopt};
    }
    if (write == device_platform::StateStoreWriteStatus::CapacityError) {
        return {ConfigurationBootstrapWriteStatus::WriteCapacityError,
                std::nullopt};
    }
    const auto readback = store_.read(
        keyFor(targetSlot),
        configuration_limits::kMaximumConfigurationBootstrapEnvelopeBytes);
    if (readback.status == StateStoreReadStatus::ReadError) {
        return {ConfigurationBootstrapWriteStatus::BootstrapCommitIndeterminate,
                std::nullopt};
    }
    if (readback.status == StateStoreReadStatus::CapacityError) {
        return {ConfigurationBootstrapWriteStatus::BootstrapCommitIndeterminate,
                std::nullopt};
    }
    if (write == device_platform::StateStoreWriteStatus::Success &&
        (readback.status != StateStoreReadStatus::Success ||
         readback.value != encoded)) {
        return {ConfigurationBootstrapWriteStatus::IntegrityFailure,
                std::nullopt};
    }
    const bool priorConfirmed =
        (readback.status == StateStoreReadStatus::NotFound &&
         !previous.has_value()) ||
        (readback.status == StateStoreReadStatus::Success &&
         previous.has_value() && readback.value == *previous);
    if (priorConfirmed) {
        return {
            write ==
                    device_platform::StateStoreWriteStatus::CommitOutcomeUnknown
                ? ConfigurationBootstrapWriteStatus::CommitNotEffective
                : ConfigurationBootstrapWriteStatus::IntegrityFailure,
            std::nullopt};
    }
    if (readback.value != encoded) {
        return {ConfigurationBootstrapWriteStatus::BootstrapCommitIndeterminate,
                std::nullopt};
    }
    auto rescanned = scan();
    if (rescanned.status != ConfigurationBootstrapScanStatus::Available ||
        !rescanned.loaded.has_value() || rescanned.loaded->record != target) {
        return {ConfigurationBootstrapWriteStatus::IntegrityFailure,
                std::nullopt};
    }
    return {ConfigurationBootstrapWriteStatus::Success,
            std::move(rescanned.loaded)};
}

}  // namespace fermentation
