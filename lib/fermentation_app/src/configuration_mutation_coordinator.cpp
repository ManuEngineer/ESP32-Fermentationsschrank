#include "configuration_mutation_coordinator.hpp"

#include <utility>

namespace fermentation {

ConfigurationMutationLease::ConfigurationMutationLease(
    ConfigurationMutationCoordinator& owner) noexcept
    : owner_(&owner) {}

ConfigurationMutationLease::~ConfigurationMutationLease() { release(); }

ConfigurationMutationLease::ConfigurationMutationLease(
    ConfigurationMutationLease&& other) noexcept
    : owner_(std::exchange(other.owner_, nullptr)) {}

ConfigurationMutationLease& ConfigurationMutationLease::operator=(
    ConfigurationMutationLease&& other) noexcept {
    if (this != &other) {
        release();
        owner_ = std::exchange(other.owner_, nullptr);
    }
    return *this;
}

void ConfigurationMutationLease::release() noexcept {
    if (owner_ != nullptr) {
        owner_->release();
        owner_ = nullptr;
    }
}

ConfigurationMutationAcquireResult
ConfigurationMutationCoordinator::tryAcquire() noexcept {
    if (held_.test_and_set(std::memory_order_acquire)) {
        return {};
    }
    ConfigurationMutationAcquireResult result;
    result.status = ConfigurationMutationAcquireStatus::Acquired;
    result.lease = ConfigurationMutationLease(*this);
    return result;
}

void ConfigurationMutationCoordinator::release() noexcept {
    held_.clear(std::memory_order_release);
}

}  // namespace fermentation
