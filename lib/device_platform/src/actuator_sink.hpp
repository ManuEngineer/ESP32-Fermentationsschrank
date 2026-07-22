#pragma once

namespace device_platform {

// Anwendungsneutraler Port fuer alle Aktorbefehle des Geraets. Heizen und
// Kuehlen sind bewusst zwei unabhaengige Freigaben (wie die beiden
// Richtungseingaenge einer realen H-Bruecke), damit ein Adapter oder Mock eine
// gleichzeitige Freigabe ueberhaupt abbilden und damit sichtbar machen kann.
// Diese Schnittstelle erzwingt selbst keine Exklusivitaet; das ist Aufgabe des
// Aktorplaners (spaeteres Issue) beziehungsweise der Sichtbarmachung im Mock.
class IActuatorSink {
   public:
    IActuatorSink() = default;
    virtual ~IActuatorSink() = default;

    IActuatorSink(const IActuatorSink&) = delete;
    IActuatorSink& operator=(const IActuatorSink&) = delete;
    IActuatorSink(IActuatorSink&&) = delete;
    IActuatorSink& operator=(IActuatorSink&&) = delete;

    virtual void setHeating(bool enabled) = 0;
    virtual void setCooling(bool enabled) = 0;
    virtual void setInsideFan(bool enabled) = 0;
    virtual void setOutsideFan(bool enabled) = 0;
    virtual void setBuzzer(bool enabled) = 0;
};

}  // namespace device_platform
