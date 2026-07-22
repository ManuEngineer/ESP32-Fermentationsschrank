#pragma once

#include <cstdint>
#include <string>

namespace device_platform {

// Anwendungsneutraler Port fuer ein Fehler-/Ereignisjournal. Dieser Port
// definiert ausschliesslich das Aufzeichnen eines Eintrags samt expliziter
// Erfolgsmeldung. Auslesen, Aufbewahrung, Bereinigung und fachliche
// Kategorien sind keine Aufgabe dieses Produktionsports; ein konkreter
// Adapter oder Testmock darf eigene Methoden dafuer anbieten, ohne dass die
// Schnittstelle selbst einen bestimmten Speichercontainer vorschreibt.
class IEventJournal {
   public:
    IEventJournal() = default;
    virtual ~IEventJournal() = default;

    IEventJournal(const IEventJournal&) = delete;
    IEventJournal& operator=(const IEventJournal&) = delete;
    IEventJournal(IEventJournal&&) = delete;
    IEventJournal& operator=(IEventJournal&&) = delete;

    // Gibt `false` zurueck, wenn der Eintrag nicht gespeichert werden konnte.
    [[nodiscard]] virtual bool record(uint64_t monotonicMillis,
                                      const std::string& message) = 0;
};

}  // namespace device_platform
