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
#include "factory_reset_flow.hpp"
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

enum class RunDetailRemainingKind : std::uint8_t {
    Timed,
    Unknown,
    Unbounded,
    NotApplicable,
};

struct RunDetailCompletion {
    CompletionMode mode{CompletionMode::FinishWithoutCooling};
    std::optional<double> coolingTargetCelsius;
    std::optional<std::uint32_t> holdDurationMinutes;
};

// Phase-scoped time (plan D2-A): never a whole-run clock. `elapsedSeconds` is
// empty whenever the exact phase time cannot be derived.
struct RunDetailTime {
    std::optional<std::uint64_t> elapsedSeconds;
    std::optional<std::uint64_t> remainingSeconds;
    RunDetailRemainingKind remainingKind{RunDetailRemainingKind::Unknown};
};

// Read-only run information from the frozen run source. The program name is
// kept in FermentationHomeView::programName (empty when the source has none).
struct FermentationRunDetail {
    // Empty for a pure manual hold.
    std::optional<ProgramSourceKind> sourceKind;
    std::optional<double> targetCelsius;
    std::optional<RunDetailCompletion> completion;
    RunDetailTime time;
};

struct FermentationHomeView {
    FermentationHomeMode mode{FermentationHomeMode::Unavailable};
    ProcessState processState{ProcessState::Boot};
    std::string activeRunId;
    // Name of the frozen run program (<= 96 bytes); empty for no name.
    std::string programName;
    std::optional<EffectiveRunValues> effectiveValues;
    std::optional<FermentationRunDetail> runDetail;
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

// Secret-free state of the local release for the web first-time setup, as
// reported by the Application owner. The UI never models the window itself.
enum class FermentationWebAccessState : std::uint8_t {
    // Web access is already set up or not in a state to be released.
    NotApplicable,
    // Setup is possible but the local release window is closed.
    Closed,
    // The local release window is open.
    WindowOpen,
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
    FermentationWebAccessState webAccess{
        FermentationWebAccessState::NotApplicable};
    FermentationFactoryResetView factoryReset;
    std::optional<device_platform::UiRefreshRevision> refreshRevision;
};

// Renderer-independent presentation input outside the run/fault/recovery
// snapshot: it changes only with configuration, not with process state, so
// it is read separately rather than folded into FermentationUiSnapshot. This
// is the sole source for these values; a local renderer/composition helper
// never re-reads UserConfiguration, the runtime program catalog, or the
// prepared time zone on its own.
struct FermentationUiPresentationSource {
    device_platform::LocaleId displayLocale{"en"};
    device_platform::TimeZoneId canonicalTimeZoneId;
    // Rule of the already prepared zone (RuntimeConfigurationSnapshot::
    // preparedTimeZone()), so no consumer resolves the zone again.
    device_platform::TimeZoneRule timeZoneRule{};
    // Visible device name (UserConfiguration), read-only display copy.
    std::string deviceName;
    ProgramCatalog programCatalog;
    // Revision of the same lease the catalog was read from.
    ProgramCatalogRevision programCatalogRevision;
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
