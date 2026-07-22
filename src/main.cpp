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

#if defined(ARDUINO)

#include <Arduino.h>

namespace {

constexpr uint32_t kHeartbeatIntervalMs = 1000;
uint32_t lastHeartbeatMs = 0;
bool applicationStarted = false;

void printBootSummary() {
    const auto& policy = app_config::kActiveProfilePolicy;

    Serial.println();
    Serial.println(app_config::kProjectName);
    Serial.print("profile: ");
    Serial.println(app_config::profileName(policy.profile));
    Serial.print("hardware state: ");
    Serial.println(app_config::hardwareStateName(policy.startupHardwareState));
    Serial.print("actuator policy: ");
    Serial.println(app_config::actuatorPolicyName(policy.actuatorPolicy));
    Serial.println("real actuators: disabled");
    Serial.println(applicationStarted ? "application: ready"
                                      : "application: startup failed");
}

}  // namespace

void setup() {
    Serial.begin(app_config::kSerialBaud);
    applicationStarted = startApplication();
    printBootSummary();
}

void loop() {
    if (!applicationStarted) {
        return;
    }

    platform.update();
    application.update();

    const uint32_t nowMs = millis();
    if (nowMs - lastHeartbeatMs >= kHeartbeatIntervalMs) {
        lastHeartbeatMs = nowMs;
        Serial.println("heartbeat: safe test mode");
    }
}

#else

int main() {
    if (!startApplication()) {
        return 1;
    }

    platform.update();
    application.update();
    return application.ready() ? 0 : 1;
}

#endif
