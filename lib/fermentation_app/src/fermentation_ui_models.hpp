#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "configuration_documents.hpp"
#include "device_ui_contracts.hpp"
#include "device_ui_session.hpp"
#include "device_ui_theme.hpp"
#include "presentation_state.hpp"
#include "run_commands.hpp"
#include "run_persistence_coordinator.hpp"
#include "sensor_quality_snapshot.hpp"

namespace fermentation {

struct FermentationUiExpectedRevisions {
    std::uint32_t expectedStateSequence{0U};
    std::optional<std::uint32_t> expectedRunRevision;
    std::optional<std::uint32_t> expectedMessageRevision;
    std::optional<std::uint32_t> expectedFaultRevision;
    std::optional<std::uint32_t> expectedRecoveryEpisodeRevision;
    std::optional<UserConfigurationRevision> expectedUserConfigurationRevision;
    std::optional<ProgramCatalogRevision> expectedProgramCatalogRevision;
};

enum class FermentationHomeMode : std::uint8_t {
    Standby,
    ActiveRun,
    Recovery,
    ServiceRequired,
    Unavailable,
};

struct FermentationHomeView {
    FermentationHomeMode mode{FermentationHomeMode::Unavailable};
    ProcessState processState{ProcessState::Boot};
    std::string activeRunId;
    std::optional<EffectiveRunValues> effectiveValues;
    device_platform::TextKey primaryAction;
};

enum class FermentationTemperatureRole : std::uint8_t {
    CabinetAir,
    Product,
    Cooling,
};

struct TemperatureView {
    FermentationTemperatureRole role{FermentationTemperatureRole::CabinetAir};
    std::optional<double> valueCelsius;
    device_platform::SensorQualitySnapshot quality;
};

struct MessageView {
    RuntimeMessage message;
};

enum class RecoveryViewMode : std::uint8_t {
    Normal,
    WaitingForTrustedTime,
    CurrentRunRecovered,
    FallbackSelectionRequired,
    RecoveryRejectedOrFailClosed,
    Completed,
    Cooling,
};

struct RecoveryView {
    RecoveryViewMode mode{RecoveryViewMode::Normal};
    std::optional<RecoveryDisposition> canonicalRecoveryDisposition;
    std::optional<RunPersistenceLoadStatus> persistenceLoadStatus;
    std::optional<RunPersistenceCoordinatorState> coordinatorState;
};

struct FermentationNavigationView {
    std::vector<device_platform::TextKey> semanticActions;
};

struct ApplicationStatusView {
    PresentationState presentation;
    bool ready{false};
};

struct ServiceAvailabilityView {
    bool available{false};
    bool confirmationRequired{false};
    bool serviceAuthorizationRequired{false};
    std::optional<device_platform::TextKey> unavailableReason;
};

struct FermentationUiSnapshot {
    FermentationUiExpectedRevisions revisions;
    FermentationHomeView home;
    FermentationNavigationView navigation;
    std::vector<TemperatureView> temperatures;
    std::vector<MessageView> messages;
    RecoveryView recovery;
    ApplicationStatusView status;
    ServiceAvailabilityView service;
    std::optional<device_platform::UiRefreshRevision> refreshRevision;
};

struct FermentationUiTemperatureInput {
    FermentationTemperatureRole role{FermentationTemperatureRole::CabinetAir};
    std::optional<double> valueCelsius;
    device_platform::SensorQualitySnapshot quality;
};

struct FermentationUiServiceSource {
    bool available{false};
    bool confirmationRequired{false};
    bool serviceAuthorizationRequired{false};
    std::optional<device_platform::TextKey> unavailableReason;
};

// Owning application state supplied to the projector. This is deliberately
// not an ApplicationStatusView; the projector alone creates that UI model.
struct FermentationUiApplicationSource {
    PresentationState presentation;
    bool ready{false};
};

class FermentationUiRefreshRevisionTracker {
   public:
    [[nodiscard]] device_platform::UiRefreshRevision publish(
        const FermentationUiSnapshot& snapshot);
    [[nodiscard]] device_platform::UiRefreshRevision current() const noexcept {
        return revision_;
    }

   private:
    std::optional<FermentationUiSnapshot> published_;
    device_platform::UiRefreshRevision revision_{};
};

[[nodiscard]] bool equalFermentationUiSemanticSnapshot(
    const FermentationUiSnapshot& left,
    const FermentationUiSnapshot& right) noexcept;

[[nodiscard]] device_platform::DeviceUiBuildCatalog
makeFermentationR1DeviceUiBuildCatalog();
[[nodiscard]] std::vector<device_platform::ThemeDescriptor>
makeFermentationR1ThemeDescriptors();
[[nodiscard]] device_platform::ServiceSessionPolicy
fermentationTouchServicePolicy();
[[nodiscard]] device_platform::ServiceSessionPolicy
fermentationWebServicePolicy();

}  // namespace fermentation
