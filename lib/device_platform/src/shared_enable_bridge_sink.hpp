#pragma once

#include "bidirectional_actuator_sink.hpp"
#include "binary_output_sink.hpp"

namespace device_platform {

// Anwendungsneutrale Bruecke fuer einen zweischenkligen Aktor mit gemeinsamem
// Enable (zum Beispiel eine H-Bruecke). Die Klasse setzt die zwei
// Richtungsbefehle auf drei binaere Ausgaenge um und erzwingt an dieser Grenze:
//  - Enable ist die Master-Sperre: beim Einschalten zuletzt, beim Ausschalten
//    zuerst;
//  - nie beide Schenkel gleichzeitig: ein widerspruechlicher Befehl schaltet
//    alles ab und verriegelt;
//  - ein Richtungswechsel laeuft nur ueber den vollstaendigen All-off-Zustand;
//  - jeder Fehler, unbekannte Zustand oder falsche Aufrufreihenfolge schaltet
//    ab und verriegelt bis zum Neustart (kein Re-Arm).
// Die Klasse hat keinen Zeitgeber; Mindest-Auszeit und Totzeit liegen allein
// beim Aktorplaner. "AUS" ist ein softwareseitiger Befehl und keine
// garantierte physische Abschaltung. Die drei Ausgaenge muessen vor `begin()`
// erfolgreich initialisiert sein; ein nicht initialisierter Ausgang meldet
// bei `setEnabled(false)` `false` und zaehlt nie als erfolgreiche All-off-
// Initialisierung.
class SharedEnableBridgeSink final : public IBidirectionalActuatorSink {
   public:
    SharedEnableBridgeSink(IBinaryOutputSink& forwardLeg,
                           IBinaryOutputSink& reverseLeg,
                           IBinaryOutputSink& enable) noexcept;

    // Einmalige Initialisierung: Enable, Vorwaerts, Rueckwaerts AUS; alle drei
    // muessen `true` liefern. Liefert `true` genau dann, wenn die Bruecke
    // bereit ist. Ein weiterer Aufruf aendert nichts.
    [[nodiscard]] bool begin();

    void setForward(bool enabled) override;
    void setReverse(bool enabled) override;

   private:
    enum class State : unsigned char { NotStarted, Ready, Faulted };

    [[nodiscard]] bool allOff();
    void shutdown();
    void setLeg(IBinaryOutputSink& leg, bool& legOn, bool otherLegOn,
                bool enabled);

    IBinaryOutputSink& forwardLeg_;
    IBinaryOutputSink& reverseLeg_;
    IBinaryOutputSink& enable_;
    State state_{State::NotStarted};
    bool forwardOn_{false};
    bool reverseOn_{false};
};

}  // namespace device_platform
