#pragma once

#include <cstdint>
#include <mutex>
#include <optional>

#include "time_source.hpp"

namespace device_platform_esp_idf {

// ESP-IDF-Adapter fuer device_platform::ITimeSource, gestuetzt auf
// esp_timer_get_time(). monotonicMillis() zaehlt bewusst seit Erstellung
// dieser Instanz (nicht seit Boot), damit derselbe Vertrag wie
// VirtualTimeSource gilt (siehe docs/tasks/issue-73-implementation-plan.md,
// Abschnitt 15).
class EspTimerTimeSource final : public device_platform::ITimeSource {
   public:
    EspTimerTimeSource();

    [[nodiscard]] uint64_t monotonicMillis() const override;
    [[nodiscard]] std::optional<int64_t> unixTimeSeconds() const override;

    // Sets the ESP system clock without making it visible through the port.
    // The caller establishes trust only after the source-specific validation
    // has completed.
    [[nodiscard]] static bool setSystemTimeUtc(int64_t utcUnixSeconds) noexcept;

    // Latches that the ESP system clock has been established by a validated
    // RTC seed or a completed SNTP synchronization.  Source reachability is
    // deliberately not part of this latch: the local system clock continues
    // to be the current boot's source after a later connectivity loss.
    [[nodiscard]] bool markAbsoluteTimeTrusted() const noexcept;

   private:
    int64_t baselineMicros_;
    mutable std::mutex trustMutex_;
    mutable bool absoluteTimeTrusted_{false};
    mutable std::optional<int64_t> lastPublishedTrustedUtc_;
};

}  // namespace device_platform_esp_idf
