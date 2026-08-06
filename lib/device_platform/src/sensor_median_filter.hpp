#pragma once

#include <array>
#include <cstddef>
#include <optional>

#include "sensor_limits.hpp"

namespace device_platform {

// Fester Ringpuffer fuer rohe, bereits plausible Temperaturwerte. Die
// Kapazitaet stammt aus der validierten Pipelinekonfiguration; eine ungueltige
// Konstruktion bleibt bewusst leer statt eine Kapazitaet zu erfinden.
class MedianFilter {
   public:
    explicit MedianFilter(std::size_t capacity) noexcept;

    [[nodiscard]] bool add(double value) noexcept;
    [[nodiscard]] std::optional<double> median() const noexcept;
    void reset() noexcept;

    [[nodiscard]] std::size_t size() const noexcept { return size_; }

   private:
    std::array<double, sensor_limits::kMaxMedianWindowSize> values_{};
    std::size_t capacity_{0U};
    std::size_t size_{0U};
    std::size_t nextIndex_{0U};
};

}  // namespace device_platform
