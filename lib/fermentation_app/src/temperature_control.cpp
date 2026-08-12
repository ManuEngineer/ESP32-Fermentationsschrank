#include "temperature_control.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace fermentation {
namespace {

using device_platform::SensorQuality;
using device_platform::SensorQualitySnapshot;

bool finite(double value) { return std::isfinite(value); }

bool validDirectionParameters(const PiDirectionParameters& parameters) {
    return finite(parameters.proportionalGainQuotePerCelsius) &&
           parameters.proportionalGainQuotePerCelsius > 0.0 &&
           finite(parameters.integralGainQuotePerCelsiusSecond) &&
           parameters.integralGainQuotePerCelsiusSecond > 0.0 &&
           finite(parameters.neutralBandWidthCelsius) &&
           parameters.neutralBandWidthCelsius > 0.0 &&
           finite(parameters.maximumQuote) && parameters.maximumQuote > 0.0 &&
           parameters.maximumQuote <= 1.0;
}

bool validAirLimits(const TemperatureControlParameters& parameters) {
    return finite(parameters.airLimitLowerBlockCelsius) &&
           finite(parameters.airLimitLowerReduceStartCelsius) &&
           finite(parameters.airLimitUpperReduceStartCelsius) &&
           finite(parameters.airLimitUpperBlockCelsius) &&
           parameters.airLimitLowerBlockCelsius <
               parameters.airLimitLowerReduceStartCelsius &&
           parameters.airLimitUpperReduceStartCelsius <
               parameters.airLimitUpperBlockCelsius;
}

bool hasConfiguredValues(const TemperatureControlParameters& parameters) {
    return parameters.maximumIntegrationStepMillis > 0U &&
           validDirectionParameters(parameters.airHeating) &&
           validDirectionParameters(parameters.airCooling) &&
           validDirectionParameters(parameters.productHeating) &&
           validDirectionParameters(parameters.productCooling) &&
           validAirLimits(parameters);
}

bool validAction(IntegratorTransitionAction action) {
    switch (action) {
        case IntegratorTransitionAction::Reset:
        case IntegratorTransitionAction::BoundedCarry:
            return true;
    }
    return false;
}

bool validRole(ControlSensorRole role) {
    switch (role) {
        case ControlSensorRole::Air:
        case ControlSensorRole::Product:
            return true;
    }
    return false;
}

std::optional<double> sampleValue(const SensorQualitySnapshot& snapshot) {
    const std::optional<double>* value = &snapshot.filteredCelsius;
    if (!value->has_value()) {
        value = &snapshot.correctedCelsius;
    }
    if (!value->has_value()) {
        value = &snapshot.rawCelsius;
    }
    if (!value->has_value() || !finite(**value)) {
        return std::nullopt;
    }
    return **value;
}

enum class SampleValidation : std::uint8_t {
    Valid,
    Unavailable,
    Invalid,
};

SampleValidation validateSnapshot(const SensorQualitySnapshot& snapshot,
                                  double& value) {
    if (snapshot.quality != SensorQuality::Valid) {
        return SampleValidation::Unavailable;
    }
    const auto sample = sampleValue(snapshot);
    if (!sample.has_value()) {
        return SampleValidation::Invalid;
    }
    value = *sample;
    return SampleValidation::Valid;
}

TemperatureControlResult invalidResult(
    TemperatureControlStatus status, TemperatureControlReason reason,
    AirLimitState airLimitState = AirLimitState::Unavailable) {
    TemperatureControlResult result;
    result.status = status;
    result.reason = reason;
    result.airLimitState = airLimitState;
    result.direction = AbstractControlDirection::Idle;
    return result;
}

}  // namespace

bool validateTemperatureControlParameters(
    const TemperatureControlParameters& parameters) {
    return hasConfiguredValues(parameters);
}

bool validateIntegratorTransitionPolicy(
    const IntegratorTransitionPolicy& policy) {
    return validAction(policy.directionChange) &&
           validAction(policy.sensorRoleChange) &&
           validAction(policy.targetContextChange) &&
           finite(policy.transitionMaximumCarryQuote) &&
           policy.transitionMaximumCarryQuote >= 0.0 &&
           policy.transitionMaximumCarryQuote <= 1.0;
}

TemperatureController::TemperatureController(
    TemperatureControlParameters parameters, IntegratorTransitionPolicy policy,
    std::uint64_t initialRequestSequence)
    : parameters_(parameters),
      policy_(policy),
      nextRequestSequence_(initialRequestSequence) {}

