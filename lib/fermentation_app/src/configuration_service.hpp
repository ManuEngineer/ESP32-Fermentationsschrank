#pragma once

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
        std::shared_ptr<const RuntimeConfigurationSnapshot> snapshot) noexcept;
    void release() noexcept;

    ConfigurationService* owner_{nullptr};
    std::shared_ptr<const RuntimeConfigurationSnapshot> snapshot_;
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
    std::uint32_t userPayloadLength{0U};
    std::uint32_t userPayloadCrc{0U};
    std::uint32_t servicePayloadLength{0U};
    std::uint32_t servicePayloadCrc{0U};
    std::uint32_t programPayloadLength{0U};
    std::uint32_t programPayloadCrc{0U};
};

struct ConfigurationPreviewView {
    std::uint64_t handle{0U};
    ConfigurationManifestReference expectedActive;
    ConfigurationChangeMask changes;
    ConfigurationCandidateIntegrity integrity;
    ConfigurationActivationEffect activationEffect{
        ConfigurationActivationEffect::Immediate};
    bool noChange{false};
};

class ConfigurationPreviewBuildLease {
   public:
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
    [[nodiscard]] bool replaceUserConfiguration(
        UserConfiguration configuration);
    [[nodiscard]] bool replaceServiceConfiguration(
        ServiceConfiguration configuration);
    [[nodiscard]] bool replaceProgramCatalog(ProgramCatalog catalog);

   private:
    friend class ConfigurationService;
    struct Candidate;
    ConfigurationPreviewBuildLease(
        ConfigurationService& owner, std::uint64_t expectedStateRevision,
        ConfigurationManifestReference expectedActive,
        std::shared_ptr<Candidate> candidate) noexcept;
    void release() noexcept;

    ConfigurationService* owner_{nullptr};
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

class ConfigurationService {
   public:
    ConfigurationService(
        ConfigurationMutationCoordinator& mutationCoordinator,
        ConfigurationGraphStore& graphStore,
        const device_platform::ITimeZoneResolver& timeZoneResolver);

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
    [[nodiscard]] std::size_t activeReadLeaseCount() const;
    [[nodiscard]] std::size_t fullModelGenerationCount() const;

   private:
    friend class RuntimeConfigurationReadLease;
    friend class ConfigurationPreviewBuildLease;

    struct Preview;
    void releaseRuntimeLease(
        const std::shared_ptr<const RuntimeConfigurationSnapshot>&
            snapshot) noexcept;
    void releasePreviewBuild(std::uint64_t expectedStateRevision) noexcept;
    [[nodiscard]] bool incrementStateRevisionLocked() noexcept;
    [[nodiscard]] std::shared_ptr<RuntimeConfigurationSnapshot> prepareSnapshot(
        const LoadedConfigurationGraph& graph,
        std::uint64_t generationId) const;
    void clearPreviewLocked();

    ConfigurationMutationCoordinator& mutationCoordinator_;
    ConfigurationGraphStore& graphStore_;
    const device_platform::ITimeZoneResolver& timeZoneResolver_;
    mutable std::mutex stateMutex_;
    ConfigurationServiceMode mode_{ConfigurationServiceMode::RuntimeFailure};
    std::uint64_t stateRevision_{1U};
    std::uint64_t nextPreviewHandle_{1U};
    std::uint64_t nextRuntimeGeneration_{1U};
    std::shared_ptr<const RuntimeConfigurationSnapshot> activeRuntime_;
    std::weak_ptr<const RuntimeConfigurationSnapshot> retiredRuntime_;
    std::shared_ptr<const Preview> visiblePreview_;
    std::size_t readLeaseCount_{0U};
    bool previewBuildReserved_{false};
    bool previewModelReserved_{false};
};

}  // namespace fermentation
