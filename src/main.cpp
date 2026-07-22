#include "app_config.hpp"

#if defined(ARDUINO)

#include <Arduino.h>

namespace {

constexpr uint32_t kHeartbeatIntervalMs = 1000;

uint32_t lastHeartbeatMs = 0;

}  // namespace

void setup() {
    Serial.begin(app_config::kSerialBaud);
    Serial.println();
    Serial.println(app_config::kProjectName);
    Serial.print("profile: ");
    Serial.println(
        app_config::profileName(app_config::kActiveProfilePolicy.profile));
    Serial.print("hardware state: ");
    Serial.println(app_config::hardwareStateName(
        app_config::kActiveProfilePolicy.startupHardwareState));
    Serial.print("actuator policy: ");
    Serial.println(app_config::actuatorPolicyName(
        app_config::kActiveProfilePolicy.actuatorPolicy));
    Serial.println("real actuators: disabled");
}

void loop() {
    const uint32_t nowMs = millis();

    if (nowMs - lastHeartbeatMs >= kHeartbeatIntervalMs) {
        lastHeartbeatMs = nowMs;
        Serial.println("heartbeat: safe test mode");
    }
}

#else

int main() {
    return app_config::hasSafeDefaults(app_config::kActiveProfilePolicy) ? 0 : 1;
}

#endif
