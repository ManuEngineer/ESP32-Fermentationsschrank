#pragma once

#include <vector>

#include "device_ui_text.hpp"

namespace fermentation {

[[nodiscard]] device_platform::TextKey fermentationTextKey(const char* value);
[[nodiscard]] std::vector<device_platform::TextPackManifest>
makeFermentationUiTextPacks();

}  // namespace fermentation
