#include "simulated_reset_controller.hpp"

namespace device_platform_test_support {

device_platform::ResetCauseSnapshot SimulatedResetController::observeBootReset()
    const {
    return snapshot_;
}

device_platform::RestartRequestResult
SimulatedResetController::requestRestart() {
    if (restartRequestCount_ != UINT32_MAX) {
        ++restartRequestCount_;
    }
    const auto result = nextResult_;
    nextResult_ = device_platform::RestartRequestResult::Accepted;
    return result;
}

void SimulatedResetController::setBootReset(device_platform::ResetCause cause,
                                            bool valid,
                                            std::uint64_t observationId) {
    snapshot_ = {cause, valid, observationId};
}

void SimulatedResetController::setNextRestartResult(
    device_platform::RestartRequestResult result) {
    nextResult_ = result;
}

}  // namespace device_platform_test_support
