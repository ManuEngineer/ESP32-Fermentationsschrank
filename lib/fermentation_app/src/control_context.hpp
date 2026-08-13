#pragma once

#include <cstdint>
#include <optional>

#include "process_state_machine.hpp"
#include "temperature_control_types.hpp"

namespace fermentation {

struct RunCommandState;

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

// Canonical projection from the live command state. Callers must not rebuild
// effective targets or roles from source-program values independently.
[[nodiscard]] std::optional<EffectiveControlContextInput>
projectEffectiveControlContextInput(const RunCommandState& current);

[[nodiscard]] EffectiveControlContext resolveEffectiveControlContext(
    const RunCommandState& current);

[[nodiscard]] bool isTemperatureControlledProcessState(ProcessState phase);

// The process/persistence commit path consumes only this already-resolved
// role identity. It does not select sensors or infer a role from
// ProductInserted.
[[nodiscard]] std::optional<ControlSensorRole>
resolveEffectiveControlSensorRole(
    ProcessState phase,
    const std::optional<RunSensorMode>& activeRunSensorMode);

[[nodiscard]] std::optional<CommittedControlContextTransition>
resolveProductInsertedControlContextTransition(
    const std::optional<ControlSensorRole>& before,
    const std::optional<ControlSensorRole>& after);

}  // namespace fermentation
