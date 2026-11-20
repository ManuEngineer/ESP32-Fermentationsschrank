#pragma once

#if defined(APP_R1_CONTROL_TRIAL)

#include <array>
#include <cstddef>
#include <cstdint>

#include "control_sensor_evidence.hpp"
#include "fermentation_application.hpp"
#include "time_source.hpp"

namespace fermentation::r1_trial {

// Bring-up-only console of the R1 control trial (R1 trial plan S5). It only
// configures values through the existing owners (planner-parameter writer,
// firmware temperature safety) and reports status; it never commands an
// actuator and never releases anything. The release profile contains neither
// this file nor a caller.
class Harness {
   public:
    Harness(FermentationApplication& application,
            const ControlSensorEvidenceProducer& producer,
            const device_platform::ITimeSource& time) noexcept
        : application_(application), producer_(producer), time_(time) {}

    void start() noexcept;
    void update() noexcept;

   private:
    void processLine() noexcept;
    void logStatus() noexcept;
    void showPlanner() noexcept;
    void showThermal() noexcept;

    FermentationApplication& application_;
    const ControlSensorEvidenceProducer& producer_;
    const device_platform::ITimeSource& time_;
    uint64_t nextStatusMs_{0U};
    std::array<char, 192U> line_{};
    std::size_t lineLength_{0U};
    bool lineOverflow_{false};
};

}  // namespace fermentation::r1_trial

#endif
