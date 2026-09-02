#pragma once

#include "fermentation_ui_models.hpp"

namespace fermentation {

// The owners provide all canonical values. Projection deliberately copies
// them into the renderer-independent snapshot and never recomputes safety,
// recovery or sensor decisions.
struct FermentationUiProjectionInput {
    const RunCommandState* runState{nullptr};
    FermentationUiExpectedRevisions revisions;
    std::vector<TemperatureView> temperatures;
    std::vector<MessageView> messages;
    std::optional<RecoveryDisposition> recoveryDisposition;
    std::optional<RunPersistenceLoadStatus> persistenceLoadStatus;
    std::optional<RunPersistenceCoordinatorState> coordinatorState;
    ApplicationStatusView status;
    ServiceAvailabilityView service;
    std::optional<device_platform::TextKey> primaryAction;
    std::vector<device_platform::TextKey> semanticActions;
    std::uint64_t semanticPublicationRevision{0U};
    device_platform::UiRefreshRevisionTracker* refreshTracker{nullptr};
};

class FermentationUiProjector {
   public:
    [[nodiscard]] static FermentationUiSnapshot project(
        const FermentationUiProjectionInput& input);
};

}  // namespace fermentation
