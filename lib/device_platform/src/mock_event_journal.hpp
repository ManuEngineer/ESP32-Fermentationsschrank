#pragma once

#include "event_journal.hpp"

namespace device_platform {

// In-Memory-Journal fuer native Tests mit fest begrenztem Puffer: Die
// aeltesten Eintraege werden verworfen, sobald die Kapazitaet erreicht ist
// (kein unbegrenzt wachsender Puffer, siehe AGENTS.md).
class MockEventJournal final : public IEventJournal {
   public:
    static constexpr std::size_t kMaxEntries = 256;

    void record(uint64_t monotonicMillis, const std::string& message) override;
    [[nodiscard]] const std::vector<JournalEntry>& entries() const override;

   private:
    std::vector<JournalEntry> entries_;
};

}  // namespace device_platform
