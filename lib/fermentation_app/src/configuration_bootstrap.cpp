#include "configuration_bootstrap.hpp"

#include <limits>

namespace fermentation {
namespace {

bool checkedTwice(std::uint64_t epoch, std::uint64_t& out) {
    if (epoch > std::numeric_limits<std::uint64_t>::max() / 2U) {
        return false;
    }
    out = epoch * 2U;
    return true;
}

}  // namespace

bool operator==(const ConfigurationBootstrapRecord& left,
                const ConfigurationBootstrapRecord& right) {
    return left.sequence == right.sequence &&
           left.storageFormatVersion == right.storageFormatVersion &&
           left.storageEpoch == right.storageEpoch && left.state == right.state;
}

bool isPlausible(const ConfigurationBootstrapRecord& record) {
    if (record.sequence.value() == 0U || record.storageEpoch.value() == 0U ||
        record.storageFormatVersion != kConfigurationStorageFormatVersion1) {
        return false;
    }
    std::uint64_t twiceEpoch = 0U;
    if (!checkedTwice(record.storageEpoch.value(), twiceEpoch)) {
        return false;
    }
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

bool isAllowedBootstrapSuccessor(const ConfigurationBootstrapRecord& previous,
                                 const ConfigurationBootstrapRecord& next) {
    if (!isPlausible(previous) || !isPlausible(next) ||
        next.sequence.value() != previous.sequence.value() + 1U) {
        return false;
    }
    if (previous.state == ConfigurationBootstrapState::Initializing) {
        return next.state == ConfigurationBootstrapState::Initialized &&
               next.storageEpoch == previous.storageEpoch;
    }
    if (previous.state == ConfigurationBootstrapState::Initialized) {
        return next.state == ConfigurationBootstrapState::Resetting &&
               next.storageEpoch.value() == previous.storageEpoch.value() + 1U;
    }
    return previous.state == ConfigurationBootstrapState::Resetting &&
           next.state == ConfigurationBootstrapState::Initialized &&
           next.storageEpoch == previous.storageEpoch;
}

}  // namespace fermentation
