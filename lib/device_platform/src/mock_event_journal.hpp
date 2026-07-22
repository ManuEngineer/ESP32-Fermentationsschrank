#pragma once

#include "event_journal.hpp"

namespace device_platform {

// In-Memory-Journal fuer native Tests mit fest begrenztem Puffer: Die
// aeltesten Eintraege werden verworfen, sobald die Kapazitaet erreicht ist
// (kein unbegrenzt wachsender Puffer, siehe AGENTS.md).
class MockEventJournal final : public IEventJournal {
   public:
    static constexpr std::size_t kMaxEntries = 256;

    [[nodiscard]] bool record(uint64_t monotonicMillis,
                              const std::string& message) override;
    [[nodiscard]] const std::vector<JournalEntry>& entries() const override;

    // Solange gesetzt, schlaegt jeder Schreibvorgang fehl, ohne vorhandene
    // Journaleintraege zu veraendern.
    void injectWriteFailure(bool shouldFail);

   private:
    std::vector<JournalEntry> entries_;
    bool writeShouldFail_{false};
};

}  // namespace device_platform
