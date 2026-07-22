#pragma once

#include <cstdint>
#include <optional>

namespace device_platform {

// Anwendungsneutraler Zeit-Port. Der fachliche Kern darf reale Systemzeit nur
// ueber diese Schnittstelle beziehen (siehe AGENTS.md, Architekturregeln).
class ITimeSource {
   public:
    ITimeSource() = default;
    virtual ~ITimeSource() = default;

    ITimeSource(const ITimeSource&) = delete;
    ITimeSource& operator=(const ITimeSource&) = delete;
    ITimeSource(ITimeSource&&) = delete;
    ITimeSource& operator=(ITimeSource&&) = delete;

    // Monoton steigende Millisekunden seit Erstellung dieser
    // Zeitquelleninstanz. Faellt nie zurueck und wird von Aenderungen der
    // absoluten Zeit nicht beeinflusst. Eine neue Instanz (z. B. nach einem
    // Neustart) beginnt wieder bei 0.
    [[nodiscard]] virtual uint64_t monotonicMillis() const = 0;

    // Absolute UTC-Zeit in Sekunden seit der Epoche, sofern aktuell bekannt
    // (zum Beispiel nach einem erfolgreichen NTP-Abgleich). `std::nullopt`,
    // solange keine verlaessliche absolute Zeit vorliegt.
    [[nodiscard]] virtual std::optional<int64_t> unixTimeSeconds() const = 0;
};

}  // namespace device_platform
