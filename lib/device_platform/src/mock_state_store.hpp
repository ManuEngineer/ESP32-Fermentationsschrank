#pragma once

#include <map>
#include <string>

#include "state_store.hpp"

namespace device_platform {

// Speicherbasiertes Persistenz-Backend fuer native Tests mit injizierbaren
// Fehlern, um kritische Speicherausfaelle nachzubilden.
class MockStateStore final : public IStateStore {
   public:
    [[nodiscard]] bool write(const std::string& key,
                             const std::string& value) override;
    [[nodiscard]] StateStoreReadResult read(
        const std::string& key) const override;

    // Solange gesetzt, schlaegt jeder Schreibvorgang fehl, ohne den
    // gespeicherten Zustand zu veraendern (simuliert einen kritischen,
    // nicht schreibbaren Speicher).
    void injectWriteFailure(bool shouldFail);

    // Solange gesetzt, schlaegt jeder Lesevorgang fehl (simuliert einen
    // kritischen, nicht lesbaren Speicher).
    void injectReadFailure(bool shouldFail);

   private:
    std::map<std::string, std::string> values_;
    bool writeShouldFail_{false};
    bool readShouldFail_{false};
};

}  // namespace device_platform
