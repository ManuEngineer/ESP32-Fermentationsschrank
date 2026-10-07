#pragma once

#include <vector>

#include "device_ui_text.hpp"
#include "fermentation_ui_models.hpp"
#include "program_model.hpp"
#include "run_commands.hpp"
#include "sensor_quality.hpp"
#include "sensor_selection_types.hpp"

namespace fermentation {

[[nodiscard]] device_platform::TextKey fermentationTextKey(const char* value);
// One text key per canonical message code / class (the enums are owned by the
// run command module); never derived from the code's numeric value.
[[nodiscard]] device_platform::TextKey messageCodeTextKey(MessageCode code);
[[nodiscard]] device_platform::TextKey messageClassTextKey(
    MessageClass messageClass);
// Read-only page content labels (S7): one key per canonical enum value, each
// an exhaustive switch; never derived from the numeric value.
[[nodiscard]] device_platform::TextKey processStateTextKey(ProcessState state);
[[nodiscard]] device_platform::TextKey recoveryModeTextKey(
    RecoveryViewMode mode);
[[nodiscard]] device_platform::TextKey sensorPreferenceTextKey(
    SensorPreference preference);
[[nodiscard]] device_platform::TextKey runSensorModeTextKey(RunSensorMode mode);
[[nodiscard]] device_platform::TextKey completionModeTextKey(
    CompletionMode mode);
[[nodiscard]] device_platform::TextKey temperatureRoleTextKey(
    FermentationTemperatureRole role);
[[nodiscard]] device_platform::TextKey sensorQualityTextKey(
    device_platform::SensorQuality quality);
[[nodiscard]] std::vector<device_platform::TextPackManifest>
makeFermentationUiTextPacks();

}  // namespace fermentation
