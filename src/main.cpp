#include <Arduino.h>

#include "app_config.hpp"

namespace {

constexpr uint32_t kHeartbeatIntervalMs = 1000;

uint32_t lastHeartbeatMs = 0;

void setAllActuatorsSafe() {
    // Es sind noch keine GPIOs oder aktiven Pegel real bestaetigt. Deshalb darf
    // dieser sichere Einstieg keinen Aktor-Pin konfigurieren oder ansteuern.
}

}  // namespace

void setup() {
    setAllActuatorsSafe();

    Serial.begin(app_config::kSerialBaud);
    delay(100);
    Serial.println();
    Serial.println(app_config::kProjectName);
    Serial.println("Safe build test: no unconfirmed GPIOs configured.");
}

void loop() {
    const uint32_t nowMs = millis();

    if (nowMs - lastHeartbeatMs >= kHeartbeatIntervalMs) {
        lastHeartbeatMs = nowMs;
        Serial.println("heartbeat");

    }
}
