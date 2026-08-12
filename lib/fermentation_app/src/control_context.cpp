#include "control_context.hpp"

#include <cmath>

namespace fermentation {
namespace {

bool finite(double value) { return std::isfinite(value); }

bool temperatureControlledPhase(ProcessState phase) {
    switch (phase) {
        case ProcessState::Preheating:
        case ProcessState::WaitingForProduct:
        case ProcessState::ReachingTarget:
        case ProcessState::QualifyingTarget:
        case ProcessState::Fermenting:
        case ProcessState::Cooling:
        case ProcessState::CoolHolding:
        case ProcessState::ManualHolding:
            return true;
        case ProcessState::Boot:
        case ProcessState::SafeBoot:
        case ProcessState::Standby:
        case ProcessState::Completed:
        case ProcessState::RecoveryEvaluation:
        case ProcessState::Fault:
        case ProcessState::ServiceMode:
            return false;
    }
    return false;
}

bool validCoolingMode(CompletionMode mode) {
    return mode == CompletionMode::CoolThenFinish ||
           mode == CompletionMode::CoolAndHoldForDuration ||
           mode == CompletionMode::CoolAndHoldUntilManualStop;
}

std::optional<ControlSensorRole> effectiveRole(
    ProcessState phase,
    const std::optional<RunSensorMode>& activeRunSensorMode) {
    if (activeRunSensorMode.has_value()) {
        switch (*activeRunSensorMode) {
            case RunSensorMode::Product:
                break;
            case RunSensorMode::Air:
                break;
            default:
                return std::nullopt;
        }
    }
    if (phase == ProcessState::Preheating ||
        phase == ProcessState::WaitingForProduct) {
        return ControlSensorRole::Air;
    }
    if (!activeRunSensorMode.has_value()) {
        return std::nullopt;
    }
    switch (*activeRunSensorMode) {
        case RunSensorMode::Product:
            return ControlSensorRole::Product;
        case RunSensorMode::Air:
            return ControlSensorRole::Air;
    }
    return std::nullopt;
}

}  // namespace

std::optional<ControlSensorRole> resolveEffectiveControlSensorRole(
    ProcessState phase,
    const std::optional<RunSensorMode>& activeRunSensorMode) {
    return effectiveRole(phase, activeRunSensorMode);
}

std::optional<CommittedControlContextTransition>
resolveProductInsertedControlContextTransition(
    const std::optional<ControlSensorRole>& before,
    const std::optional<ControlSensorRole>& after) {
    if (before == ControlSensorRole::Air &&
        after == ControlSensorRole::Product) {
        return CommittedControlContextTransition::ProductInserted;
    }
    return std::nullopt;
}

EffectiveControlContext resolveEffectiveControlContext(
    const EffectiveControlContextInput& input) {
    EffectiveControlContext result;
    result.phase = input.phase;
    result.requestContext.processTransitionSequence =
        input.processTransitionSequence;
    result.requestContext.runRevision = input.runRevision;

    if (!temperatureControlledPhase(input.phase)) {
        return result;
    }

    const auto role = resolveEffectiveControlSensorRole(
        input.phase, input.activeRunSensorMode);
    if (!role.has_value()) {
        return result;
    }
    result.controlSensorRole = *role;
    result.requestContext.controlSensorRole = *role;

    std::optional<double> target;
    ControlTargetKind targetKind = ControlTargetKind::FermentationRun;
    switch (input.phase) {
        case ProcessState::Cooling:
        case ProcessState::CoolHolding:
            if (!validCoolingMode(input.completionMode) ||
                !input.completionCoolingTargetCelsius.has_value()) {
                return result;
            }
            target = input.completionCoolingTargetCelsius;
            targetKind = ControlTargetKind::CoolingCompletion;
            break;
        case ProcessState::ManualHolding:
            if (!input.manualRun || !input.manualTargetCelsius.has_value()) {
                return result;
            }
            target = input.manualTargetCelsius;
            targetKind = ControlTargetKind::ManualRun;
            break;
        case ProcessState::Preheating:
        case ProcessState::WaitingForProduct:
        case ProcessState::ReachingTarget:
        case ProcessState::QualifyingTarget:
        case ProcessState::Fermenting:
            target = input.effectiveFermentationTargetCelsius;
            targetKind = ControlTargetKind::FermentationRun;
            break;
        default:
            return result;
    }

    if (!target.has_value() || !finite(*target)) {
        return result;
    }
    result.target = {*target, targetKind, input.runRevision, true};
    result.valid = true;
    return result;
}

}  // namespace fermentation
