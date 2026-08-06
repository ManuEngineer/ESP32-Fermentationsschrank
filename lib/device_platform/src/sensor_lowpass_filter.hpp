#pragma once

#include <cstdint>
#include <optional>

namespace device_platform {

// Zeitbasierter Exponentialfilter. Die Zeitbasis ist der Zeitstempel der
// Probe, die tatsaechlich in den Filter eingeflossen ist.
class LowPassFilter {
   public:
    explicit LowPassFilter(double tauSeconds) noexcept;

    [[nodiscard]] std::optional<double> update(double input,
                                               uint64_t timestampMs) noexcept;
    void shiftState(double delta) noexcept;
    void reset() noexcept;

    [[nodiscard]] std::optional<double> value() const noexcept {
        return filteredValue_;
    }

   private:
    double tauSeconds_;
    std::optional<double> filteredValue_;
    std::optional<uint64_t> lastTimestampMs_;
};

}  // namespace device_platform
