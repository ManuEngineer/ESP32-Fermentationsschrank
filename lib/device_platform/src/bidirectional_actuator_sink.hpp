#pragma once

namespace device_platform {

// Anwendungsneutraler Port fuer einen bidirektionalen Aktor mit zwei
// unabhaengig ansteuerbaren Richtungen (zum Beispiel eine H-Bruecke). Beide
// Richtungen bleiben absichtlich unabhaengig ansteuerbar, damit ein Adapter
// oder Mock eine gleichzeitige Freigabe ueberhaupt abbilden und damit sichtbar
// machen kann. Diese Schnittstelle erzwingt selbst keine Exklusivitaet und
// nimmt keinen spaeteren Aktorplaner vorweg.
class IBidirectionalActuatorSink {
   public:
    IBidirectionalActuatorSink() = default;
    virtual ~IBidirectionalActuatorSink() = default;

    IBidirectionalActuatorSink(const IBidirectionalActuatorSink&) = delete;
    IBidirectionalActuatorSink& operator=(const IBidirectionalActuatorSink&) =
        delete;
    IBidirectionalActuatorSink(IBidirectionalActuatorSink&&) = delete;
    IBidirectionalActuatorSink& operator=(IBidirectionalActuatorSink&&) =
        delete;

    virtual void setForward(bool enabled) = 0;
    virtual void setReverse(bool enabled) = 0;
};

}  // namespace device_platform
