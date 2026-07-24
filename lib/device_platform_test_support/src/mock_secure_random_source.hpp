#pragma once

#include <cstdint>
#include <string>

#include "secure_random_source.hpp"

namespace device_platform_test_support {

// Deterministisch steuerbare Zufallsquelle fuer native Tests. Ohne
// vorgegebene Bytes liefert sie eine reproduzierbare, seedbare
// Pseudozufallsfolge; mit `setNextBytes` liefert sie exakt die
// vorgegebenen Bytes fuer den naechsten `fill`-Aufruf (fuer Golden-Tests
// spaeterer Anwendungsfaelle).
class MockSecureRandomSource final
    : public device_platform::ISecureRandomSource {
   public:
    explicit MockSecureRandomSource(uint64_t seed = 1U) : state_(seed) {}

    [[nodiscard]] bool fill(void* buffer, std::size_t length) override;

    // Ueberschreibt genau den naechsten `fill`-Aufruf mit exakt `bytes`
    // (Laenge muss zur angeforderten Laenge passen); danach faellt `fill`
    // wieder auf die seedbare Folge zurueck.
    void setNextBytes(std::string bytes);

    // Solange gesetzt, schlaegt jeder `fill`-Aufruf fehl, ohne den Puffer zu
    // veraendern.
    void injectFailure(bool shouldFail);

   private:
    uint64_t state_;
    std::string nextBytes_;
    bool hasNextBytes_{false};
    bool shouldFail_{false};
};

}  // namespace device_platform_test_support
