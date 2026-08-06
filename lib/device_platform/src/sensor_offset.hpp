#pragma once

#include <cmath>
#include <cstdint>
#include <optional>

#include "sensor_limits.hpp"

namespace device_platform {

enum class SensorOffsetStatus : uint8_t {
    Success,
    NonFinite,
    OutOfFirmwareRange,
};

struct SensorOffsetCreateResult;

// Ein explizit erzeugter, endlicher Kalibrier-Offset. Das Fehlen einer
// Kalibrierung wird ausserhalb dieses Werttyps durch std::optional dargestellt
// und niemals durch einen impliziten Null-Offset.
class SensorOffset {
   public:
    [[nodiscard]] static SensorOffsetCreateResult create(double celsius);

    [[nodiscard]] double celsius() const { return celsius_; }

   private:
    explicit SensorOffset(double celsius) : celsius_(celsius) {}

    double celsius_;
};

struct SensorOffsetCreateResult {
    SensorOffsetStatus status{SensorOffsetStatus::NonFinite};
    std::optional<SensorOffset> offset;
};

inline SensorOffsetCreateResult SensorOffset::create(double celsius) {
    if (!std::isfinite(celsius)) {
        return SensorOffsetCreateResult{SensorOffsetStatus::NonFinite,
                                        std::nullopt};
    }
    if (std::fabs(celsius) > sensor_limits::kMaxAbsoluteOffsetCelsius) {
        return SensorOffsetCreateResult{SensorOffsetStatus::OutOfFirmwareRange,
                                        std::nullopt};
    }
    return SensorOffsetCreateResult{SensorOffsetStatus::Success,
                                    SensorOffset(celsius)};
}

}  // namespace device_platform
