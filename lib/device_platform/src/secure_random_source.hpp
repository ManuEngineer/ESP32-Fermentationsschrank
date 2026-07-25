#pragma once

#include <cstddef>

namespace device_platform {

// Anwendungsneutraler Port fuer kryptografisch geeigneten Zufall. Verwendet
// von spaeteren Konfigurations- und Sicherheitsfunktionen (z. B. Preview-
// Handles, Nonces, Tokens); kennt selbst keine fachliche Bedeutung.
class ISecureRandomSource {
   public:
    ISecureRandomSource() = default;
    virtual ~ISecureRandomSource() = default;

    ISecureRandomSource(const ISecureRandomSource&) = delete;
    ISecureRandomSource& operator=(const ISecureRandomSource&) = delete;
    ISecureRandomSource(ISecureRandomSource&&) = delete;
    ISecureRandomSource& operator=(ISecureRandomSource&&) = delete;

    // Fuellt genau `length` Bytes ab `buffer`. Liefert `false`, wenn keine
    // ausreichende Zufallsquelle zur Verfuegung steht; `buffer` bleibt in
    // diesem Fall unveraendert. `length == 0`: `buffer` darf `nullptr` sein,
    // der Aufruf ist ein erfolgreicher No-op ohne jede Zustandsaenderung
    // (kein Verbrauch vorbereiteter Testdaten, kein Fortschalten eines
    // Generatorzustands). Positive Laenge: `nullptr` muss von jeder
    // Implementierung beobachtbar mit `false` abgelehnt werden, ohne
    // `buffer` zu dereferenzieren.
    [[nodiscard]] virtual bool fill(void* buffer, std::size_t length) = 0;
};

}  // namespace device_platform
