#pragma once

#if defined(APP_ISSUE_33_BRIDGE_IDLE_TEST)

#include <array>
#include <cstddef>
#include <cstdint>

#include "bidirectional_actuator_sink.hpp"

namespace fermentation::issue_33_bridge_idle {

// Bring-up-only, manueller Leerlauftest der vorhandenen BTS7960-Bruecke
// (Issue #33): kein Peltier, kein Verbraucher an den Ausgangsklemmen. Ein
// UART-Zeilenkommando (`bridge forward|reverse|off`) schaltet hoechstens eine
// Richtung fuer eine feste Frist; es gibt keinen automatischen EIN-Test beim
// Boot und keinen automatischen Richtungswechsel. Die Schaltlogik (Enable
// zuletzt, nie beide Richtungen, All-off-Wechsel, Verriegelung) liegt
// ausschliesslich in der uebergebenen Bruecke. Die Port-Aufrufe liefern keinen
// Fehlerwert; die Logs belegen nur den gegebenen Befehl, nicht den physischen
// Pegel. Das Release-Profil enthaelt weder diese Datei noch einen Aufrufer.
// Kein Planner-/Driver-Pfad, keine Aktorfreigabe.
class Harness {
   public:
    Harness(device_platform::IBidirectionalActuatorSink& bridge,
            bool bridgeReady) noexcept
        : bridge_(bridge), bridgeReady_(bridgeReady) {}

    void start() noexcept;
    void update() noexcept;

   private:
    enum class Direction : std::uint8_t { None, Forward, Reverse };

    void processLine() noexcept;
    void beginDirection(Direction direction, const char* name) noexcept;
    void finishDirection(const char* reason) noexcept;
    void commandAllOff() noexcept;

    device_platform::IBidirectionalActuatorSink& bridge_;
    bool bridgeReady_;
    Direction active_{Direction::None};
    const char* activeName_{nullptr};
    std::int64_t deadlineUs_{0};
    std::array<char, 64U> line_{};
    std::size_t lineLength_{0U};
    bool lineOverflow_{false};
};

}  // namespace fermentation::issue_33_bridge_idle

#endif
