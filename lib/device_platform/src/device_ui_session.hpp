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
               nowMillis - grantedAtMillis_ >=
                   *policy_.absoluteTimeoutMillis;
    }
    void observe(ServiceSessionEvent event, std::uint64_t nowMillis) noexcept {
        if (!active_) return;
        if (event == ServiceSessionEvent::RelevantUserActivity &&
            nowMillis >= lastActivityAtMillis_) {
            lastActivityAtMillis_ = nowMillis;
            return;
        }
        if (event != ServiceSessionEvent::RelevantUserActivity) active_ = false;
    }

   private:
    ServiceSessionPolicy policy_;
    std::uint64_t grantedAtMillis_{0U};
    std::uint64_t lastActivityAtMillis_{0U};
    bool active_{false};
};

}  // namespace device_platform
