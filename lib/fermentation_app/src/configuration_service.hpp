#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <string>

#include "configuration_graph.hpp"
#include "configuration_graph_store.hpp"
#include "configuration_mutation_coordinator.hpp"
#include "runtime_configuration_snapshot.hpp"
#include "time_zone_resolver.hpp"

namespace fermentation {

enum class ConfigurationServiceMode : std::uint8_t {
    Operational,
    CommitInProgress,
    CommitIndeterminate,
    RuntimeFailure,
};

enum class RuntimeConfigurationReadStatus : std::uint8_t {
    RuntimeLeaseGranted,
    RuntimeReadLeaseBusy,
    ConfigurationRuntimeUnavailable,
};

class RuntimeConfigurationReadLease {
   public:
    RuntimeConfigurationReadLease() = default;
    ~RuntimeConfigurationReadLease();
    RuntimeConfigurationReadLease(const RuntimeConfigurationReadLease&) =
        delete;
    RuntimeConfigurationReadLease& operator=(
        const RuntimeConfigurationReadLease&) = delete;
    RuntimeConfigurationReadLease(
        RuntimeConfigurationReadLease&& other) noexcept;
    RuntimeConfigurationReadLease& operator=(
        RuntimeConfigurationReadLease&& other) noexcept;

    [[nodiscard]] const RuntimeConfigurationSnapshot& get() const;
    [[nodiscard]] const RuntimeConfigurationSnapshot* operator->() const;
    [[nodiscard]] bool valid() const noexcept { return owner_ != nullptr; }

   private:
    friend class ConfigurationService;
    RuntimeConfigurationReadLease(
        ConfigurationService& owner,
        std::shared_ptr<const RuntimeConfigurationSnapshot> snapshot,
        std::uint64_t generationId) noexcept;
    void release() noexcept;

    ConfigurationService* owner_{nullptr};
    std::shared_ptr<const RuntimeConfigurationSnapshot> snapshot_;
    std::uint64_t generationId_{0U};
};

struct RuntimeConfigurationReadResult {
    RuntimeConfigurationReadStatus status{
        RuntimeConfigurationReadStatus::ConfigurationRuntimeUnavailable};
    RuntimeConfigurationReadLease lease;
};

enum class ConfigurationPreviewStatus : std::uint8_t {
    Success,
    ConfigurationRuntimeUnavailable,
    ConfigurationModelBudgetBusy,
    InvalidCandidate,
    StateChanged,
    PreviewNotFound,
    PreviewSuperseded,
};

enum class ConfigurationActivationEffect : std::uint8_t { Immediate };

struct ConfigurationCandidateIntegrity {
    std::uint32_t userSchema{0U};
    std::uint32_t userPayloadLength{0U};
    std::uint32_t userPayloadCrc{0U};
    std::uint32_t serviceSchema{0U};
    std::uint32_t servicePayloadLength{0U};
    std::uint32_t servicePayloadCrc{0U};
    std::uint32_t programSchema{0U};
    std::uint32_t programPayloadLength{0U};
    std::uint32_t programPayloadCrc{0U};
};

struct ConfigurationChangeSummary {
    bool displayLanguageChanged{false};
    bool timeZoneChanged{false};
    bool deviceNameChanged{false};
    std::uint16_t programsAdded{0U};
    std::uint16_t programsRemoved{0U};
    std::uint16_t programsModified{0U};
};

struct ConfigurationPreviewView {
    std::uint64_t handle{0U};
    ConfigurationManifestReference expectedActive;
    ConfigurationChangeMask changes;
    ConfigurationCandidateIntegrity integrity;
    ConfigurationChangeSummary summary;
    ConfigurationActivationEffect activationEffect{
        ConfigurationActivationEffect::Immediate};
    bool noChange{false};
};

class ConfigurationPreviewBuildLease {
   public:
    struct Candidate;
    ConfigurationPreviewBuildLease() = default;
    ~ConfigurationPreviewBuildLease();
    ConfigurationPreviewBuildLease(const ConfigurationPreviewBuildLease&) =
        delete;
    ConfigurationPreviewBuildLease& operator=(
        const ConfigurationPreviewBuildLease&) = delete;
    ConfigurationPreviewBuildLease(
        ConfigurationPreviewBuildLease&& other) noexcept;
    ConfigurationPreviewBuildLease& operator=(
        ConfigurationPreviewBuildLease&& other) noexcept;

