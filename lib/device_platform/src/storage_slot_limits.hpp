#pragma once

#include <cstddef>

namespace device_platform::storage_slot_limits {

// Ein technischer Scan verarbeitet eine homogene Recordgruppe. Die aktuell
// bekannten Gruppen benoetigen hoechstens vier Slots; acht lassen eine kleine
// Reserve fuer dieselbe Plattformabstraktion, ohne den Ergebnisspeicher
// unbeschraenkt mit einer aufruferbestimmten Slotzahl wachsen zu lassen.
inline constexpr std::size_t kMaximumTechnicalSlotsPerScan = 8U;

}  // namespace device_platform::storage_slot_limits
