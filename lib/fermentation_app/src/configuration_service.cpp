#include "configuration_service.hpp"

#include <limits>
#include <utility>

#include "configuration_document_codec.hpp"
#include "configuration_limits.hpp"
#include "crc32.hpp"

namespace fermentation {

struct ConfigurationPreviewBuildLease::Candidate {
    std::shared_ptr<const UserConfiguration> userConfiguration;
    std::shared_ptr<const ServiceConfiguration> serviceConfiguration;
    std::shared_ptr<const ProgramCatalog> programCatalog;
};

struct ConfigurationService::Preview {
    ConfigurationPreviewView view;
    ChangeOrigin origin;
    ChangeOperation operation;
    std::shared_ptr<const ConfigurationPreviewBuildLease::Candidate> candidate;
    device_platform::PreparedTimeZone preparedTimeZone;
};

RuntimeConfigurationReadLease::RuntimeConfigurationReadLease(
    ConfigurationService& owner,
    std::shared_ptr<const RuntimeConfigurationSnapshot> snapshot) noexcept
    : owner_(&owner), snapshot_(std::move(snapshot)) {}

RuntimeConfigurationReadLease::~RuntimeConfigurationReadLease() { release(); }

RuntimeConfigurationReadLease::RuntimeConfigurationReadLease(
    RuntimeConfigurationReadLease&& other) noexcept
    : owner_(std::exchange(other.owner_, nullptr)),
      snapshot_(std::move(other.snapshot_)) {}

RuntimeConfigurationReadLease& RuntimeConfigurationReadLease::operator=(
    RuntimeConfigurationReadLease&& other) noexcept {
    if (this != &other) {
        release();
        owner_ = std::exchange(other.owner_, nullptr);
        snapshot_ = std::move(other.snapshot_);
    }
    return *this;
}

const RuntimeConfigurationSnapshot& RuntimeConfigurationReadLease::get() const {
    return *snapshot_;
}

const RuntimeConfigurationSnapshot* RuntimeConfigurationReadLease::operator->()
    const {
    return snapshot_.get();
}

void RuntimeConfigurationReadLease::release() noexcept {
    if (owner_ != nullptr) {
        owner_->releaseRuntimeLease(snapshot_);
        snapshot_.reset();
        owner_ = nullptr;
    }
}

ConfigurationPreviewBuildLease::ConfigurationPreviewBuildLease(
    ConfigurationService& owner, std::uint64_t expectedStateRevision,
    ConfigurationManifestReference expectedActive,
    std::shared_ptr<Candidate> candidate) noexcept
    : owner_(&owner),
      expectedStateRevision_(expectedStateRevision),
      expectedActive_(expectedActive),
      candidate_(std::move(candidate)) {}

ConfigurationPreviewBuildLease::~ConfigurationPreviewBuildLease() { release(); }

ConfigurationPreviewBuildLease::ConfigurationPreviewBuildLease(
    ConfigurationPreviewBuildLease&& other) noexcept
    : owner_(std::exchange(other.owner_, nullptr)),
      expectedStateRevision_(other.expectedStateRevision_),
      expectedActive_(other.expectedActive_),
      candidate_(std::move(other.candidate_)) {}

ConfigurationPreviewBuildLease& ConfigurationPreviewBuildLease::operator=(
    ConfigurationPreviewBuildLease&& other) noexcept {
    if (this != &other) {
        release();
        owner_ = std::exchange(other.owner_, nullptr);
        expectedStateRevision_ = other.expectedStateRevision_;
        expectedActive_ = other.expectedActive_;
        candidate_ = std::move(other.candidate_);
    }
    return *this;
}

bool ConfigurationPreviewBuildLease::replaceUserConfiguration(
    UserConfiguration configuration) {
    if (!valid()) {
        return false;
    }
    candidate_->userConfiguration =
        std::make_shared<const UserConfiguration>(std::move(configuration));
    return true;
}

bool ConfigurationPreviewBuildLease::replaceServiceConfiguration(
    ServiceConfiguration configuration) {
    if (!valid()) {
        return false;
    }
    candidate_->serviceConfiguration =
        std::make_shared<const ServiceConfiguration>(std::move(configuration));
    return true;
}

bool ConfigurationPreviewBuildLease::replaceProgramCatalog(
    ProgramCatalog catalog) {
    if (!valid()) {
        return false;
    }
    candidate_->programCatalog =
        std::make_shared<const ProgramCatalog>(std::move(catalog));
    return true;
}

void ConfigurationPreviewBuildLease::release() noexcept {
    if (owner_ != nullptr) {
        candidate_.reset();
        owner_->releasePreviewBuild(expectedStateRevision_);
        owner_ = nullptr;
    }
}

ConfigurationService::ConfigurationService(
    ConfigurationMutationCoordinator& mutationCoordinator,
    ConfigurationGraphStore& graphStore,
    const device_platform::ITimeZoneResolver& timeZoneResolver)
    : mutationCoordinator_(mutationCoordinator),
      graphStore_(graphStore),
      timeZoneResolver_(timeZoneResolver) {}

bool ConfigurationService::initialize(const LoadedConfigurationGraph& graph) {
    auto prepared = prepareSnapshot(graph, 1U);
    std::lock_guard<std::mutex> lock(stateMutex_);
    if (!prepared || activeRuntime_) {
        mode_ = ConfigurationServiceMode::RuntimeFailure;
        return false;
    }
    activeRuntime_ = std::move(prepared);
    nextRuntimeGeneration_ = 2U;
    mode_ = ConfigurationServiceMode::Operational;
    return true;
}

ConfigurationServiceMode ConfigurationService::mode() const {
    std::lock_guard<std::mutex> lock(stateMutex_);
    return mode_;
}

std::uint64_t ConfigurationService::stateRevision() const {
    std::lock_guard<std::mutex> lock(stateMutex_);
    return stateRevision_;
}

RuntimeConfigurationReadResult ConfigurationService::acquireRuntime() {
    std::lock_guard<std::mutex> lock(stateMutex_);
    if (mode_ != ConfigurationServiceMode::Operational || !activeRuntime_) {
        return {};
    }
    if (readLeaseCount_ >=
        configuration_limits::kMaxRuntimeConfigurationReadLeases) {
        RuntimeConfigurationReadResult result;
        result.status = RuntimeConfigurationReadStatus::RuntimeReadLeaseBusy;
        return result;
    }
    ++readLeaseCount_;
    RuntimeConfigurationReadResult result;
    result.status = RuntimeConfigurationReadStatus::RuntimeLeaseGranted;
    result.lease = RuntimeConfigurationReadLease(*this, activeRuntime_);
    return result;
}

ConfigurationPreviewBuildResult ConfigurationService::beginPreview() {
    std::lock_guard<std::mutex> lock(stateMutex_);
    ConfigurationPreviewBuildResult result;
    if (mode_ != ConfigurationServiceMode::Operational || !activeRuntime_) {
        result.status =
            ConfigurationPreviewStatus::ConfigurationRuntimeUnavailable;
        return result;
    }
    if (previewBuildReserved_ || previewModelReserved_ ||
        !retiredRuntime_.expired()) {
        result.status =
            ConfigurationPreviewStatus::ConfigurationModelBudgetBusy;
        return result;
    }
    previewBuildReserved_ = true;
    previewModelReserved_ = true;
    auto candidate =
        std::make_shared<ConfigurationPreviewBuildLease::Candidate>();
    candidate->userConfiguration = activeRuntime_->userConfiguration_;
    candidate->serviceConfiguration = activeRuntime_->serviceConfiguration_;
    candidate->programCatalog = activeRuntime_->programCatalog_;
    result.status = ConfigurationPreviewStatus::Success;
    result.lease = ConfigurationPreviewBuildLease(
        *this, stateRevision_, activeRuntime_->manifestReference_,
        std::move(candidate));
    return result;
}

ConfigurationPreviewInstallResult ConfigurationService::installPreview(
    ConfigurationPreviewBuildLease&& buildLease, ChangeOrigin origin,
    ChangeOperation operation) {
    ConfigurationPreviewInstallResult result;
    if (buildLease.owner_ != this || !buildLease.candidate_) {
        result.status = ConfigurationPreviewStatus::InvalidCandidate;
        return result;
    }
    auto candidate = buildLease.candidate_;
    auto userValidation = validateUserConfiguration(
        *candidate->userConfiguration, timeZoneResolver_);
    if (userValidation.status != UserConfigurationStatus::Success ||
        !userValidation.preparedTimeZone.has_value() ||
        validateProgramCatalog(*candidate->programCatalog) !=
            ProgramCatalogStatus::Success) {
        result.status = ConfigurationPreviewStatus::InvalidCandidate;
        return result;
    }

    std::string candidateUser;
    std::string candidateService;
    std::string candidatePrograms;
    std::string activeUser;
    std::string activeService;
    std::string activePrograms;
    if (encodeUserConfigurationPayload(*candidate->userConfiguration,
                                       timeZoneResolver_, candidateUser) !=
            ConfigurationCodecStatus::Success ||
        encodeServiceConfigurationPayload(*candidate->serviceConfiguration,
                                          candidateService) !=
            ConfigurationCodecStatus::Success ||
        encodeProgramCatalogPayload(*candidate->programCatalog,
                                    candidatePrograms) !=
            ConfigurationCodecStatus::Success) {
        result.status = ConfigurationPreviewStatus::InvalidCandidate;
        return result;
    }

    std::lock_guard<std::mutex> lock(stateMutex_);
    if (mode_ != ConfigurationServiceMode::Operational ||
        stateRevision_ != buildLease.expectedStateRevision_ ||
        !activeRuntime_ ||
        activeRuntime_->manifestReference_ != buildLease.expectedActive_) {
        result.status = ConfigurationPreviewStatus::StateChanged;
        return result;
    }
    if (encodeUserConfigurationPayload(activeRuntime_->userConfiguration(),
                                       timeZoneResolver_, activeUser) !=
            ConfigurationCodecStatus::Success ||
        encodeServiceConfigurationPayload(
            activeRuntime_->serviceConfiguration(), activeService) !=
            ConfigurationCodecStatus::Success ||
        encodeProgramCatalogPayload(activeRuntime_->programCatalog(),
                                    activePrograms) !=
            ConfigurationCodecStatus::Success) {
        mode_ = ConfigurationServiceMode::RuntimeFailure;
        (void)incrementStateRevisionLocked();
        clearPreviewLocked();
        result.status =
            ConfigurationPreviewStatus::ConfigurationRuntimeUnavailable;
        return result;
    }
    if (nextPreviewHandle_ == std::numeric_limits<std::uint64_t>::max()) {
        mode_ = ConfigurationServiceMode::RuntimeFailure;
        (void)incrementStateRevisionLocked();
        clearPreviewLocked();
        result.status =
            ConfigurationPreviewStatus::ConfigurationRuntimeUnavailable;
        return result;
    }
    const ConfigurationChangeMask changes{candidateUser != activeUser,
                                          candidateService != activeService,
                                          candidatePrograms != activePrograms};
    const bool noChange = !changes.userConfiguration &&
                          !changes.serviceConfiguration &&
                          !changes.programCatalog;
    ConfigurationPreviewView view{
        nextPreviewHandle_++,
        buildLease.expectedActive_,
        changes,
        {static_cast<std::uint32_t>(candidateUser.size()),
         device_platform::computeCrc32IsoHdlc(candidateUser),
         static_cast<std::uint32_t>(candidateService.size()),
         device_platform::computeCrc32IsoHdlc(candidateService),
         static_cast<std::uint32_t>(candidatePrograms.size()),
         device_platform::computeCrc32IsoHdlc(candidatePrograms)},
        ConfigurationActivationEffect::Immediate,
        noChange};
    if (noChange) {
        candidate.reset();
        candidate =
            std::make_shared<ConfigurationPreviewBuildLease::Candidate>();
        candidate->userConfiguration = activeRuntime_->userConfiguration_;
        candidate->serviceConfiguration = activeRuntime_->serviceConfiguration_;
        candidate->programCatalog = activeRuntime_->programCatalog_;
        previewModelReserved_ = false;
    }
    visiblePreview_ = std::make_shared<const Preview>(
        Preview{view, origin, operation, std::move(candidate),
                std::move(*userValidation.preparedTimeZone)});
    previewBuildReserved_ = false;
    buildLease.candidate_.reset();
    buildLease.owner_ = nullptr;
    result.status = ConfigurationPreviewStatus::Success;
    result.preview = view;
    return result;
}

std::optional<ConfigurationPreviewView> ConfigurationService::visiblePreview()
    const {
    std::lock_guard<std::mutex> lock(stateMutex_);
    if (!visiblePreview_) {
        return std::nullopt;
    }
    return visiblePreview_->view;
}

ConfigurationPreviewStatus ConfigurationService::cancelPreview(
    std::uint64_t handle) {
    std::lock_guard<std::mutex> lock(stateMutex_);
    if (!visiblePreview_) {
        return ConfigurationPreviewStatus::PreviewNotFound;
    }
    if (visiblePreview_->view.handle != handle) {
        return ConfigurationPreviewStatus::PreviewSuperseded;
    }
    clearPreviewLocked();
    return ConfigurationPreviewStatus::Success;
}

std::size_t ConfigurationService::activeReadLeaseCount() const {
    std::lock_guard<std::mutex> lock(stateMutex_);
    return readLeaseCount_;
}

std::size_t ConfigurationService::fullModelGenerationCount() const {
    std::lock_guard<std::mutex> lock(stateMutex_);
    return activeRuntime_
               ? 1U + static_cast<std::size_t>(previewModelReserved_ ||
                                               !retiredRuntime_.expired())
               : 0U;
}

void ConfigurationService::releaseRuntimeLease(
    const std::shared_ptr<const RuntimeConfigurationSnapshot>&
        snapshot) noexcept {
    std::lock_guard<std::mutex> lock(stateMutex_);
    if (readLeaseCount_ > 0U) {
        --readLeaseCount_;
    }
    if (snapshot && activeRuntime_ &&
        snapshot->volatileGenerationId() !=
            activeRuntime_->volatileGenerationId() &&
        snapshot.use_count() <= 2) {
        retiredRuntime_.reset();
    }
}

void ConfigurationService::releasePreviewBuild(
    std::uint64_t expectedStateRevision) noexcept {
    std::lock_guard<std::mutex> lock(stateMutex_);
    if (previewBuildReserved_) {
        previewBuildReserved_ = false;
        previewModelReserved_ = false;
    }
    (void)expectedStateRevision;
}

bool ConfigurationService::incrementStateRevisionLocked() noexcept {
    if (stateRevision_ == std::numeric_limits<std::uint64_t>::max()) {
        mode_ = ConfigurationServiceMode::RuntimeFailure;
        return false;
    }
    ++stateRevision_;
    return true;
}

std::shared_ptr<RuntimeConfigurationSnapshot>
ConfigurationService::prepareSnapshot(const LoadedConfigurationGraph& graph,
                                      std::uint64_t generationId) const {
    const auto prepared = validateUserConfiguration(
        *graph.active.userConfiguration, timeZoneResolver_);
    if (prepared.status != UserConfigurationStatus::Success ||
        !prepared.preparedTimeZone.has_value()) {
        return {};
    }
    return std::shared_ptr<RuntimeConfigurationSnapshot>(
        new RuntimeConfigurationSnapshot(
            graph.active.manifestReference.storageEpoch,
            graph.active.manifestReference,
            graph.active.manifest.userConfiguration.version,
            graph.active.manifest.serviceConfiguration.version,
            graph.active.manifest.programCatalog.version,
            graph.active.userConfiguration, graph.active.serviceConfiguration,
            graph.active.programCatalog, *prepared.preparedTimeZone,
            generationId));
}

void ConfigurationService::clearPreviewLocked() {
    if (visiblePreview_ && !visiblePreview_->view.noChange) {
        previewModelReserved_ = false;
    }
    visiblePreview_.reset();
}

}  // namespace fermentation
