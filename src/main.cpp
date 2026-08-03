#include "app_config.hpp"
#include "device_platform.hpp"
#include "fermentation_application.hpp"

namespace {

device_platform::DevicePlatform platform;
fermentation::FermentationApplication application;

bool startApplication() {
    const device_platform::PlatformStartupContext startupContext{
        app_config::hasSafeDefaults(app_config::kActiveProfilePolicy),
    };

    return platform.begin(startupContext) && application.begin(platform);
}

}  // namespace

int main() {
    if (!startApplication()) {
        return 1;
    }

    platform.update();
    application.update();
    return application.ready() ? 0 : 1;
}
