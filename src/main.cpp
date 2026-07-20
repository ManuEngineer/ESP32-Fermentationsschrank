#include <Arduino.h>

#include "app_config.hpp"

namespace {

constexpr uint32_t kHeartbeatIntervalMs = 1000;

uint32_t lastHeartbeatMs = 0;

}  // namespace

void setup() {
    // Keine Aktor-GPIOs konfigurieren, bis Pins und aktive Pegel gemessen sind.
    Serial.begin(app_config::kSerialBaud);
    Serial.println();
    Serial.println(app_config::kProjectName);
    Serial.println("Safe build test: no unconfirmed GPIOs configured.");
}

void loop() {
    const uint32_t nowMs = millis();

    if (nowMs - lastHeartbeatMs >= kHeartbeatIntervalMs) {
        lastHeartbeatMs = nowMs;
        Serial.println("heartbeat: safe test mode");
    }
}
