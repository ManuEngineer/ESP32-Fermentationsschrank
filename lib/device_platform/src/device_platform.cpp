#include "device_platform.hpp"

namespace device_platform {

bool DevicePlatform::begin() {
    ready_ = app_config::hasSafeDefaults(app_config::kActiveProfilePolicy);
    return ready_;
}

void DevicePlatform::update() {
    // Gemeinsame Geraetedienste werden in den Folge-Issues hier angebunden.
}

bool DevicePlatform::ready() const {
    return ready_;
}

const app_config::ProfilePolicy& DevicePlatform::profilePolicy() const {
    return app_config::kActiveProfilePolicy;
}

}  // namespace device_platform
