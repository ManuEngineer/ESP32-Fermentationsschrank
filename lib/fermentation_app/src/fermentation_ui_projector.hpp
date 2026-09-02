#pragma once

#include "fermentation_ui_models.hpp"

namespace fermentation {

// The owners provide all canonical values. Projection deliberately copies
// them into the renderer-independent snapshot and never recomputes safety,
// recovery or sensor decisions.
struct FermentationUiProjectionInput {
    FermentationUiSnapshot snapshot;
};

class FermentationUiProjector {
   public:
    [[nodiscard]] static FermentationUiSnapshot project(
        const FermentationUiProjectionInput& input);
};

}  // namespace fermentation
