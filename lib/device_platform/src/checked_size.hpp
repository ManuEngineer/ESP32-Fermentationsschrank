#pragma once

#include <cstddef>

// Anwendungsneutraler, ueberlaufsicherer Groessen-Additionshelfer. `size_t`
// ist plattformabhaengig (auf dem ESP32 32 Bit, auf dem nativen Testhost
// typischerweise 64 Bit); jede Groessenberechnung vor einer Reservierung
// oder Allokation muss unabhaengig von dieser Breite sicher gegen Ueberlauf
// sein. Siehe docs/CONFIGURATION_PERSISTENCE.md, Abschnitt "Envelope-Version
// 1".
namespace device_platform {

// Liefert `false`, wenn `a + b` entweder den Wertebereich von `std::size_t`
// ueberlaufen wuerde oder `maxAllowed` ueberschreiten wuerde; `outSum` bleibt
// in diesem Fall unveraendert. `maxAllowed` ist explizit uebergeben statt
// implizit `SIZE_MAX` anzunehmen, damit sich auch auf einem 64-Bit-Testhost
// eine kleinere (z. B. 32-Bit-) Grenze deterministisch nachweisen laesst.
[[nodiscard]] constexpr bool checkedAddSize(std::size_t a, std::size_t b,
                                            std::size_t maxAllowed,
                                            std::size_t& outSum) {
    if (a > maxAllowed || b > maxAllowed) {
        return false;
    }
    if (a > maxAllowed - b) {
        return false;
    }
    outSum = a + b;
    return true;
}

}  // namespace device_platform
