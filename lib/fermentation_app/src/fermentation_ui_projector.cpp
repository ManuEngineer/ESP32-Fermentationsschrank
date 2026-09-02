#include "fermentation_ui_projector.hpp"

namespace fermentation {

FermentationUiSnapshot FermentationUiProjector::project(
    const FermentationUiProjectionInput& input) {
    return input.snapshot;
}

}  // namespace fermentation
