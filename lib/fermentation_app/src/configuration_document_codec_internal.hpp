#pragma once

#include <cstddef>
#include <cstdint>

#include "configuration_document_codec.hpp"

// Kleine codecinterne Testoberflaeche. Sie ist kein persistenter
// Anwendungsvertrag und fuehrt bewusst keine allgemeine Codec-Infrastruktur
// ein. Produktion und native Grenztests verwenden exakt dieselben
// Enum-Zuordnungen und dieselbe ProgramCatalog-Groessenberechnung.
namespace fermentation::configuration_codec_internal {

struct ProgramCatalogPayloadSizeResult {
    ConfigurationCodecStatus status{ConfigurationCodecStatus::InvalidDocument};
    std::size_t payloadSize{0U};
};

[[nodiscard]] bool sensorPreferenceToWireId(SensorPreference value,
                                            std::uint8_t& out);
[[nodiscard]] bool sensorPreferenceFromWireId(std::uint8_t wireId,
                                              SensorPreference& out);

[[nodiscard]] bool productSensorFailurePolicyToWireId(
    ProductSensorFailurePolicy value, std::uint8_t& out);
[[nodiscard]] bool productSensorFailurePolicyFromWireId(
    std::uint8_t wireId, ProductSensorFailurePolicy& out);

[[nodiscard]] bool completionModeToWireId(CompletionMode value,
                                          std::uint8_t& out);
[[nodiscard]] bool completionModeFromWireId(std::uint8_t wireId,
                                            CompletionMode& out);

[[nodiscard]] ProgramCatalogPayloadSizeResult
calculateProgramCatalogPayloadSize(const ProgramCatalog& catalog);

}  // namespace fermentation::configuration_codec_internal
