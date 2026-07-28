#pragma once

#include <atomic>
#include <cstdint>

namespace fermentation {

class ConfigurationMutationCoordinator;

class ConfigurationMutationLease {
   public:
    ConfigurationMutationLease() = default;
    ~ConfigurationMutationLease();

    ConfigurationMutationLease(const ConfigurationMutationLease&) = delete;
    ConfigurationMutationLease& operator=(const ConfigurationMutationLease&) =
        delete;
    ConfigurationMutationLease(ConfigurationMutationLease&& other) noexcept;
    ConfigurationMutationLease& operator=(
        ConfigurationMutationLease&& other) noexcept;

    [[nodiscard]] bool valid() const noexcept { return owner_ != nullptr; }

   private:
    friend class ConfigurationMutationCoordinator;
    explicit ConfigurationMutationLease(
        ConfigurationMutationCoordinator& owner) noexcept;
    void release() noexcept;

    ConfigurationMutationCoordinator* owner_{nullptr};
};

enum class ConfigurationMutationAcquireStatus : std::uint8_t {
    Acquired,
    ConfigurationMutationBusy,
};

struct ConfigurationMutationAcquireResult {
    ConfigurationMutationAcquireStatus status{
        ConfigurationMutationAcquireStatus::ConfigurationMutationBusy};
    ConfigurationMutationLease lease;
};

class ConfigurationMutationCoordinator {
   public:
    [[nodiscard]] ConfigurationMutationAcquireResult tryAcquire() noexcept;

   private:
    friend class ConfigurationMutationLease;
    void release() noexcept;
    std::atomic_flag held_ = ATOMIC_FLAG_INIT;
};

}  // namespace fermentation
