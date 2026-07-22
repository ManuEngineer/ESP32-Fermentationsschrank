#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace device_platform {

struct JournalEntry {
    uint64_t monotonicMillis;
    std::string message;
};

// Anwendungsneutraler Port fuer ein Fehler-/Ereignisjournal. Aufbewahrung,
// Bereinigung und fachliche Kategorien sind Aufgabe spaeterer Issues; dieser
// Port stellt nur das Aufzeichnen und Auslesen bereit.
class IEventJournal {
   public:
    IEventJournal() = default;
    virtual ~IEventJournal() = default;

    IEventJournal(const IEventJournal&) = delete;
    IEventJournal& operator=(const IEventJournal&) = delete;
    IEventJournal(IEventJournal&&) = delete;
    IEventJournal& operator=(IEventJournal&&) = delete;

    virtual void record(uint64_t monotonicMillis,
                        const std::string& message) = 0;
    [[nodiscard]] virtual const std::vector<JournalEntry>& entries() const = 0;
};

}  // namespace device_platform
