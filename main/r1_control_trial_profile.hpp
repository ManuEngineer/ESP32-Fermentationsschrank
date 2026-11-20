#pragma once

// R1 control trial profile (R1 trial plan S5; issues #32/#33/#34/#35).
//
// *** TRIAL_NOT_COMMISSIONED ***
// Included only by the dedicated bring-up trial build (APP_R1_CONTROL_TRIAL,
// scripts/build_r1_control_trial.py). Never part of the release image, never a
// commissioned value. Each number below is a technically valid PROPOSAL
// (`PROPOSAL_NOT_APPROVED`) that the owner must approve at gate G-P1 before the
// first flash of a trial image and may replace at G-P2 (new build + new flash
// gate for compile-time values). Validity: this trial image and the supervised
// trial only; replacement: the real #34/#35 values through the same injection
// points (no code change needed).
//
// Runtime values are NOT here: the planner parameters and the firmware
// temperature safety limits are set at the gates over the harness console
// (`planner set ...`, `thermal set ...`) with the numbers the owner decides.

#include <optional>

#include "control_chain.hpp"
#include "control_sensor_evidence.hpp"
#include "sensor_quality_config.hpp"

namespace fermentation::r1_trial {

// PI parameters: Kp > 0, Ki > 0 for all four sets (validDirectionParameters),
// small maximum quote for a supervised trial, integral time constant
// Ti = Kp / Ki = 50 s (well above the first switching windows).
inline ControlChainParameters trialControlChainParameters() {
    ControlChainParameters parameters;
    parameters.pi.airHeating = {0.10, 0.002, 0.5, 0.30};
    parameters.pi.airCooling = {0.10, 0.002, 0.5, 0.30};
    parameters.pi.productHeating = {0.05, 0.001, 0.5, 0.30};
    parameters.pi.productCooling = {0.05, 0.001, 0.5, 0.30};
    parameters.pi.maximumIntegrationStepMillis = 10'000U;
    parameters.pi.airLimitLowerBlockCelsius = 5.0;
    parameters.pi.airLimitLowerReduceStartCelsius = 8.0;
    parameters.pi.airLimitUpperReduceStartCelsius = 38.0;
    parameters.pi.airLimitUpperBlockCelsius = 42.0;
    parameters.policy = {IntegratorTransitionAction::Reset,
                         IntegratorTransitionAction::Reset,
                         IntegratorTransitionAction::Reset, 0.2};
    return parameters;
}

// Sensor quality per role: air and product share the cabinet range, the
// cooling body (heat sink) gets a wider plausible range.
inline ControlSensorQualityConfigs trialSensorQualityConfigs() {
    const auto create = [](double minPlausible, double maxPlausible) {
        return device_platform::SensorQualityConfig::create(
                   3U, 5.0, minPlausible, maxPlausible, 20.0, 10'000U, 3U, 2U,
                   2'000U)
            .config;
    };
    ControlSensorQualityConfigs configs;
    configs.air = create(-20.0, 60.0);
    configs.product = create(-20.0, 60.0);
    configs.cooling = create(-20.0, 100.0);
    return configs;
}

}  // namespace fermentation::r1_trial
