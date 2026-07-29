#pragma once

#include <cstddef>

namespace fermentation::configuration_limits {

inline constexpr std::size_t kMinimumLanguageIdBytes = 2U;
inline constexpr std::size_t kMaximumLanguageIdBytes = 16U;
inline constexpr std::size_t kMinimumTimeZoneIdBytes = 1U;
inline constexpr std::size_t kMaximumTimeZoneIdBytes = 64U;
inline constexpr std::size_t kMinimumProgramIdBytes = 1U;
inline constexpr std::size_t kMaximumProgramIdBytes = 48U;
inline constexpr std::size_t kMinimumVisibleNameScalars = 1U;
inline constexpr std::size_t kMaximumVisibleNameScalars = 48U;
inline constexpr std::size_t kMaximumVisibleNameBytes = 96U;
inline constexpr std::size_t kMaximumNotesScalars = 512U;
inline constexpr std::size_t kMaximumNotesBytes = 1024U;
inline constexpr std::size_t kFactoryProgramCount = 4U;
inline constexpr std::size_t kMaximumUserProgramCount = 12U;
inline constexpr std::size_t kMaximumProgramCount = 16U;
inline constexpr std::size_t kMaximumUserConfigurationPayloadBytes = 256U;
inline constexpr std::size_t kMaximumProgramCatalogPayloadBytes = 32768U;
inline constexpr std::size_t kConfigurationReferenceBytes = 34U;
inline constexpr std::size_t kConfigurationManifestPayloadBytes = 104U;
inline constexpr std::size_t kConfigurationRootPayloadWithoutFallbackBytes =
    35U;
inline constexpr std::size_t kConfigurationRootPayloadWithFallbackBytes = 69U;
inline constexpr std::size_t kMaximumConfigurationManifestEnvelopeBytes = 149U;
inline constexpr std::size_t kMaximumConfigurationRootEnvelopeBytes = 114U;
inline constexpr std::size_t kConfigurationDocumentSlotCount = 4U;
inline constexpr std::size_t kConfigurationManifestSlotCount = 3U;
inline constexpr std::size_t kConfigurationRootSlotCount = 2U;
inline constexpr std::size_t kMaxDistinctConfigurationModelGenerations = 2U;
inline constexpr std::size_t kMaxRuntimeConfigurationReadLeases = 8U;
inline constexpr std::size_t kMaxConcurrentFullPreviewBuilds = 1U;

}  // namespace fermentation::configuration_limits
