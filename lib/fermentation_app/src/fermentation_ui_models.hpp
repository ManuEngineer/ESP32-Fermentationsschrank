#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "configuration_documents.hpp"
#include "application_lifecycle.hpp"
#include "device_ui_contracts.hpp"
#include "device_ui_session.hpp"
#include "device_ui_theme.hpp"
#include "network_mode.hpp"
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
    Waiting,
    Completed,
    Restricted,
    Recovery,
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

// The network selector is the smallest renderer-independent view needed by
// local and future UI surfaces. UNSELECTED is an internal bootstrap state;
// the fixed array is the complete user-facing choice set. SoftAP setup data
// intentionally does not belong here and remains on the dedicated
// FermentationApplication accessor.
struct FermentationNetworkModeView {
    device_platform::NetworkMode currentMode{
        device_platform::NetworkMode::UNSELECTED};
    bool selectionRequired{true};
    std::array<device_platform::NetworkMode, 2U> selectableModes{
        device_platform::NetworkMode::AP_ONLY,
        device_platform::NetworkMode::HOME_WIFI};
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
    FermentationNetworkModeView network;
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

// Application-owned, secret-free network input for projection. Credentials
// and SoftAP access data never cross the general UI snapshot boundary.
struct FermentationUiNetworkSource {
    device_platform::NetworkMode currentMode{
        device_platform::NetworkMode::UNSELECTED};
};

// Owning application state supplied to the projector. This is deliberately
// not an ApplicationStatusView; the projector alone creates that UI model.
struct FermentationUiApplicationSource {
    ApplicationLifecycleState lifecycleState{
        ApplicationLifecycleState::Initializing};
    PresentationState presentation;
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
