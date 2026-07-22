#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "event_journal.hpp"

namespace device_platform_test_support {

// Nur fuer native Tests: Ein Produktionsadapter muss Eintraege nicht als
// zusammenhaengenden In-Memory-Container bereitstellen. Der Mock tut dies der
// Einfachheit halber.
struct JournalEntry {
    uint64_t monotonicMillis;
    std::string message;
};

// In-Memory-Journal fuer native Tests mit fest begrenztem Puffer: Die
// aeltesten Eintraege werden verworfen, sobald die Kapazitaet erreicht ist
// (kein unbegrenzt wachsender Puffer, siehe AGENTS.md). `entries()` ist eine
// Testhilfe des Mocks und kein Bestandteil des Produktionsports
// `IEventJournal`.
class MockEventJournal final : public device_platform::IEventJournal {
   public:
    static constexpr std::size_t kMaxEntries = 256;

    [[nodiscard]] bool record(uint64_t monotonicMillis,
                              const std::string& message) override;
    [[nodiscard]] const std::vector<JournalEntry>& entries() const;

    // Solange gesetzt, schlaegt jeder Schreibvorgang fehl, ohne vorhandene
    // Journaleintraege zu veraendern.
    void injectWriteFailure(bool shouldFail);

   private:
    std::vector<JournalEntry> entries_;
    bool writeShouldFail_{false};
};

}  // namespace device_platform_test_support
