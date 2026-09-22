#include "configuration_bootstrap.hpp"

#include <limits>

namespace fermentation {
namespace {

bool checkedDouble(std::uint64_t value, std::uint64_t& out) {
    if (value > std::numeric_limits<std::uint64_t>::max() / 2U) {
        return false;
    }
    out = value * 2U;
    return true;
}

bool checkedIncrement(std::uint64_t value, std::uint64_t& out) {
    if (value == std::numeric_limits<std::uint64_t>::max()) return false;
    out = value + 1U;
    return true;
}

bool noHandoffBinding(const ConfigurationBootstrapRecord& record) {
    return record.handoff == RunEpochHandoffState::None &&
           !record.previousEpoch.has_value() &&
           !record.currentEpoch.has_value();
}

bool boundToEpoch(const ConfigurationBootstrapRecord& record) {
    if (record.handoff == RunEpochHandoffState::None ||
        !record.previousEpoch.has_value() || !record.currentEpoch.has_value() ||
        record.previousEpoch->value() == 0U ||
        record.currentEpoch->value() != record.storageEpoch.value() ||
        record.previousEpoch->value() ==
            std::numeric_limits<std::uint64_t>::max()) {
        return false;
    }
    return record.previousEpoch->value() + 1U == record.currentEpoch->value();
}

bool schema1Plausible(const ConfigurationBootstrapRecord& record) {
    if (record.sequence.value() == 0U || record.storageEpoch.value() == 0U ||
        record.storageFormatVersion != kConfigurationStorageFormatVersion1 ||
        !noHandoffBinding(record)) {
        return false;
    }
    std::uint64_t twiceEpoch = 0U;
    if (!checkedDouble(record.storageEpoch.value(), twiceEpoch)) return false;
    switch (record.state) {
        case ConfigurationBootstrapState::Initializing:
            return record.storageEpoch.value() == 1U &&
                   record.sequence.value() == 1U;
        case ConfigurationBootstrapState::Initialized:
            return record.sequence.value() == twiceEpoch;
        case ConfigurationBootstrapState::Resetting:
            return record.storageEpoch.value() >= 2U &&
                   record.sequence.value() == twiceEpoch - 1U;
    }
    return false;
}

bool schema2SequencePlausible(const ConfigurationBootstrapRecord& record,
                              std::uint64_t phaseOffset) {
    if (record.storageEpoch.value() < 2U) return false;
    std::uint64_t twiceEpoch = 0U;
    if (!checkedDouble(record.storageEpoch.value(), twiceEpoch) ||
        twiceEpoch == 0U) {
        return false;
    }
    const auto base = twiceEpoch - 1U;
    if (record.sequence.value() < base) return false;
    const auto delta = record.sequence.value() - base;
    if (delta < phaseOffset) return false;
    const auto phaseDelta = delta - phaseOffset;
    if ((phaseDelta % 2U) != 0U ||
        (phaseDelta / 2U > record.storageEpoch.value() - 2U)) {
        return false;
    }
    return true;
}

bool schema2PhasePlausible(const ConfigurationBootstrapRecord& record,
                           std::uint64_t phaseOffset) {
    return boundToEpoch(record) &&
           schema2SequencePlausible(record, phaseOffset);
}

bool schema2Plausible(const ConfigurationBootstrapRecord& record) {
    if (record.sequence.value() == 0U || record.storageEpoch.value() == 0U ||
        record.storageFormatVersion != kConfigurationStorageFormatVersion1) {
        return false;
    }
    if (record.state == ConfigurationBootstrapState::Initializing) {
        return record.storageEpoch.value() == 1U &&
               record.sequence.value() == 1U && noHandoffBinding(record);
    }
    if (record.state == ConfigurationBootstrapState::Initialized &&
        record.handoff == RunEpochHandoffState::None) {
        return record.storageEpoch.value() == 1U &&
               record.sequence.value() == 2U && noHandoffBinding(record);
    }
    if (record.state == ConfigurationBootstrapState::Resetting) {
        return record.handoff == RunEpochHandoffState::None &&
               noHandoffBinding(record) && schema2SequencePlausible(record, 0U);
    }
    if (record.state != ConfigurationBootstrapState::Initialized) {
        return false;
    }
    switch (record.handoff) {
        case RunEpochHandoffState::Pending:
            return schema2PhasePlausible(record, 1U);
        case RunEpochHandoffState::Committed:
            return schema2PhasePlausible(record, 2U);
        case RunEpochHandoffState::Consumed:
            return schema2PhasePlausible(record, 3U);
        case RunEpochHandoffState::None:
            return false;
    }
    return false;
}

bool authHandoffPlausible(AuthDomainHandoffState state);

bool schema3Plausible(const ConfigurationBootstrapRecord& record) {
    if (record.sequence.value() < 3U || record.storageEpoch.value() == 0U ||
        record.storageFormatVersion != kConfigurationStorageFormatVersion1 ||
        !authHandoffPlausible(record.authDomainHandoff)) {
        return false;
    }
    if (record.state == ConfigurationBootstrapState::Initializing) {
        return record.storageEpoch.value() == 1U &&
               record.sequence.value() == 1U && noHandoffBinding(record) &&
               record.authDomainHandoff == AuthDomainHandoffState::None;
    }
    if (record.state == ConfigurationBootstrapState::Resetting) {
        return record.handoff == RunEpochHandoffState::None &&
               noHandoffBinding(record) &&
               record.authDomainHandoff == AuthDomainHandoffState::None;
    }
    if (record.state != ConfigurationBootstrapState::Initialized) {
        return false;
    }
    if (record.handoff == RunEpochHandoffState::None) {
        return noHandoffBinding(record);
    }
    return boundToEpoch(record);
}

bool authHandoffPlausible(AuthDomainHandoffState state) {
    return state == AuthDomainHandoffState::None ||
           state == AuthDomainHandoffState::Unconsumed ||
           state == AuthDomainHandoffState::InProgress ||
           state == AuthDomainHandoffState::Consumed ||
           state == AuthDomainHandoffState::Indeterminate;
}

bool sameBinding(const ConfigurationBootstrapRecord& left,
                 const ConfigurationBootstrapRecord& right) {
    return left.previousEpoch == right.previousEpoch &&
           left.currentEpoch == right.currentEpoch &&
           left.storageEpoch == right.storageEpoch;
}

bool schema1ToSchema2Successor(const ConfigurationBootstrapRecord& previous,
                               const ConfigurationBootstrapRecord& next) {
    if (previous.state == ConfigurationBootstrapState::Initializing) {
        return next.state == ConfigurationBootstrapState::Initialized &&
               next.handoff == RunEpochHandoffState::None &&
               next.storageEpoch == previous.storageEpoch &&
               next.sequence.value() == 2U;
    }
    if (previous.state == ConfigurationBootstrapState::Initialized) {
        std::uint64_t nextEpoch = 0U;
        if (!checkedIncrement(previous.storageEpoch.value(), nextEpoch)) {
            return false;
        }
        return next.state == ConfigurationBootstrapState::Resetting &&
               next.handoff == RunEpochHandoffState::None &&
               next.storageEpoch.value() == nextEpoch;
    }
    if (previous.storageEpoch.value() < 2U) return false;
    return next.state == ConfigurationBootstrapState::Initialized &&
           next.handoff == RunEpochHandoffState::Pending &&
           next.storageEpoch == previous.storageEpoch &&
           next.previousEpoch ==
               std::optional<device_platform::StorageEpoch>{
                   device_platform::StorageEpoch{previous.storageEpoch.value() -
                                                 1U}} &&
           next.currentEpoch == next.storageEpoch;
}

bool schema2ToSchema2Successor(const ConfigurationBootstrapRecord& previous,
                               const ConfigurationBootstrapRecord& next) {
    if (previous.state == ConfigurationBootstrapState::Initializing) {
        return next.state == ConfigurationBootstrapState::Initialized &&
               next.handoff == RunEpochHandoffState::None &&
               next.storageEpoch == previous.storageEpoch;
    }
    if (previous.state == ConfigurationBootstrapState::Initialized &&
        previous.handoff == RunEpochHandoffState::None) {
        std::uint64_t nextEpoch = 0U;
        if (!checkedIncrement(previous.storageEpoch.value(), nextEpoch)) {
            return false;
        }
        return next.state == ConfigurationBootstrapState::Resetting &&
               next.handoff == RunEpochHandoffState::None &&
               next.storageEpoch.value() == nextEpoch;
    }
    if (previous.state == ConfigurationBootstrapState::Resetting) {
        return next.state == ConfigurationBootstrapState::Initialized &&
               next.handoff == RunEpochHandoffState::Pending &&
               next.storageEpoch == previous.storageEpoch &&
               next.previousEpoch ==
                   std::optional<device_platform::StorageEpoch>{
                       device_platform::StorageEpoch{
                           previous.storageEpoch.value() - 1U}} &&
               next.currentEpoch == next.storageEpoch;
    }
    if (previous.state != ConfigurationBootstrapState::Initialized ||
        !boundToEpoch(previous) || !sameBinding(previous, next)) {
        return false;
    }
    if (previous.handoff == RunEpochHandoffState::Pending) {
        return next.handoff == RunEpochHandoffState::Committed;
    }
    if (previous.handoff == RunEpochHandoffState::Committed) {
        return next.handoff == RunEpochHandoffState::Consumed;
    }
    if (previous.handoff == RunEpochHandoffState::Consumed) {
        std::uint64_t nextEpoch = 0U;
        if (!checkedIncrement(previous.storageEpoch.value(), nextEpoch)) {
            return false;
        }
        return next.state == ConfigurationBootstrapState::Resetting &&
               next.handoff == RunEpochHandoffState::None &&
               !next.previousEpoch.has_value() &&
               !next.currentEpoch.has_value() &&
               next.storageEpoch.value() == nextEpoch;
    }
    return false;
}

}  // namespace

bool operator==(const ConfigurationBootstrapRecord& left,
                const ConfigurationBootstrapRecord& right) {
    return left.sequence == right.sequence &&
           left.storageFormatVersion == right.storageFormatVersion &&
           left.storageEpoch == right.storageEpoch &&
           left.state == right.state &&
           left.schemaVersion == right.schemaVersion &&
           left.handoff == right.handoff &&
           left.previousEpoch == right.previousEpoch &&
           left.currentEpoch == right.currentEpoch &&
           left.authDomainHandoff == right.authDomainHandoff;
}

bool isPlausible(const ConfigurationBootstrapRecord& record) {
    if (record.schemaVersion == kConfigurationBootstrapSchemaVersion1) {
        return schema1Plausible(record);
    }
    if (record.schemaVersion == kConfigurationBootstrapSchemaVersion2) {
        return schema2Plausible(record);
    }
    if (record.schemaVersion == kConfigurationBootstrapSchemaVersion3) {
        return schema3Plausible(record);
    }
    return false;
}

bool isAllowedBootstrapSuccessor(const ConfigurationBootstrapRecord& previous,
                                 const ConfigurationBootstrapRecord& next) {
    std::uint64_t expectedSequence = 0U;
    if (!isPlausible(previous) || !isPlausible(next) ||
        !checkedIncrement(previous.sequence.value(), expectedSequence) ||
        next.sequence.value() != expectedSequence) {
        return false;
    }
    if (previous.schemaVersion == kConfigurationBootstrapSchemaVersion1) {
        if (next.schemaVersion == kConfigurationBootstrapSchemaVersion1) {
            if (previous.state == ConfigurationBootstrapState::Initializing) {
                return next.state == ConfigurationBootstrapState::Initialized &&
                       next.storageEpoch == previous.storageEpoch;
            }
            if (previous.state == ConfigurationBootstrapState::Initialized) {
                std::uint64_t nextEpoch = 0U;
                return checkedIncrement(previous.storageEpoch.value(),
                                        nextEpoch) &&
                       next.state == ConfigurationBootstrapState::Resetting &&
                       next.storageEpoch.value() == nextEpoch;
            }
            return previous.state == ConfigurationBootstrapState::Resetting &&
                   next.state == ConfigurationBootstrapState::Initialized &&
                   next.storageEpoch == previous.storageEpoch;
        }
        return next.schemaVersion == kConfigurationBootstrapSchemaVersion2 &&
               schema1ToSchema2Successor(previous, next);
    }
    if (previous.schemaVersion == kConfigurationBootstrapSchemaVersion2 &&
        next.schemaVersion == kConfigurationBootstrapSchemaVersion3) {
        return previous.state == ConfigurationBootstrapState::Initialized &&
               next.state == previous.state &&
               next.storageEpoch == previous.storageEpoch &&
               next.handoff == previous.handoff &&
               next.previousEpoch == previous.previousEpoch &&
               next.currentEpoch == previous.currentEpoch &&
               next.authDomainHandoff == AuthDomainHandoffState::Unconsumed;
    }
    if (previous.schemaVersion != kConfigurationBootstrapSchemaVersion2) {
        if (previous.schemaVersion != kConfigurationBootstrapSchemaVersion3 ||
            next.schemaVersion != kConfigurationBootstrapSchemaVersion3) {
            return false;
        }
        const bool sameConfiguration =
            previous.state == next.state &&
            previous.storageEpoch == next.storageEpoch &&
            previous.handoff == next.handoff &&
            previous.previousEpoch == next.previousEpoch &&
            previous.currentEpoch == next.currentEpoch;
        const bool authStep =
            sameConfiguration &&
            ((previous.authDomainHandoff == AuthDomainHandoffState::None &&
              next.authDomainHandoff == AuthDomainHandoffState::Unconsumed) ||
             (previous.authDomainHandoff ==
                  AuthDomainHandoffState::Unconsumed &&
              next.authDomainHandoff == AuthDomainHandoffState::InProgress) ||
             (previous.authDomainHandoff ==
                  AuthDomainHandoffState::InProgress &&
              (next.authDomainHandoff == AuthDomainHandoffState::Consumed ||
               next.authDomainHandoff ==
                   AuthDomainHandoffState::Indeterminate)));
        const bool configurationStep =
            [&] {
                auto previousLegacy = previous;
                auto nextLegacy = next;
                previousLegacy.schemaVersion =
                    kConfigurationBootstrapSchemaVersion2;
                nextLegacy.schemaVersion =
                    kConfigurationBootstrapSchemaVersion2;
                return schema2ToSchema2Successor(previousLegacy, nextLegacy);
            }() &&
            ((next.state == ConfigurationBootstrapState::Initialized &&
              next.authDomainHandoff == previous.authDomainHandoff) ||
             (next.state == ConfigurationBootstrapState::Resetting &&
              next.authDomainHandoff == AuthDomainHandoffState::None));
        return authStep || configurationStep;
    }
    return next.schemaVersion == kConfigurationBootstrapSchemaVersion2 &&
           schema2ToSchema2Successor(previous, next);
}

}  // namespace fermentation
