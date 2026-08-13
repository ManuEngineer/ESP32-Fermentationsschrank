#pragma once

#include <cstdint>
#include <optional>

#include "sensor_quality_snapshot.hpp"
#include "temperature_control_types.hpp"

namespace fermentation {

struct TemperatureControlInput {
    std::uint64_t sampleTimestampMonotonicMillis{0U};
    double targetCelsius{0.0};
    ControlSensorRole controlSensorRole{ControlSensorRole::Air};
    device_platform::SensorQualitySnapshot air;
    device_platform::SensorQualitySnapshot product;
    std::optional<PreviousControlRequestFeedback>
        previousControlRequestFeedback;
    std::uint32_t processTransitionSequence{0U};
    std::uint32_t runRevision{0U};
};

enum class TemperatureControlParametersValidation : std::uint8_t {
    Unconfigured,
    Valid,
    Invalid,
};

[[nodiscard]] TemperatureControlParametersValidation
classifyTemperatureControlParameters(
    const TemperatureControlParameters& parameters);
[[nodiscard]] bool validateTemperatureControlParameters(
    const TemperatureControlParameters& parameters);
[[nodiscard]] bool validateIntegratorTransitionPolicy(
    const IntegratorTransitionPolicy& policy);

struct TemperatureControlRuntimeState {
    double integralContributionQuote{0.0};
    std::optional<std::uint64_t> lastSampleTimestampMonotonicMillis;
    bool suppressIntegrationAfterGap{false};
    std::optional<AbstractControlDirection> lastActiveDirection;
    std::optional<CommittedControlContextTransition> pendingContextTransition;
    std::optional<IntegratorTransitionAction> pendingTransitionAction;
    std::optional<ControlRequest> feedbackWindow;
};

class TemperatureController {
   public:
    TemperatureController(TemperatureControlParameters parameters,
                          IntegratorTransitionPolicy policy,
                          std::uint64_t initialRequestSequence = 1U);

    [[nodiscard]] TemperatureControlResult evaluate(
        const TemperatureControlInput& input);

    // Wird ausschliesslich nach einem erfolgreich persistierten und
    // angewendeten Kontextwechsel aufgerufen. Ein Idle-Sample verbraucht die
    // Markierung nicht; die konkrete neue Richtung muss erst bekannt sein.
    [[nodiscard]] bool markCommittedControlContextTransitionPending(
        CommittedControlContextTransition transition);

    // Fail-closed RAM boundary for a new run, recovery, or leaving the
    // temperature-control domain. Request-identity high-watermark state is
    // deliberately retained so reset cannot enable sequence reuse.
    void resetRuntime();

    [[nodiscard]] const TemperatureControlRuntimeState& state() const {
        return state_;
    }

    [[nodiscard]] const TemperatureControlParameters& parameters() const {
        return parameters_;
    }

   private:
    TemperatureControlParameters parameters_;
    IntegratorTransitionPolicy policy_;
    TemperatureControlRuntimeState state_;
    std::uint64_t nextRequestSequence_{1U};
    bool requestIdentityExhausted_{false};
};

}  // namespace fermentation
