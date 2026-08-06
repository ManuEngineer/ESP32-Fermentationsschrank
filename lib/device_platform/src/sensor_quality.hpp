#pragma once

#include <cstdint>

namespace device_platform {

// Oeffentlicher Qualitaetszustand einer Sensorpipeline (siehe
// docs/tasks/issue-20-sensor-quality-filtering-plan.md, Abschnitt 8). Der
// Startzustand vor der ersten gueltigen Probe wird intern als Stale ohne
// letzten gueltigen Wert gefuehrt - kein separater vierter Zustand, da
// SAFETY_COMPONENT_FAULTS.md selbst nur die Folge VALID -> STALE -> FAILED
// kennt.
enum class SensorQuality : uint8_t {
    Valid,
    Stale,
    Failed,
};

}  // namespace device_platform
