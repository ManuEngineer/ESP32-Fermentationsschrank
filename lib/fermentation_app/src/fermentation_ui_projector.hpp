#pragma once

#include "fermentation_ui_models.hpp"

namespace fermentation {

// The owners provide canonical values. Projection constructs the
// renderer-independent view models and never recomputes safety, recovery or
// sensor decisions.
struct FermentationUiProjectionInput {
    const RunCommandState* runState{nullptr};
    FermentationUiExpectedRevisions revisions;
    std::vector<FermentationUiTemperatureInput> temperatures;
    std::optional<RecoveryDisposition> recoveryDisposition;
    std::optional<RunPersistenceLoadStatus> persistenceLoadStatus;
    std::optional<RunPersistenceCoordinatorState> coordinatorState;
    PresentationState presentation;
    bool applicationReady{false};
    FermentationUiServiceSource service;
    std::optional<device_platform::TextKey> primaryAction;
    std::vector<device_platform::TextKey> semanticActions;
    FermentationUiRefreshRevisionTracker* refreshTracker{nullptr};
};

class FermentationUiProjector {
   public:
    [[nodiscard]] static FermentationUiSnapshot project(
        const FermentationUiProjectionInput& input);
};

}  // namespace fermentation
