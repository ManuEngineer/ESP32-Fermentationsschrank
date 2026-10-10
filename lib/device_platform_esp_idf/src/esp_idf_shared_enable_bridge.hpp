#pragma once

#include "bidirectional_actuator_sink.hpp"
#include "esp_idf_binary_output_sink.hpp"
#include "output_polarity.hpp"
#include "shared_enable_bridge_sink.hpp"

namespace device_platform_esp_idf {

// Konkreter GPIO-Adapter fuer eine Bruecke mit gemeinsamem Enable (zum Beispiel
// BTS7960/IBT-2): drei `EspIdfBinaryOutputSink` plus die portable
// `SharedEnableBridgeSink`. Der Adapter kennt keine Rollen; Pins und
// Polaritaeten legt der Aufrufer fest (Composition Root, aus der SSOT).
// Der Konstruktor greift nicht auf Hardware zu. `begin()` ist die einzige
// Initialisierung (Enable, dann Vorwaerts, dann Rueckwaerts, jeweils inaktiv
// vorgesetzt); die Bruecke wird erst nach drei erfolgreichen Ausgangs-
// Initialisierungen gestartet. Schlaegt eine Stufe fehl, bleibt die Bruecke
// ungestartet (jedes EIN wird verworfen) und jeder Ausgang erhaelt genau einen
// Best-effort-AUS-Versuch. Ein Erfolg ist keine Aussage ueber Modulpolaritaet
// oder Boot-/Reset-Hardwareverhalten.
class EspIdfSharedEnableBridge final
    : public device_platform::IBidirectionalActuatorSink {
   public:
    EspIdfSharedEnableBridge(
        int enablePin, device_platform::OutputPolarity enablePolarity,
        int forwardPin, device_platform::OutputPolarity forwardPolarity,
        int reversePin,
        device_platform::OutputPolarity reversePolarity) noexcept;

    EspIdfSharedEnableBridge(const EspIdfSharedEnableBridge&) = delete;
    EspIdfSharedEnableBridge& operator=(const EspIdfSharedEnableBridge&) =
        delete;
    EspIdfSharedEnableBridge(EspIdfSharedEnableBridge&&) = delete;
    EspIdfSharedEnableBridge& operator=(EspIdfSharedEnableBridge&&) = delete;

    // `true` genau dann, wenn alle drei Ausgaenge und die Bruecke bereit sind.
    [[nodiscard]] bool begin();

    void setForward(bool enabled) override;
    void setReverse(bool enabled) override;

   private:
    EspIdfBinaryOutputSink enable_;
    EspIdfBinaryOutputSink forward_;
    EspIdfBinaryOutputSink reverse_;
    device_platform::SharedEnableBridgeSink bridge_;
};

}  // namespace device_platform_esp_idf
