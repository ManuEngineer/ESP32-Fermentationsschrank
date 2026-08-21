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
    // Nach dem bestehenden Adaptervertrag ist der Vorgang sicher nicht
    // wirksam geworden; der zuvor gespeicherte Wert (falls vorhanden) bleibt
    // dort unveraendert, wo dieser Vertrag tatsaechlich gilt.
    WriteError,
    // Nach dem bestehenden Adaptervertrag ist der Speicher voll; der zuvor
    // gespeicherte Wert (falls vorhanden) bleibt dort unveraendert, wo dieser
    // Vertrag tatsaechlich gilt.
    CapacityError,
    // Der Commit-Ausgang ist unbekannt. Der neue Wert kann bereits
    // vollstaendig und dauerhaft gespeichert sein oder auch nicht. Ein
    // spaeterer Read darf deshalb einen vollstaendig lesbaren Record,
    // `NotFound`, `ReadError`, `CapacityError` oder einen anderen vorhandenen
    // Readstatus liefern; daraus folgt auf Portebene weder OLD noch NEW.
    // Der Aufrufer muss den Readback zusammen mit Envelope-, Generation- und
    // Recoverykontext vollstaendig validieren.
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
// Technischer Vertrag pro Schluessel: ein erfolgreich zurueckgekehrter
// `write` bedeutet, dass der neue Wert vollstaendig und dauerhaft gespeichert
// ist. Fuer einen Write, dessen Abschluss unklar oder unterbrochen ist, legt
// der Port dagegen nicht fest, welchen spaeteren Readstatus oder welche Bytes
// ein Backend beobachtbar macht. Der Read kann einen vollstaendig lesbaren
// Record, `NotFound`, `ReadError`, `CapacityError` oder einen anderen
// vorhandenen Readstatus liefern. Eine Unterbrechung waehrend eines
// Same-Key-Writes erhaelt deshalb keine technische OLD/NEW-Garantie. Die
// hoeheren Ebenen muessen Records, Envelopes, Generationen, Referenzen und
// Recoveryzustand vollstaendig validieren; nicht eindeutig validierbare
// Ergebnisse bleiben indeterminiert und fail-closed.
//
// `write` liefert genau eines von vier eindeutig unterscheidbaren Ergebnissen:
//   - `Success`: der neue Wert ist vollstaendig und dauerhaft gespeichert.
//   - `WriteError`: nach dem bestehenden Adaptervertrag ist der Vorgang
//     sicher nicht wirksam geworden; der zuvor gespeicherte Wert (falls
//     vorhanden) bleibt dort unveraendert, wo dieser Vertrag tatsaechlich
//     gilt.
//   - `CapacityError`: nach dem bestehenden Adaptervertrag ist der Speicher
//     voll und der zuvor gespeicherte Wert bleibt dort unveraendert, wo dieser
//     Vertrag tatsaechlich gilt.
//   - `CommitOutcomeUnknown`: der Commit-Ausgang ist unbekannt (z. B. ein
//     Stromausfall zwischen Commit und Rueckkehr an den Aufrufer). Der neue
//     Wert kann bereits dauerhaft gespeichert sein oder auch nicht. Ein
//     spaeterer Read kann `Success` mit Bytes, `NotFound`, `ReadError`,
//     `CapacityError` oder einen anderen bestehenden Readstatus liefern; der
//     Aufrufer muss den vollstaendigen Produkt-/Recoverykontext validieren.
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
    // `NotFound` bedeutet, dass dieser konkrete Read-Aufruf keinen Wert unter
    // dem Schluessel beobachtet hat. Es ist kein historischer Beleg, dass der
    // Schluessel nie existierte. Bei Readback nach einem begonnenen Write oder
    // einer unklaren Transaktion darf ein spaeteres `NotFound` deshalb nicht
    // als urspruengliches leeres Factory-Neuheitsereignis umgedeutet werden;
    // die aufrufende Recoveryebene muss die Read-Phase und den unklaren
    // Ausgang erhalten.
    // Rueckgabetyp `StateStoreReadStatus` (in `StateStoreReadResult`)
    // schliesst `WriteError`/`CommitOutcomeUnknown` bereits durch das
    // Typsystem aus - kein Adapter kann diese als Leseergebnis zurueckgeben.
    [[nodiscard]] virtual StateStoreReadResult read(
        const StateStoreKey& key, std::size_t maxBytes) const = 0;
};

}  // namespace device_platform
