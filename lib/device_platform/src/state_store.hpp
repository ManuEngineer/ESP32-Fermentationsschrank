#pragma once

#include <optional>
#include <string>

namespace device_platform {

// Anwendungsneutraler Persistenz-Port. Bewusst generisch (Schluessel/Wert):
// Schema, atomare Revisionen und Rueckfalllogik sind Aufgabe spaeterer Issues
// (Konfigurations- und Laufpersistenz), nicht dieses Ports.
class IStateStore {
   public:
    IStateStore() = default;
    virtual ~IStateStore() = default;

    IStateStore(const IStateStore&) = delete;
    IStateStore& operator=(const IStateStore&) = delete;
    IStateStore(IStateStore&&) = delete;
    IStateStore& operator=(IStateStore&&) = delete;

    // Gibt `false` zurueck, wenn der Schreibvorgang fehlschlaegt (z. B.
    // injizierter kritischer Speicherfehler). Bei `false` bleibt ein zuvor
    // gespeicherter Wert fuer diesen Schluessel unveraendert.
    [[nodiscard]] virtual bool write(const std::string& key,
                                     const std::string& value) = 0;

    // Liefert `std::nullopt`, wenn kein Wert vorhanden ist oder der Lesevorgang
    // fehlschlaegt (z. B. injizierter Lesefehler).
    [[nodiscard]] virtual std::optional<std::string> read(
        const std::string& key) const = 0;
};

}  // namespace device_platform
