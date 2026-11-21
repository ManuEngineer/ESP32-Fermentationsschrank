#pragma once

#include <optional>
#include <string_view>

#include "actuator_plan_types.hpp"
#include "thermal_safety_supervisor.hpp"

namespace fermentation {

// Console command grammar of the R1 control-trial harness (R1 trial plan S5).
// Pure parsing, no side effects; the harness executes the result. The
// grammar is the only way to set trial planner parameters and the firmware
// temperature safety limits at runtime (the owner decides every number at the
// gates G-P1/G-P2; nothing is hard-coded here):
//
//   trial status
//   planner show | planner clear
//   planner set <window_ms> <min_on_ms> <min_off_ms> <polarity_dead_ms>
//               <pulse_cap_ms> <counter_quote> <counter_ms> <watchdog_ms>
//               <outer_postrun_ms> <inner_postrun_ms>
//   thermal show | thermal clear
//   thermal set <air_emerg_lo> <air_int_lo> <air_int_up> <air_emerg_up>
//               <cool_emerg_lo> <cool_int_lo> <cool_int_up> <cool_emerg_up>
//               <cool_rise_c_per_min> <trend_window_ms> <max_age_ms>
//               <invalid_latch_ms>
enum class R1TrialCommandKind : std::uint8_t {
    Rejected,
    Status,
    PlannerShow,
    PlannerClear,
    PlannerSet,
    ThermalShow,
    ThermalClear,
    ThermalSet,
};

struct R1TrialCommand {
    R1TrialCommandKind kind{R1TrialCommandKind::Rejected};
    ActuatorPlannerParameters planner;
    ThermalSafetyLimits thermal;
};

// Strict: exact token count, finite decimal numbers (no hex, no trailing
// garbage, no NaN/Inf), unsigned integers only for millisecond fields. Value
// validity (ordering, bounds) is decided by the owners of the values.
[[nodiscard]] R1TrialCommand parseR1TrialCommand(std::string_view line);

}  // namespace fermentation
