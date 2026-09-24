#include "app_config.hpp"
#include "device_platform.hpp"
#include "fermentation_application.hpp"

namespace {

bool startApplication(device_platform::DevicePlatform& platform,
                      fermentation::FermentationApplication& application) {
    const device_platform::PlatformStartupContext startupContext{
        app_config::hasSafeDefaults(app_config::kActiveProfilePolicy),
    };

    return platform.begin(startupContext) && application.begin(platform);
}

}  // namespace

int main() {
    device_platform::DevicePlatform platform;
    fermentation::FermentationApplication application;
    if (!startApplication(platform, application)) {
        return 1;
    }

    platform.update();
    application.update();
    return application.ready() ? 0 : 1;
}
