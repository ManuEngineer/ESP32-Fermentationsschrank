#pragma once

#include <cstddef>
#include <string>

namespace fermentation::run_limits {

inline constexpr std::size_t kMinimumRunIdBytes = 1U;
inline constexpr std::size_t kMaximumRunIdBytes = 48U;

[[nodiscard]] inline bool validRunId(const std::string& runId) {
    return runId.size() >= kMinimumRunIdBytes &&
           runId.size() <= kMaximumRunIdBytes;
}

}  // namespace fermentation::run_limits
