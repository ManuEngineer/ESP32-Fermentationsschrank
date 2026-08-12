#pragma once

#include <cstdint>
#include <optional>

#include "process_state_machine.hpp"
#include "temperature_control_types.hpp"

namespace fermentation {

struct EffectiveControlContextInput {
    ProcessState phase{ProcessState::Boot};
    std::optional<RunSensorMode> activeRunSensorMode;
    std::optional<double> effectiveFermentationTargetCelsius;
    std::optional<double> completionCoolingTargetCelsius;
    std::optional<double> manualTargetCelsius;
    CompletionMode completionMode{CompletionMode::FinishWithoutCooling};
    std::uint32_t processTransitionSequence{0U};
    std::uint32_t runRevision{0U};
    bool manualRun{false};
};

struct EffectiveControlContext {
    ProcessState phase{ProcessState::Boot};
    ControlSensorRole controlSensorRole{ControlSensorRole::Air};
    EffectiveControlTarget target;
    ControlRequestContext requestContext;
    bool valid{false};
};

// Loest nur den bereits kanonisch aufgeloesten Lauf-/Prozesskontext auf. Die
// Funktion waehlt weder Sensoren aus noch validiert sie Sensorqualitaet oder
// Safety; diese Verantwortungen bleiben bei #21 beziehungsweise #24.
[[nodiscard]] EffectiveControlContext resolveEffectiveControlContext(
    const EffectiveControlContextInput& input);

}  // namespace fermentation