bool TemperatureController::markCommittedControlContextTransitionPending(
    CommittedControlContextTransition transition) {
    if (!validateIntegratorTransitionPolicy(policy_)) {
        return false;
    }

    IntegratorTransitionAction action = policy_.targetContextChange;
    if (transition == CommittedControlContextTransition::SensorRoleChange ||
        transition == CommittedControlContextTransition::ProductInserted) {
        action = policy_.sensorRoleChange;
    }
    if (transition ==
        CommittedControlContextTransition::CoolingTargetContextChange) {
        action = policy_.targetContextChange;
    }
    state_.pendingContextTransition = transition;
    if (!state_.pendingTransitionAction.has_value() ||
        action == IntegratorTransitionAction::Reset) {
        state_.pendingTransitionAction = action;
    }
    return true;
}

TemperatureControlResult TemperatureController::evaluate(
    const TemperatureControlInput& input) {
    const auto clearFailClosed = [this]() {
        state_.integralContributionQuote = 0.0;
        state_.lastActiveDirection = std::nullopt;
        state_.pendingContextTransition = std::nullopt;
        state_.pendingTransitionAction = std::nullopt;
        state_.feedbackWindow = std::nullopt;
    };

    if (!validateTemperatureControlParameters(parameters_)) {
        clearFailClosed();
        return invalidResult(TemperatureControlStatus::Unavailable,
                             TemperatureControlReason::NoCommissioning);
    }
    if (!validateIntegratorTransitionPolicy(policy_) ||
        !validRole(input.controlSensorRole) || !finite(input.targetCelsius)) {
        clearFailClosed();
        return invalidResult(TemperatureControlStatus::InvalidInput,
                             TemperatureControlReason::InvalidConfiguration);
    }

    double airCelsius = 0.0;
    const auto airValidation = validateSnapshot(input.air, airCelsius);
    if (airValidation == SampleValidation::Unavailable) {
        clearFailClosed();
        return invalidResult(TemperatureControlStatus::Unavailable,
                             TemperatureControlReason::SensorUnavailable);
    }
    if (airValidation == SampleValidation::Invalid) {
        clearFailClosed();
        return invalidResult(TemperatureControlStatus::InvalidInput,
                             TemperatureControlReason::InvalidSample);
    }

    double measuredCelsius = airCelsius;
    if (input.controlSensorRole == ControlSensorRole::Product) {
        double productCelsius = 0.0;
        const auto productValidation =
            validateSnapshot(input.product, productCelsius);
        if (productValidation == SampleValidation::Unavailable ||
            airValidation == SampleValidation::Unavailable) {
            clearFailClosed();
            return invalidResult(TemperatureControlStatus::Unavailable,
                                 TemperatureControlReason::SensorUnavailable,
                                 AirLimitState::Unavailable);
        }
        if (productValidation == SampleValidation::Invalid) {
            clearFailClosed();
            return invalidResult(TemperatureControlStatus::InvalidInput,
                                 TemperatureControlReason::InvalidSample,
                                 AirLimitState::Unavailable);
        }
        measuredCelsius = productCelsius;
    }

    if (!finite(measuredCelsius)) {
        clearFailClosed();
        return invalidResult(
            TemperatureControlStatus::InvalidInput,
            TemperatureControlReason::InvalidSample,
            input.controlSensorRole == ControlSensorRole::Product
                ? AirLimitState::Unavailable
                : AirLimitState::NotApplied);
    }

    const bool hasPreviousTimestamp =
        state_.lastSampleTimestampMonotonicMillis.has_value();
    bool firstSample = !hasPreviousTimestamp;
    bool timestampRecoveryAnchor = state_.suppressIntegrationAfterGap;
    std::uint64_t deltaMillis = 0U;
    if (hasPreviousTimestamp) {
        const auto previous = *state_.lastSampleTimestampMonotonicMillis;
        if (input.sampleTimestampMonotonicMillis < previous) {
            clearFailClosed();
            return invalidResult(
                TemperatureControlStatus::InvalidInput,
                TemperatureControlReason::TimeInvalid,
                input.controlSensorRole == ControlSensorRole::Product
                    ? AirLimitState::Unavailable
                    : AirLimitState::NotApplied);
        }
        deltaMillis = input.sampleTimestampMonotonicMillis - previous;
        if (deltaMillis > parameters_.maximumIntegrationStepMillis) {
            state_.lastSampleTimestampMonotonicMillis =
                input.sampleTimestampMonotonicMillis;
            state_.suppressIntegrationAfterGap = true;
            clearFailClosed();
            return invalidResult(
                TemperatureControlStatus::InvalidInput,
                TemperatureControlReason::TimeInvalid,
                input.controlSensorRole == ControlSensorRole::Product
                    ? AirLimitState::Unavailable
                    : AirLimitState::NotApplied);
        }
    }
    state_.lastSampleTimestampMonotonicMillis =
        input.sampleTimestampMonotonicMillis;
    state_.suppressIntegrationAfterGap = false;

    bool allowPositiveIntegration = false;
    if (state_.feedbackWindow.has_value()) {
        if (!input.previousControlRequestFeedback.has_value()) {
            // Fehlendes Feedback friert den Integrator fuer genau diesen
            // Folgezyklus ein und schliesst das Fenster.
            state_.feedbackWindow = std::nullopt;
        } else {
            const auto& feedback = *input.previousControlRequestFeedback;
            if (feedback.controlRequestSequence !=
                    state_.feedbackWindow->identity.sequence ||
                feedback.controlRequestSequence == 0U) {
                clearFailClosed();
                return invalidResult(
                    TemperatureControlStatus::InvalidInput,
                    TemperatureControlReason::InvalidSample,
                    input.controlSensorRole == ControlSensorRole::Product
                        ? AirLimitState::Unavailable
                        : AirLimitState::NotApplied);
            }
            allowPositiveIntegration = feedback.disposition ==
                                       PreviousControlRequestFeedback::
                                           Disposition::NoIntegratorConstraint;
            state_.feedbackWindow = std::nullopt;
        }
    } else if (input.previousControlRequestFeedback.has_value()) {
        // Feedback fuer OFF, ein fremdes Fenster oder ein bereits
        // konsumiertes Fenster ist kein gueltiges Anti-Windup-Signal.
        clearFailClosed();
        return invalidResult(
            TemperatureControlStatus::InvalidInput,
            TemperatureControlReason::InvalidSample,
            input.controlSensorRole == ControlSensorRole::Product
                ? AirLimitState::Unavailable
                : AirLimitState::NotApplied);
    }

    const double rawErrorCelsius = input.targetCelsius - measuredCelsius;
    if (!finite(rawErrorCelsius)) {
        clearFailClosed();
        return invalidResult(
            TemperatureControlStatus::InvalidInput,
            TemperatureControlReason::InvalidSample,
            input.controlSensorRole == ControlSensorRole::Product
                ? AirLimitState::Unavailable
                : AirLimitState::NotApplied);
    }

    const PiDirectionParameters* profile = nullptr;
    AbstractControlDirection direction = AbstractControlDirection::Idle;
    if (input.controlSensorRole == ControlSensorRole::Air) {
        if (rawErrorCelsius > parameters_.airHeating.neutralBandWidthCelsius) {
            profile = &parameters_.airHeating;
            direction = AbstractControlDirection::Heating;
        } else if (rawErrorCelsius <
                   -parameters_.airCooling.neutralBandWidthCelsius) {
            profile = &parameters_.airCooling;
            direction = AbstractControlDirection::Cooling;
        }
    } else {
        if (rawErrorCelsius >
            parameters_.productHeating.neutralBandWidthCelsius) {
            profile = &parameters_.productHeating;
            direction = AbstractControlDirection::Heating;
        } else if (rawErrorCelsius <
                   -parameters_.productCooling.neutralBandWidthCelsius) {
            profile = &parameters_.productCooling;
            direction = AbstractControlDirection::Cooling;
        }
    }

    TemperatureControlResult result;
    result.direction = direction;
    result.airLimitState = input.controlSensorRole == ControlSensorRole::Air
                               ? AirLimitState::NotApplied
                               : AirLimitState::Unrestricted;

    if (direction == AbstractControlDirection::Idle) {
        result.status = TemperatureControlStatus::Off;
        result.reason = TemperatureControlReason::NeutralBand;
        result.timeQuote = 0.0;
        // Idle does not change the active-direction anchor and does not
        // consume a pending transition because no new profile is known.
        if (requestIdentityExhausted_) {
            return invalidResult(
                TemperatureControlStatus::InvalidInput,
                TemperatureControlReason::RequestIdentityExhausted,
                result.airLimitState);
        }
        ControlRequest request;
        request.identity = {nextRequestSequence_,
                            input.sampleTimestampMonotonicMillis};
        request.context = {input.processTransitionSequence, input.runRevision,
                           input.controlSensorRole};
        request.direction = AbstractControlDirection::Idle;
        request.timeQuote = 0.0;
        result.controlRequest = request;
        state_.feedbackWindow = std::nullopt;
        if (nextRequestSequence_ == std::numeric_limits<std::uint64_t>::max()) {
            requestIdentityExhausted_ = true;
        } else {
            ++nextRequestSequence_;
        }
        firstSample = false;
        timestampRecoveryAnchor = false;
        static_cast<void>(firstSample);
        static_cast<void>(timestampRecoveryAnchor);
        return result;
    }

    const double directionalThreshold = profile->neutralBandWidthCelsius;
    const double activeErrorCelsius =
        std::fabs(rawErrorCelsius) - directionalThreshold;
    if (!finite(activeErrorCelsius) || activeErrorCelsius <= 0.0) {
        clearFailClosed();
        return invalidResult(TemperatureControlStatus::InvalidInput,
                             TemperatureControlReason::InvalidSample,
                             result.airLimitState);
    }

    const double rawP =
        profile->proportionalGainQuotePerCelsius * activeErrorCelsius;
    if (!finite(rawP) || rawP < 0.0) {
        clearFailClosed();
        return invalidResult(TemperatureControlStatus::InvalidInput,
                             TemperatureControlReason::InvalidSample,
                             result.airLimitState);
    }
    result.rawProportionalQuote = rawP;

    // Die normale Luftbegrenzung ist bereits vor der Integrationsentscheidung
    // bekannt. Eine Reduktion oder Sperre darf keine neue positive I-Ladung
    // erlauben; die eigentliche Quotenausgabe wird weiter unten mit derselben
    // checked Formel berechnet.
    if (input.controlSensorRole == ControlSensorRole::Product) {
        if (direction == AbstractControlDirection::Heating) {
            if (airCelsius >= parameters_.airLimitUpperBlockCelsius) {
                result.airLimitState = AirLimitState::Blocked;
            } else if (airCelsius >
                       parameters_.airLimitUpperReduceStartCelsius) {
                result.airLimitState = AirLimitState::Reduced;
            }
        } else if (airCelsius <= parameters_.airLimitLowerBlockCelsius) {
            result.airLimitState = AirLimitState::Blocked;
        } else if (airCelsius < parameters_.airLimitLowerReduceStartCelsius) {
            result.airLimitState = AirLimitState::Reduced;
        }
    }
    const bool airLimitRestricts =
        result.airLimitState == AirLimitState::Reduced ||
        result.airLimitState == AirLimitState::Blocked;

    bool directionChanged = false;
    if (state_.lastActiveDirection.has_value()) {
        directionChanged = *state_.lastActiveDirection != direction;
    }
    const bool hasPendingTransition =
        state_.pendingTransitionAction.has_value();
    const bool transitionSample = directionChanged || hasPendingTransition ||
                                  firstSample || timestampRecoveryAnchor;
    if (hasPendingTransition || directionChanged) {
        IntegratorTransitionAction action =
            state_.pendingTransitionAction.value_or(policy_.directionChange);
        if (hasPendingTransition && directionChanged &&
            policy_.directionChange == IntegratorTransitionAction::Reset) {
            action = IntegratorTransitionAction::Reset;
        }
        if (action == IntegratorTransitionAction::Reset) {
            state_.integralContributionQuote = 0.0;
        } else {
            state_.integralContributionQuote = std::min(
                {state_.integralContributionQuote,
                 policy_.transitionMaximumCarryQuote, profile->maximumQuote});
        }
        state_.pendingContextTransition = std::nullopt;
        state_.pendingTransitionAction = std::nullopt;
    }

    double allowedDeltaI = 0.0;
    if (allowPositiveIntegration && !airLimitRestricts && !transitionSample &&
        !firstSample && deltaMillis > 0U) {
        const double dtSeconds = static_cast<double>(deltaMillis) / 1000.0;
        const double deltaI = profile->integralGainQuotePerCelsiusSecond *
                              activeErrorCelsius * dtSeconds;
        if (!finite(dtSeconds) || !finite(deltaI) || deltaI < 0.0) {
            clearFailClosed();
            return invalidResult(TemperatureControlStatus::InvalidInput,
                                 TemperatureControlReason::InvalidSample,
                                 result.airLimitState);
        }
        allowedDeltaI = deltaI;
    }

    const double integralHeadroom = std::max(0.0, profile->maximumQuote - rawP);
    if (!finite(integralHeadroom)) {
        clearFailClosed();
        return invalidResult(TemperatureControlStatus::InvalidInput,
                             TemperatureControlReason::InvalidSample,
                             result.airLimitState);
    }
    if (rawP >= profile->maximumQuote) {
        state_.integralContributionQuote = 0.0;
    } else {
        const double candidate =
            state_.integralContributionQuote + allowedDeltaI;
        if (!finite(candidate)) {
            clearFailClosed();
            return invalidResult(TemperatureControlStatus::InvalidInput,
                                 TemperatureControlReason::InvalidSample,
                                 result.airLimitState);
        }
        state_.integralContributionQuote =
            std::min(std::max(candidate, 0.0), integralHeadroom);
    }
    const double unboundedQuote = rawP + state_.integralContributionQuote;
    if (!finite(unboundedQuote)) {
        clearFailClosed();
        return invalidResult(TemperatureControlStatus::InvalidInput,
                             TemperatureControlReason::InvalidSample,
                             result.airLimitState);
    }
    const double maximumLimitedQuote =
        std::min(unboundedQuote, profile->maximumQuote);
    result.integralContributionQuote = state_.integralContributionQuote;
    result.unboundedQuote = unboundedQuote;
    result.maximumLimitedQuote = maximumLimitedQuote;

    double limitedQuote = maximumLimitedQuote;
    if (input.controlSensorRole == ControlSensorRole::Product) {
        if (direction == AbstractControlDirection::Heating) {
            if (airCelsius >= parameters_.airLimitUpperBlockCelsius) {
                result.airLimitState = AirLimitState::Blocked;
                limitedQuote = 0.0;
            } else if (airCelsius >
                       parameters_.airLimitUpperReduceStartCelsius) {
                result.airLimitState = AirLimitState::Reduced;
                const double denominator =
                    parameters_.airLimitUpperBlockCelsius -
                    parameters_.airLimitUpperReduceStartCelsius;
                const double factor =
                    (parameters_.airLimitUpperBlockCelsius - airCelsius) /
                    denominator;
                limitedQuote = maximumLimitedQuote * factor;
            }
        } else if (airCelsius <= parameters_.airLimitLowerBlockCelsius) {
            result.airLimitState = AirLimitState::Blocked;
            limitedQuote = 0.0;
        } else if (airCelsius < parameters_.airLimitLowerReduceStartCelsius) {
            result.airLimitState = AirLimitState::Reduced;
            const double denominator =
                parameters_.airLimitLowerReduceStartCelsius -
                parameters_.airLimitLowerBlockCelsius;
            const double factor =
                (airCelsius - parameters_.airLimitLowerBlockCelsius) /
                denominator;
            limitedQuote = maximumLimitedQuote * factor;
        }
    }
    if (!finite(limitedQuote) || limitedQuote < 0.0) {
        clearFailClosed();
        return invalidResult(TemperatureControlStatus::InvalidInput,
                             TemperatureControlReason::InvalidSample,
                             result.airLimitState);
    }
    limitedQuote = std::min(limitedQuote, profile->maximumQuote);
    result.timeQuote = limitedQuote;

    if (result.airLimitState == AirLimitState::Blocked || limitedQuote == 0.0) {
        result.status = TemperatureControlStatus::Off;
        result.reason = TemperatureControlReason::AirLimitBlocked;
    } else {
        result.status = TemperatureControlStatus::Demand;
        if (result.airLimitState == AirLimitState::Reduced) {
            result.reason = TemperatureControlReason::AirLimitReduced;
        } else if (rawP >= profile->maximumQuote ||
                   maximumLimitedQuote >= profile->maximumQuote) {
            result.reason = TemperatureControlReason::Saturated;
        } else {
            result.reason = TemperatureControlReason::None;
        }
    }

    if (requestIdentityExhausted_) {
        clearFailClosed();
        return invalidResult(TemperatureControlStatus::InvalidInput,
                             TemperatureControlReason::RequestIdentityExhausted,
                             result.airLimitState);
    }
    ControlRequest request;
    request.identity = {nextRequestSequence_,
                        input.sampleTimestampMonotonicMillis};
    request.context = {input.processTransitionSequence, input.runRevision,
                       input.controlSensorRole};
    request.direction = result.status == TemperatureControlStatus::Off
                            ? AbstractControlDirection::Idle
                            : direction;
    request.timeQuote = limitedQuote;
    result.controlRequest = request;
    if (result.status == TemperatureControlStatus::Demand) {
        state_.feedbackWindow = request;
    } else {
        state_.feedbackWindow = std::nullopt;
    }
    if (nextRequestSequence_ == std::numeric_limits<std::uint64_t>::max()) {
        requestIdentityExhausted_ = true;
    } else {
        ++nextRequestSequence_;
    }
    state_.lastActiveDirection = direction;
    return result;
}

}  // namespace fermentation
