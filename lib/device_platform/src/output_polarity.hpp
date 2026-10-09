#pragma once

#include <cstdint>

namespace device_platform {

// Wie der aktive (verbraucherseitig EIN) Zustand eines binaeren Ausgangs auf
// den physischen Pegel abgebildet wird. `Unconfirmed` bedeutet, dass die
// Polaritaet nicht bestaetigt ist; ein Adapter fuehrt dann keine
// Hardwareoperation aus und raet keinen Pegel.
enum class OutputPolarity : std::uint8_t {
    Unconfirmed,
    ActiveHigh,
    ActiveLow,
};

}  // namespace device_platform
