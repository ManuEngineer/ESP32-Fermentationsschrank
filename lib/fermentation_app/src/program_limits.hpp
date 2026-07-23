#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>

namespace fermentation::program_limits {

inline constexpr std::size_t kMinimumFermentationStageCount = 1U;
inline constexpr std::size_t kMaximumFermentationStageCount = 1U;
inline constexpr double kMinimumFermentationTemperatureCelsius = 4.0;
inline constexpr double kMaximumFermentationTemperatureCelsius = 45.0;
inline constexpr std::uint32_t kMinimumFermentationDurationMinutes = 1U;
inline constexpr std::uint32_t kMaximumFermentationDurationMinutes = 20160U;
inline constexpr double kMinimumQualificationBandCelsius = 0.1;
inline constexpr double kMaximumQualificationBandCelsius = 2.0;
inline constexpr std::uint32_t kMinimumQualificationDurationMinutes = 1U;
inline constexpr std::uint32_t kMaximumQualificationDurationMinutes =
    std::numeric_limits<std::uint32_t>::max();
inline constexpr std::uint32_t kMinimumTargetReachMinutes = 1U;
inline constexpr std::uint32_t kMaximumTargetReachMinutes =
    std::numeric_limits<std::uint32_t>::max();
inline constexpr std::uint32_t kMinimumProductWaitMinutes = 1U;
inline constexpr std::uint32_t kMaximumProductWaitMinutes = 1440U;
inline constexpr std::uint32_t kMinimumFallbackDelaySeconds = 0U;
inline constexpr std::uint32_t kMaximumFallbackDelaySeconds = 3600U;
inline constexpr double kMinimumCoolingTargetCelsius = 4.0;
inline constexpr double kMaximumCoolingTargetCelsius = 25.0;
inline constexpr std::uint32_t kMinimumHoldDurationMinutes = 1U;
inline constexpr std::uint32_t kMaximumHoldDurationMinutes =
    std::numeric_limits<std::uint32_t>::max();

}  // namespace fermentation::program_limits
