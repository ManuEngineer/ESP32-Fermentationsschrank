#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include "state_store_key.hpp"

namespace device_platform {

// Read- und Write-Ergebnisse sind bewusst getrennte Typen, nicht nur eine
// dokumentierte Teilmenge eines gemeinsamen Enums: ein Adapter kann
// `StateStoreWriteStatus::WriteError` oder `CommitOutcomeUnknown` schon
// aufgrund des Rueckgabetyps nicht als Leseergebnis zurueckgeben, und
// umgekehrt kann `read()` `NotFound`/`ReadError` nicht als Schreibergebnis
// liefern - das ist ein Compilefehler, kein nur dokumentierter oder
// getesteter Vertrag. `CapacityError` kommt bewusst in beiden Enums vor: Lesen
// meldet damit ein den Leselimit ueberschreitendes Ergebnis, Schreiben einen
// vollen Speicher.
enum class StateStoreReadStatus : uint8_t {
    Success,
    NotFound,
    ReadError,
    // Gespeicherter Wert ueberschreitet das aufrufer- beziehungsweise
    // schluesselspezifische Leselimit.
    CapacityError,
};

enum class StateStoreWriteStatus : uint8_t {
    Success,
    // Der Vorgang ist sicher nicht wirksam geworden; der zuvor gespeicherte
    // Wert (falls vorhanden) ist unveraendert.
    WriteError,
    // Der Speicher ist voll; der zuvor gespeicherte Wert (falls vorhanden)
    // ist unveraendert.
    CapacityError,
    // Der Commit-Ausgang ist unbekannt. Der neue Wert kann bereits
    // vollstaendig und dauerhaft gespeichert sein oder auch nicht - beides
    // ist zulaessig, ein abgeschnittener oder gemischter Wert jedoch nie.
    // Der Aufrufer muss zuruecklesen, um den tatsaechlichen Stand zu
    // bestimmen.
    CommitOutcomeUnknown,
};

struct StateStoreReadResult {
    StateStoreReadStatus status;
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

    // Rueckgabetyp `StateStoreWriteStatus` schliesst `NotFound`/`ReadError`
    // bereits durch das Typsystem aus - kein Adapter kann diese als
    // Schreibergebnis zurueckgeben.
    [[nodiscard]] virtual StateStoreWriteStatus write(
        const StateStoreKey& key, const std::string& value) = 0;

    // `maxBytes` ist das aufrufer- beziehungsweise schluesselspezifische
    // Leselimit: uebersteigt der gespeicherte Wert `maxBytes`, liefert dies
    // `CapacityError` statt eines unkontrolliert grossen Werts.
    // Rueckgabetyp `StateStoreReadStatus` (in `StateStoreReadResult`)
    // schliesst `WriteError`/`CommitOutcomeUnknown` bereits durch das
    // Typsystem aus - kein Adapter kann diese als Leseergebnis zurueckgeben.
    [[nodiscard]] virtual StateStoreReadResult read(
        const StateStoreKey& key, std::size_t maxBytes) const = 0;
};

}  // namespace device_platform
