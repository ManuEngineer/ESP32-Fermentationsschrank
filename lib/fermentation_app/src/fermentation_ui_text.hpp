#pragma once

#include <vector>

#include "device_ui_text.hpp"
#include "run_commands.hpp"

namespace fermentation {

[[nodiscard]] device_platform::TextKey fermentationTextKey(const char* value);
// One text key per canonical message code / class (the enums are owned by the
// run command module); never derived from the code's numeric value.
[[nodiscard]] device_platform::TextKey messageCodeTextKey(MessageCode code);
[[nodiscard]] device_platform::TextKey messageClassTextKey(
    MessageClass messageClass);
[[nodiscard]] std::vector<device_platform::TextPackManifest>
makeFermentationUiTextPacks();

}  // namespace fermentation
