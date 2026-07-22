#include "device_platform.hpp"

namespace device_platform {

bool DevicePlatform::begin(const PlatformStartupContext& context) {
    ready_ = context.configurationSafe;
    return ready_;
}

void DevicePlatform::update() {
    // Gemeinsame Geraetedienste werden in den Folge-Issues hier angebunden.
}

bool DevicePlatform::ready() const {
    return ready_;
}

}  // namespace device_platform
