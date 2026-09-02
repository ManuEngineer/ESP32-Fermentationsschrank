#pragma once

#include <optional>

#include "actuation_interlock.hpp"
#include "reset_cause.hpp"

namespace fermentation {

// Diagnostic/UI-only state. It is not an input to ActuationInterlock and
// acknowledgement can never grant an actuation permission.
struct PresentationState {
    FaultCode faultCode{FaultCode::None};
    bool acknowledged{false};
    std::optional<device_platform::ResetCause> resetCause;
    bool applicationAllocationFailure{false};
};

}  // namespace fermentation
