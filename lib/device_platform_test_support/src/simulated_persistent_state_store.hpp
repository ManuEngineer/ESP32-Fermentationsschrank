#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>

#include "state_store.hpp"
#include "state_store_key.hpp"

namespace device_platform_test_support {

// Simuliert einen binaersicheren, stromausfallsicheren Schluessel/Wert-
// Speicher fuer native Tests. Trennt drei Zustandsbereiche: dauerhaft
// committed Daten (`committed_`), die aktuelle beziehungsweise laufende
// Schreiboperation (`pendingWrite_`) und sonstigen fluechtigen Testzustand
// (Fault-Schalter, Read-/NotFound-Injektion). Jeder in
// docs/CONFIGURATION_PERSISTENCE.md geforderte Cut-Point kann damit
// nachgebildet werden. `restart()` bildet einen simulierten Neustart nach:
// nur committed Daten ueberleben, `pendingWrite_` und alle Testschalter
// werden zurueckgesetzt.
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
        // ist bereits dauerhaft committed; der Aufrufer erhaelt
        // `CommitOutcomeUnknown` und muss zuruecklesen, um dies festzustellen.
        PowerCutAfterCommitBeforeReturn,
        // Der Speicher ist voll; der Speicher wird nicht beruehrt.
        CapacityExceeded,
    };

    [[nodiscard]] device_platform::StateStoreWriteStatus write(
        const device_platform::StateStoreKey& key,
        const std::string& value) override;
    [[nodiscard]] device_platform::StateStoreReadResult read(
        const device_platform::StateStoreKey& key,
        std::size_t maxBytes) const override;

    // Gilt fuer genau den naechsten `write`-Aufruf und wird danach auf
    // `None` zurueckgesetzt.
    void setNextWriteFault(WriteFault fault);

    // Solange gesetzt, schlaegt jeder Lesevorgang fuer `key` mit
    // `ReadError` fehl.
    void injectReadFailure(const device_platform::StateStoreKey& key,
                           bool shouldFail);

    // Solange gesetzt, liefert jeder Lesevorgang fuer `key` `NotFound`,
    // unabhaengig vom tatsaechlich committed Wert.
    void forceNotFound(const device_platform::StateStoreKey& key,
                       bool shouldForce);

    // Ueberschreibt die physisch committed Bytes fuer `key` direkt, ohne den
    // normalen Schreibpfad zu durchlaufen (bildet reale Bitkorruption nach,
    // die einen Neustart uebersteht).
    void injectCorruption(const device_platform::StateStoreKey& key,
                          std::string corruptedBytes);

    // Simuliert einen Neustart: committed Daten bleiben erhalten, eine noch
    // nicht committete laufende Schreiboperation sowie alle Testschalter
    // (Fault, Read-Fehler, erzwungenes NotFound) werden geloescht.
    void restart();

    // Nur fuer native Tests: macht sichtbar, ob nach dem letzten `write()`
    // eine gestagte, aber noch nicht committete Schreiboperation existiert
    // (z. B. nach einem simulierten Stromausfall vor Commit). Keine
    // Produktionsschnittstelle; existiert nur, weil die interne
    // Staging-/Commit-Trennung ueber die oeffentliche `IStateStore`-Sicht
    // sonst nicht beobachtbar waere.
    [[nodiscard]] bool hasPendingWriteForTesting() const {
        return pendingWrite_.has_value();
    }

    // Nur fuer native Tests: beweist neben der Existenz auch, dass die
    // laufende Schreiboperation Schluessel und vollstaendigen binaeren Wert
    // unveraendert gestagt hat. Vergroessert den Produktionsport nicht.
    [[nodiscard]] bool pendingWriteMatchesForTesting(
        const device_platform::StateStoreKey& key,
        const std::string& value) const {
        return pendingWrite_.has_value() && pendingWrite_->key == key &&
               pendingWrite_->value == value;
    }

   private:
    struct PendingWrite {
        device_platform::StateStoreKey key;
        std::string value;
    };

    std::map<device_platform::StateStoreKey, std::string> committed_;
    std::optional<PendingWrite> pendingWrite_;
    WriteFault nextWriteFault_{WriteFault::None};
    std::map<device_platform::StateStoreKey, bool> readShouldFail_;
    std::map<device_platform::StateStoreKey, bool> forceNotFound_;
};

}  // namespace device_platform_test_support
