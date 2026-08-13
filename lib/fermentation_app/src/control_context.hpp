#pragma once

#include <cstdint>
#include <optional>

#include "control_context_types.hpp"
#include "process_state_machine.hpp"
#include "sensor_selection_types.hpp"

namespace fermentation {

struct RunCommandState;

enum class ControlTargetKind : std::uint8_t {
    FermentationRun,
    CoolingCompletion,
    ManualRun,
};

struct EffectiveControlTarget {
    double targetCelsius{0.0};
    ControlTargetKind targetKind{ControlTargetKind::FermentationRun};
    std::uint32_t sourceRevision{0U};
    bool valid{false};
};

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

// Resolves both sides through the canonical phase/mode matrix. A persisted
// RunSensorMode change is not by itself a PI-role change (PREHEATING and
// WAITING_FOR_PRODUCT are always Air).
[[nodiscard]] std::optional<CommittedControlContextTransition>
resolveControlSensorRoleTransition(
    ProcessState beforePhase,
    const std::optional<RunSensorMode>& beforeRunSensorMode,
    ProcessState afterPhase,
    const std::optional<RunSensorMode>& afterRunSensorMode);

// The canonical qualification-context projection (FR4): target and role come
// from resolveEffectiveControlContext(); band and duration are bound to the
// same immutable run/qualification contract (program targetQualification or
// ManualRunPlan), never a caller-supplied value. Only valid for the phases
// that own an active qualification episode (Preheating, ReachingTarget,
// QualifyingTarget) - a target_qualification.hpp-independent header stays
// intentionally forward-declared here to avoid widening this shared header.
struct EffectiveQualificationContext {
    ControlSensorRole controlSensorRole{ControlSensorRole::Air};
    double targetCelsius{0.0};
    double bandCelsius{0.0};
    std::uint64_t qualificationDurationMillis{0U};
    bool valid{false};
};

[[nodiscard]] EffectiveQualificationContext
resolveEffectiveQualificationContext(const RunCommandState& current);

}  // namespace fermentation
