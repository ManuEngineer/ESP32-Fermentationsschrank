#pragma once

#include <vector>

#include "device_ui_text.hpp"

namespace fermentation {

[[nodiscard]] std::vector<device_platform::TextPackManifest>
makeFermentationUiTextPacks();

}  // namespace fermentation
