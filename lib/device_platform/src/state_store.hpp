#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace device_platform {

enum class StateStoreStatus : uint8_t {
    Success,
    NotFound,
    ReadError,
    WriteError,
    // Lesen: gespeicherter Wert ueberschreitet das aufrufer- beziehungsweise
    // schluesselspezifische Leselimit. Schreiben: der Speicher ist voll.
    CapacityError,
};

struct StateStoreReadResult {
    StateStoreStatus status;
    // Nur bei `Success` gueltig; sonst leer.
    std::string value;
};

// Anwendungsneutraler, begrenzter und binaersicherer Persistenz-Port.
// Bewusst generisch (Schluessel/Wert): konkrete Schluesselbedeutung, Schema,
// atomare Revisionen und Rueckfalllogik sind Aufgabe der aufrufenden
// Anwendung, nicht dieses Ports (siehe docs/CONFIGURATION_PERSISTENCE.md,
// Abschnitt "Speicherport und Modulgrenzen").
//
// Vertrag pro Schluessel: ein erfolgreich zurueckgekehrter `write` ersetzt den
// vorherigen Wert atomar und dauerhaft. Nach einer Unterbrechung ist fuer
// jeden Schluessel entweder der vollstaendige alte oder der vollstaendige
// neue Wert sichtbar, nie ein abgeschnittener oder gemischter Wert. Ein
// fehlgeschlagener `write` laesst den zuvor gespeicherten Wert unveraendert.
class IStateStore {
   public:
    IStateStore() = default;
    virtual ~IStateStore() = default;

    IStateStore(const IStateStore&) = delete;
    IStateStore& operator=(const IStateStore&) = delete;
    IStateStore(IStateStore&&) = delete;
    IStateStore& operator=(IStateStore&&) = delete;

    [[nodiscard]] virtual StateStoreStatus write(const std::string& key,
                                                 const std::string& value) = 0;

    // `maxBytes` ist das aufrufer- beziehungsweise schluesselspezifische
    // Leselimit: uebersteigt der gespeicherte Wert `maxBytes`, liefert dies
    // `CapacityError` statt eines unkontrolliert grossen Werts.
    [[nodiscard]] virtual StateStoreReadResult read(
        const std::string& key, std::size_t maxBytes) const = 0;
};

}  // namespace device_platform
