#pragma once

#include <cstdint>
#include <optional>

namespace device_platform {

struct ServiceSessionPolicy {
    std::uint64_t inactivityTimeoutMillis{0U};
    std::optional<std::uint64_t> absoluteTimeoutMillis;

    [[nodiscard]] bool valid() const noexcept {
        return inactivityTimeoutMillis != 0U &&
               (!absoluteTimeoutMillis.has_value() ||
                (*absoluteTimeoutMillis >= inactivityTimeoutMillis &&
                 *absoluteTimeoutMillis != 0U));
    }
};

enum class ServiceSessionEvent : std::uint8_t {
    RelevantUserActivity,
    DeviceRestart,
    ExplicitSignOut,
    SafetyStateInvalidated,
};

class ServiceSessionLease {
   public:
    ServiceSessionLease() = default;
    ServiceSessionLease(ServiceSessionPolicy policy,
                        std::uint64_t grantedAtMillis) noexcept
        : policy_(policy),
          grantedAtMillis_(grantedAtMillis),
          lastActivityAtMillis_(grantedAtMillis),
          active_(policy.valid()) {}

    [[nodiscard]] bool activeAt(std::uint64_t nowMillis) const noexcept {
        return active_ && !expired(nowMillis);
    }
    [[nodiscard]] bool expired(std::uint64_t nowMillis) const noexcept {
        if (!active_ || nowMillis < grantedAtMillis_ ||
            nowMillis < lastActivityAtMillis_) {
            return true;
        }
        if (nowMillis - lastActivityAtMillis_ >=
            policy_.inactivityTimeoutMillis) {
            return true;
        }
        return policy_.absoluteTimeoutMillis.has_value() &&
               nowMillis - grantedAtMillis_ >= *policy_.absoluteTimeoutMillis;
    }
    // This read-only view deliberately does not count as relevant activity.
    // A UI can display the remaining authorization without extending it.
    [[nodiscard]] std::optional<std::uint64_t> remainingAt(
        std::uint64_t nowMillis) const noexcept {
        if (!activeAt(nowMillis)) {
            return std::nullopt;
        }
        const auto idleRemaining = policy_.inactivityTimeoutMillis -
                                   (nowMillis - lastActivityAtMillis_);
        if (!policy_.absoluteTimeoutMillis.has_value()) {
            return idleRemaining;
        }
        const auto absoluteRemaining =
            *policy_.absoluteTimeoutMillis - (nowMillis - grantedAtMillis_);
        return idleRemaining < absoluteRemaining ? idleRemaining
                                                 : absoluteRemaining;
    }
    void observe(ServiceSessionEvent event, std::uint64_t nowMillis) noexcept {
        if (!active_) {
            return;
        }
        if (event == ServiceSessionEvent::RelevantUserActivity) {
            // Expiry is terminal.  Activity delivered at or after the
            // inactivity/absolute boundary must never resurrect a lease.
            if (expired(nowMillis)) {
                active_ = false;
                return;
            }
            if (nowMillis < lastActivityAtMillis_) {
                active_ = false;
                return;
            }
            lastActivityAtMillis_ = nowMillis;
            return;
        }
        if (event != ServiceSessionEvent::RelevantUserActivity) {
            active_ = false;
        }
    }

   private:
    ServiceSessionPolicy policy_;
    std::uint64_t grantedAtMillis_{0U};
    std::uint64_t lastActivityAtMillis_{0U};
    bool active_{false};
};

}  // namespace device_platform
