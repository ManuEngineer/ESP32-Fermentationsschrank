#pragma once

#include <cstdint>
#include <optional>

#include "control_context_types.hpp"
#include "sensor_selection_types.hpp"

namespace fermentation {

struct ControlRequestIdentity {
    std::uint64_t sequence{0U};
    std::uint64_t createdAtMonotonicMillis{0U};
};

[[nodiscard]] inline bool operator==(const ControlRequestIdentity& left,
                                     const ControlRequestIdentity& right) {
    return left.sequence == right.sequence &&
           left.createdAtMonotonicMillis == right.createdAtMonotonicMillis;
}

[[nodiscard]] inline bool operator!=(const ControlRequestIdentity& left,
                                     const ControlRequestIdentity& right) {
    return !(left == right);
}

struct ControlRequest {
    ControlRequestIdentity identity;
    ControlRequestContext context;
    AbstractControlDirection direction{AbstractControlDirection::Idle};
    double timeQuote{0.0};
};

enum class TemperatureControlStatus : std::uint8_t {
    Demand,
    Off,
    Unavailable,
    InvalidInput,
};

enum class TemperatureControlReason : std::uint8_t {
    None,
    NeutralBand,
    Saturated,
    AirLimitReduced,
    AirLimitBlocked,
    NoCommissioning,
    SensorUnavailable,
    InvalidConfiguration,
    InvalidSample,
    TimeInvalid,
    RequestIdentityExhausted,
};

enum class AirLimitState : std::uint8_t {
    NotApplied,
    Unrestricted,
    Reduced,
    Blocked,
    Unavailable,
};

struct PreviousControlRequestFeedback {
    enum class Disposition : std::uint8_t {
        NoIntegratorConstraint,
        DeferredOrLimited,
        Rejected,
    };

    std::uint64_t controlRequestSequence{0U};
    Disposition disposition{Disposition::Rejected};
};

struct TemperatureControlResult {
    TemperatureControlStatus status{TemperatureControlStatus::InvalidInput};
    TemperatureControlReason reason{TemperatureControlReason::InvalidSample};
    AirLimitState airLimitState{AirLimitState::Unavailable};
    AbstractControlDirection direction{AbstractControlDirection::Idle};
    double timeQuote{0.0};
    double rawProportionalQuote{0.0};
    double integralContributionQuote{0.0};
    double unboundedQuote{0.0};
    double maximumLimitedQuote{0.0};
    std::optional<ControlRequest> controlRequest;
};

struct PiDirectionParameters {
    double proportionalGainQuotePerCelsius{0.0};
    double integralGainQuotePerCelsiusSecond{0.0};
    double neutralBandWidthCelsius{0.0};
    double maximumQuote{0.0};
};

struct TemperatureControlParameters {
    PiDirectionParameters airHeating;
    PiDirectionParameters airCooling;
    PiDirectionParameters productHeating;
    PiDirectionParameters productCooling;
    std::uint64_t maximumIntegrationStepMillis{0U};
    double airLimitLowerBlockCelsius{0.0};
    double airLimitLowerReduceStartCelsius{0.0};
    double airLimitUpperReduceStartCelsius{0.0};
    double airLimitUpperBlockCelsius{0.0};
};

enum class IntegratorTransitionAction : std::uint8_t {
    Reset,
    BoundedCarry,
};

struct IntegratorTransitionPolicy {
    IntegratorTransitionAction directionChange{
        IntegratorTransitionAction::Reset};
    IntegratorTransitionAction sensorRoleChange{
        IntegratorTransitionAction::Reset};
    IntegratorTransitionAction targetContextChange{
        IntegratorTransitionAction::Reset};
    double transitionMaximumCarryQuote{0.0};
};

}  // namespace fermentation
