#pragma once

#include <cstdint>

namespace fermentation {

// Kleine, fluechtige Control-Kontextwerte ohne Abhaengigkeit auf den
// PI-Parameter- oder Resultvertrag. Diese Typen werden von Prozess-,
// Kommando-, Persistenz- und Control-Context-Schichten geteilt.
enum class ControlSensorRole : std::uint8_t {
    Air,
    Product,
};

struct ControlRequestContext {
    std::uint32_t processTransitionSequence{0U};
    std::uint32_t runRevision{0U};
    ControlSensorRole controlSensorRole{ControlSensorRole::Air};
};

enum class CommittedControlContextTransition : std::uint8_t {
    TargetContextChange,
    SensorRoleChange,
    ProductInserted,
    CoolingTargetContextChange,
};

}  // namespace fermentation
