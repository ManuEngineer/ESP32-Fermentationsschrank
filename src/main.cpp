#include <Arduino.h>

namespace {

constexpr uint32_t kSerialBaud = 115200;
constexpr uint32_t kHeartbeatIntervalMs = 1000;

uint32_t lastHeartbeatMs = 0;

void setAllActuatorsSafe() {
    // Projektspezifisch implementieren.
    // Alle Ausgaenge muessen vor der weiteren Initialisierung sicher AUS sein.
}

}  // namespace

void setup() {
    setAllActuatorsSafe();

    Serial.begin(kSerialBaud);
    delay(100);
    Serial.println();
    Serial.println("PROJECT_NAME");
    Serial.println("Boot completed; actuators are in safe state.");

#ifdef LED_BUILTIN
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW);
#endif
}

void loop() {
    const uint32_t nowMs = millis();

    if (nowMs - lastHeartbeatMs >= kHeartbeatIntervalMs) {
        lastHeartbeatMs = nowMs;
        Serial.println("heartbeat");

#ifdef LED_BUILTIN
        digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
#endif
    }
}
