#pragma once

#include <cstdint>

namespace fermentation {

enum class ApplicationLifecycleState : std::uint8_t {
    Initializing,
    Ready,
    ServiceRequired,
};

}  // namespace fermentation
