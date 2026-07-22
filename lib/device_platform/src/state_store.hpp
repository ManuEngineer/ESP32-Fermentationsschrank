#pragma once

#include <cstdint>
#include <string>

namespace device_platform {

enum class StateStoreReadStatus : uint8_t {
    Success,
    NotFound,
    Error,
};

struct StateStoreReadResult {
    StateStoreReadStatus status;
    // Nur bei `Success` gueltig; bei `NotFound` und `Error` leer.
    std::string value;
};

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

    // Unterscheidet einen fehlenden Schluessel von einem Speicherfehler, damit
    // der Aufrufer auf kritische Lesefehler sicher reagieren kann.
    [[nodiscard]] virtual StateStoreReadResult read(
        const std::string& key) const = 0;
};

}  // namespace device_platform