    [[nodiscard]] bool valid() const noexcept { return owner_ != nullptr; }
    [[nodiscard]] UserConfiguration& userConfiguration();
    [[nodiscard]] ServiceConfiguration& serviceConfiguration();
    [[nodiscard]] ProgramCatalog& programCatalog();

   private:
    friend class ConfigurationService;
    ConfigurationPreviewBuildLease(
        ConfigurationService& owner, std::uint64_t reservationId,
        std::uint64_t expectedStateRevision,
        ConfigurationManifestReference expectedActive,
        std::shared_ptr<Candidate> candidate) noexcept;
    void release() noexcept;

    ConfigurationService* owner_{nullptr};
    std::uint64_t reservationId_{0U};
    std::uint64_t expectedStateRevision_{0U};
    ConfigurationManifestReference expectedActive_;
    std::shared_ptr<Candidate> candidate_;
};

struct ConfigurationPreviewBuildResult {
    ConfigurationPreviewStatus status{
        ConfigurationPreviewStatus::ConfigurationRuntimeUnavailable};
    ConfigurationPreviewBuildLease lease;
};

struct ConfigurationPreviewInstallResult {
    ConfigurationPreviewStatus status{
        ConfigurationPreviewStatus::InvalidCandidate};
    std::optional<ConfigurationPreviewView> preview;
};

enum class ConfigurationCommitStatus : std::uint8_t {
    Activated,
    NoChange,
    PreviewNotFound,
    PreviewSuperseded,
    ConfigurationMutationBusy,
    ConfigurationConflictFailure,
    ConfigurationValidationFailure,
    PersistenceFailure,
    CapacityFailure,
    ConfigurationCommitIndeterminate,
    ConfigurationRuntimeFailure,
};

struct ConfigurationCommitResult {
    ConfigurationCommitStatus status{
        ConfigurationCommitStatus::ConfigurationRuntimeFailure};
};

enum class ConfigurationRuntimeFailureCause : std::uint8_t {
    PersistentGraphVerificationFailure,
    PersistentGraphIntegrityFailure,
    UnsupportedNewerConfigurationSchema,
    PersistentStoreReadFailure,
    PostCommitVerificationFailure,
    RuntimePreparationAfterResolutionFailure,
    PublishContractViolation,
    ServiceStateInvariantViolation,
    ConfigurationModelBudgetInvariantViolation,
    PersistentConfigurationIdentityCollision,
};

enum class ConfigurationCommitIndeterminateCause : std::uint8_t {
    RootReadError,
    RootCapacityError,
    GraphReadError,
    GraphCapacityError,
    GraphEnvelopeOrCrcFailure,
    GraphReferenceFailure,
    GraphSemanticFailure,
    GraphIntegrityFailure,
    AmbiguousRootOutcome,
};

class ConfigurationService {
   public:
    ConfigurationService(
        ConfigurationMutationCoordinator& mutationCoordinator,
        ConfigurationGraphStore& graphStore,
        const device_platform::ITimeZoneResolver& timeZoneResolver);
    ~ConfigurationService();
    ConfigurationService(const ConfigurationService&) = delete;
    ConfigurationService& operator=(const ConfigurationService&) = delete;
    ConfigurationService(ConfigurationService&&) = delete;
    ConfigurationService& operator=(ConfigurationService&&) = delete;

    [[nodiscard]] bool initialize(const LoadedConfigurationGraph& graph);
    [[nodiscard]] ConfigurationServiceMode mode() const;
    [[nodiscard]] std::uint64_t stateRevision() const;
    [[nodiscard]] RuntimeConfigurationReadResult acquireRuntime();
    [[nodiscard]] ConfigurationPreviewBuildResult beginPreview();
    [[nodiscard]] ConfigurationPreviewInstallResult installPreview(
        ConfigurationPreviewBuildLease&& buildLease, ChangeOrigin origin,
        ChangeOperation operation);
    [[nodiscard]] std::optional<ConfigurationPreviewView> visiblePreview()
        const;
    [[nodiscard]] ConfigurationPreviewStatus cancelPreview(
        std::uint64_t handle);
    [[nodiscard]] ConfigurationCommitResult confirmPreview(
        std::uint64_t handle);
    [[nodiscard]] ConfigurationCommitResolutionStatus resolveIndeterminate();
    [[nodiscard]] ConfigurationCommitResolutionStatus recoverRuntimeFailure();
    [[nodiscard]] std::optional<ConfigurationCommitIndeterminateCause>
    commitIndeterminateCause() const;
    [[nodiscard]] std::optional<ConfigurationRuntimeFailureCause>
    runtimeFailureCause() const;
    [[nodiscard]] std::size_t activeReadLeaseCount() const;
    [[nodiscard]] std::size_t fullModelGenerationCount() const;

