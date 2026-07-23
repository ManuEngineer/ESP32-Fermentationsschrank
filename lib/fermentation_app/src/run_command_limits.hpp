#pragma once

#include <cstddef>

namespace fermentation::run_command_limits {

inline constexpr std::size_t kMaximumProcessedCommandIds = 32U;
inline constexpr std::size_t kMaximumRuntimeMessages = 16U;
inline constexpr std::size_t kMaximumCommandEffects = 6U;

}  // namespace fermentation::run_command_limits
