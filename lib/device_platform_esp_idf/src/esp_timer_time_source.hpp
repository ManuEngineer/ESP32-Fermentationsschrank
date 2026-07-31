#pragma once

#include <cstdint>

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

   private:
    int64_t baselineMicros_;
};

}  // namespace device_platform_esp_idf
