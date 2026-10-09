#include "esp_idf_shared_enable_bridge.hpp"

namespace device_platform_esp_idf {

EspIdfSharedEnableBridge::EspIdfSharedEnableBridge(
    int enablePin, device_platform::OutputPolarity enablePolarity,
    int forwardPin, device_platform::OutputPolarity forwardPolarity,
    int reversePin, device_platform::OutputPolarity reversePolarity) noexcept
    : enable_(enablePin, enablePolarity),
      forward_(forwardPin, forwardPolarity),
      reverse_(reversePin, reversePolarity),
      bridge_(forward_, reverse_, enable_) {}

bool EspIdfSharedEnableBridge::begin() {
    // All three outputs are always attempted in this order; each only drives
    // its inactive level.
    const auto enableResult = enable_.begin();
    const auto forwardResult = forward_.begin();
    const auto reverseResult = reverse_.begin();
    if (enableResult == BinaryOutputBeginResult::Ready &&
        forwardResult == BinaryOutputBeginResult::Ready &&
        reverseResult == BinaryOutputBeginResult::Ready) {
        return bridge_.begin();
    }
    // The bridge is not started: every enable request stays discarded. One
    // bounded best-effort OFF per output (no guarantee of a physical
    // shutdown; results are ignored and never lift a latch).
    static_cast<void>(enable_.setEnabled(false));
    static_cast<void>(forward_.setEnabled(false));
    static_cast<void>(reverse_.setEnabled(false));
    return false;
}

void EspIdfSharedEnableBridge::setForward(bool enabled) {
    bridge_.setForward(enabled);
}

void EspIdfSharedEnableBridge::setReverse(bool enabled) {
    bridge_.setReverse(enabled);
}

}  // namespace device_platform_esp_idf
