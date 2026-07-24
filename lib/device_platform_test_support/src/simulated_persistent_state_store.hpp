#pragma once

#include <cstdint>
#include <map>
#include <string>

#include "state_store.hpp"

namespace device_platform_test_support {

// Simuliert einen binaersicheren, stromausfallsicheren Schluessel/Wert-
// Speicher fuer native Tests. Trennt dauerhaft committed Daten von einem
// einstellbaren Fehlerverhalten des naechsten Schreibvorgangs, damit jeder
// in docs/CONFIGURATION_PERSISTENCE.md geforderte Cut-Point nachgebildet
// werden kann. `restart()` bildet einen simulierten Neustart nach: nur
// committed Daten ueberleben, alle Testschalter werden zurueckgesetzt.
class SimulatedPersistentStateStore final
    : public device_platform::IStateStore {
   public:
    enum class WriteFault : uint8_t {
        // Kein injizierter Fehler; normaler erfolgreicher Schreibvorgang.
        None,
        // Fehler vor Beginn: der Speicher wird nicht beruehrt.
        FailBeforeBegin,
        // Stromausfall vor Commit: der Speicher wird nicht beruehrt.
        PowerCutBeforeCommit,
        // Stromausfall nach Commit, vor Rueckkehr an den Aufrufer: der Wert
        // ist bereits dauerhaft committed, der Aufrufer sieht dennoch einen
        // Fehler.
        PowerCutAfterCommitBeforeReturn,
        // Der Speicher ist voll; der Speicher wird nicht beruehrt.
        CapacityExceeded,
    };

    [[nodiscard]] device_platform::StateStoreStatus write(
        const std::string& key, const std::string& value) override;
    [[nodiscard]] device_platform::StateStoreReadResult read(
        const std::string& key, std::size_t maxBytes) const override;

    // Gilt fuer genau den naechsten `write`-Aufruf und wird danach auf
    // `None` zurueckgesetzt.
    void setNextWriteFault(WriteFault fault);

    // Solange gesetzt, schlaegt jeder Lesevorgang fuer `key` mit
    // `ReadError` fehl.
    void injectReadFailure(const std::string& key, bool shouldFail);

    // Solange gesetzt, liefert jeder Lesevorgang fuer `key` `NotFound`,
    // unabhaengig vom tatsaechlich committed Wert.
    void forceNotFound(const std::string& key, bool shouldForce);

    // Ueberschreibt die physisch committed Bytes fuer `key` direkt, ohne den
    // normalen Schreibpfad zu durchlaufen (bildet reale Bitkorruption nach,
    // die einen Neustart uebersteht).
    void injectCorruption(const std::string& key, std::string corruptedBytes);

    // Simuliert einen Neustart: committed Daten bleiben erhalten, alle
    // Testschalter (Fault, Read-Fehler, erzwungenes NotFound) werden
    // geloescht.
    void restart();

   private:
    std::map<std::string, std::string> committed_;
    WriteFault nextWriteFault_{WriteFault::None};
    std::map<std::string, bool> readShouldFail_;
    std::map<std::string, bool> forceNotFound_;
};

}  // namespace device_platform_test_support
