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
    FermentationUiApplicationSource application;
    FermentationUiServiceSource service;
    FermentationUiNetworkSource network;
    FermentationWebAccessState webAccess{
        FermentationWebAccessState::NotApplicable};
    std::optional<device_platform::TextKey> primaryAction;
    std::vector<device_platform::TextKey> semanticActions;
    FermentationUiRefreshRevisionTracker* refreshTracker{nullptr};
};

class FermentationUiProjector {
   public:
    [[nodiscard]] static FermentationUiSnapshot project(
        const FermentationUiProjectionInput& input);
    // Same projection into an existing snapshot. The temperature, message and
    // semantic-action buffers of `output` are cleared but kept, so a recycled
    // snapshot is refilled without new heap allocation once its capacity has
    // been reached (steady-state UI loop). Every other field is reset exactly
    // as for a freshly constructed snapshot.
    static void projectInto(FermentationUiSnapshot& output,
                            const FermentationUiProjectionInput& input);
};

}  // namespace fermentation
