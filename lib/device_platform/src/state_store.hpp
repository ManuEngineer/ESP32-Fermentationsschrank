#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include "state_store_key.hpp"

namespace device_platform {

// Ein gemeinsamer Status fuer Lesen und Schreiben, weil beide Operationen
// `CapacityError` teilen (siehe unten). Die jeweils gueltige Teilmenge ist
// hier eindeutig dokumentiert und in den Tests von
// `SimulatedPersistentStateStore` vollstaendig abgedeckt:
//   - `read()` liefert ausschliesslich `Success`, `NotFound`, `ReadError`
//     oder `CapacityError` - niemals `WriteError` oder
//     `CommitOutcomeUnknown` (beides reine Schreibergebnisse).
//   - `write()` liefert ausschliesslich `Success`, `WriteError`,
//     `CapacityError` oder `CommitOutcomeUnknown` - niemals `NotFound` oder
//     `ReadError` (beides reine Leseergebnisse).
enum class StateStoreStatus : uint8_t {
    Success,
    // Nur als Leseergebnis.
    NotFound,
    // Nur als Leseergebnis.
    ReadError,
    // Nur als Schreibergebnis: der Vorgang ist sicher nicht wirksam
    // geworden; der zuvor gespeicherte Wert (falls vorhanden) ist
    // unveraendert.
    WriteError,
    // Lesen: gespeicherter Wert ueberschreitet das aufrufer- beziehungsweise
    // schluesselspezifische Leselimit. Schreiben: der Speicher ist voll; der
    // zuvor gespeicherte Wert (falls vorhanden) ist unveraendert.
    CapacityError,
    // Nur als Schreibergebnis: der Commit-Ausgang ist unbekannt. Der neue
    // Wert kann bereits vollstaendig und dauerhaft gespeichert sein oder
    // auch nicht - beides ist zulaessig, ein abgeschnittener oder gemischter
    // Wert jedoch nie. Der Aufrufer muss zuruecklesen, um den tatsaechlichen
    // Stand zu bestimmen.
    CommitOutcomeUnknown,
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
// neue Wert sichtbar, nie ein abgeschnittener oder gemischter Wert.
//
// `write` liefert genau eines von vier eindeutig unterscheidbaren Ergebnissen:
//   - `Success`: der neue Wert ist vollstaendig und dauerhaft gespeichert.
//   - `WriteError`: der Vorgang ist sicher nicht wirksam geworden; der zuvor
//     gespeicherte Wert (falls vorhanden) ist unveraendert.
//   - `CapacityError`: der Speicher ist voll; ebenfalls sicher unveraendert.
//   - `CommitOutcomeUnknown`: der Commit-Ausgang ist unbekannt (z. B. ein
//     Stromausfall zwischen Commit und Rueckkehr an den Aufrufer). Der neue
//     Wert kann bereits dauerhaft gespeichert sein oder auch nicht - welcher
//     der beiden Faelle zutrifft, ist ohne Ruecklesen nicht bekannt. Es gibt
//     nie einen abgeschnittenen oder gemischten Wert. Der Aufrufer muss in
//     diesem Fall zuruecklesen, um den tatsaechlichen Stand zu bestimmen.
// Es gibt bewusst keine pauschale Garantie, dass jeder nicht erfolgreiche
// `write` den alten Wert unveraendert laesst - das gilt nur fuer `WriteError`
// und `CapacityError`, nicht fuer `CommitOutcomeUnknown`.
class IStateStore {
   public:
    IStateStore() = default;
    virtual ~IStateStore() = default;

    IStateStore(const IStateStore&) = delete;
    IStateStore& operator=(const IStateStore&) = delete;
    IStateStore(IStateStore&&) = delete;
    IStateStore& operator=(IStateStore&&) = delete;

    // Liefert ausschliesslich `Success`, `WriteError`, `CapacityError` oder
    // `CommitOutcomeUnknown` (siehe `StateStoreStatus`).
    [[nodiscard]] virtual StateStoreStatus write(const StateStoreKey& key,
                                                 const std::string& value) = 0;

    // `maxBytes` ist das aufrufer- beziehungsweise schluesselspezifische
    // Leselimit: uebersteigt der gespeicherte Wert `maxBytes`, liefert dies
    // `CapacityError` statt eines unkontrolliert grossen Werts. Liefert
    // ausschliesslich `Success`, `NotFound`, `ReadError` oder
    // `CapacityError` (siehe `StateStoreStatus`).
    [[nodiscard]] virtual StateStoreReadResult read(
        const StateStoreKey& key, std::size_t maxBytes) const = 0;
};

}  // namespace device_platform