   private:
    friend class RuntimeConfigurationReadLease;
    friend class ConfigurationPreviewBuildLease;
    friend class ConfigurationServiceTestAccess;

    struct Preview;
    struct ResolutionContext;
    enum class TestPoint : std::uint8_t {
        PreviewCaptured,
        BeforeRetirementRelease,
        BeforeResolutionContextRelease,
        PreviewBeforeInstall,
        BeforePublish,
        BeforeRuntimePreparation,
    };
    using TestHook = void (*)(void*, TestPoint);
    void releaseRuntimeLease(std::uint64_t generationId) noexcept;
    void releasePreviewBuild(std::uint64_t reservationId) noexcept;
    [[nodiscard]] bool incrementStateRevisionLocked() noexcept;
    [[nodiscard]] bool stateRevisionHasHeadroomLocked(
        std::uint64_t steps) const noexcept;
    [[nodiscard]] std::shared_ptr<RuntimeConfigurationSnapshot> prepareSnapshot(
        const LoadedConfigurationGraph& graph,
        std::uint64_t generationId) const;
    void clearPreviewLocked();
    void enterFailClosedLocked(ConfigurationServiceMode mode,
                               ConfigurationRuntimeFailureCause cause);
    void publishPreparedLocked(
        PreparedConfigurationCommit& persistent,
        std::unique_ptr<LoadedConfigurationGraph>& preparedGraph,
        std::shared_ptr<const RuntimeConfigurationSnapshot> preparedRuntime,
        std::shared_ptr<const RuntimeConfigurationSnapshot>& retiredRuntime,
        std::unique_ptr<LoadedConfigurationGraph>& retiredGraph) noexcept;
    [[nodiscard]] bool completeRuntimeRetirement(
        std::uint64_t generationId) noexcept;
    void invokeTestHook(TestPoint point) const;
    void invalidateRuntimePreparationBindingForTest();
    void rejectRuntimePreparationForTest(bool reject) noexcept;
    [[nodiscard]] bool runtimePreparationRetryConsumedForTest() const noexcept;

    ConfigurationMutationCoordinator& mutationCoordinator_;
    ConfigurationGraphStore& graphStore_;
    const device_platform::ITimeZoneResolver& timeZoneResolver_;
    mutable std::mutex stateMutex_;
    ConfigurationServiceMode mode_{ConfigurationServiceMode::RuntimeFailure};
    std::uint64_t stateRevision_{1U};
    std::uint64_t nextPreviewHandle_{1U};
    std::uint64_t nextPreviewBuildReservation_{1U};
    std::uint64_t nextRuntimeGeneration_{1U};
    std::shared_ptr<const RuntimeConfigurationSnapshot> activeRuntime_;
    std::unique_ptr<LoadedConfigurationGraph> currentGraph_;
    std::shared_ptr<const Preview> visiblePreview_;
    std::shared_ptr<const Preview> capturedPreview_;
    std::size_t readLeaseCount_{0U};
    std::size_t activeGenerationReadLeases_{0U};
    std::optional<std::uint64_t> retiredGenerationId_;
    std::size_t retiredGenerationReadLeases_{0U};
    bool retirementOwnerPending_{false};
    std::optional<std::uint64_t> previewBuildReservation_;
    bool previewBuildRevoked_{false};
    bool previewModelReserved_{false};
    std::unique_ptr<ResolutionContext> resolutionContext_;
    std::optional<ConfigurationRuntimeFailureCause> runtimeFailureCause_;
    void* testHookContext_{nullptr};
    TestHook testHook_{nullptr};
    std::atomic<bool> rejectRuntimePreparationForTest_{false};
};

}  // namespace fermentation
