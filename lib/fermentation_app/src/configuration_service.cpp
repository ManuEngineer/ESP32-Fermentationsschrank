#include "configuration_service.hpp"

#include <algorithm>
#include <limits>
#include <utility>

#include "configuration_document_codec.hpp"
#include "configuration_limits.hpp"
#include "crc32.hpp"

namespace fermentation {

struct ConfigurationPreviewBuildLease::Candidate {
    std::shared_ptr<UserConfiguration> userConfiguration;
    std::shared_ptr<ServiceConfiguration> serviceConfiguration;
    std::shared_ptr<ProgramCatalog> programCatalog;
    std::shared_ptr<const UserConfiguration> baseUserConfiguration;
    std::shared_ptr<const ServiceConfiguration> baseServiceConfiguration;
    std::shared_ptr<const ProgramCatalog> baseProgramCatalog;
};

struct ConfigurationService::Preview {
    ConfigurationPreviewView view;
    ChangeOrigin origin;
    ChangeOperation operation;
    std::uint64_t installationStateRevision{0U};
    std::shared_ptr<const ConfigurationPreviewBuildLease::Candidate> candidate;
    device_platform::PreparedTimeZone preparedTimeZone;
};

struct RuntimePreparationBinding {
    device_platform::StorageEpoch storageEpoch;
    ConfigurationRootSequence rootSequence;
    ConfigurationManifestReference manifestReference;
    UserConfigurationReference userConfiguration;
    ServiceConfigurationReference serviceConfiguration;
    ProgramCatalogReference programCatalog;
    std::string preparedTimeZoneIdentifier;
};

struct ConfigurationService::ResolutionContext {
    PreparedConfigurationCommit persistent;
    std::unique_ptr<LoadedConfigurationGraph> preparedGraph;
    std::shared_ptr<const RuntimeConfigurationSnapshot> preparedRuntime;
    RuntimePreparationBinding runtimeBinding;
    ConfigurationCommitIndeterminateCause indeterminateCause{
        ConfigurationCommitIndeterminateCause::AmbiguousRootOutcome};
    bool runtimePreparationRetryConsumed{false};
};

namespace {

RuntimePreparationBinding makeRuntimeBinding(
    const LoadedConfigurationGraph& graph,
    const RuntimeConfigurationSnapshot& snapshot) {
    return {graph.active.manifestReference.storageEpoch,
            graph.rootSequence,
            graph.active.manifestReference,
            graph.active.manifest.userConfiguration,
            graph.active.manifest.serviceConfiguration,
            graph.active.manifest.programCatalog,
            snapshot.preparedTimeZone().canonicalIdentifier};
}

bool runtimeBindingMatches(const RuntimePreparationBinding& binding,
                           const LoadedConfigurationGraph& graph,
                           const RuntimeConfigurationSnapshot& snapshot) {
    return binding.storageEpoch ==
               graph.active.manifestReference.storageEpoch &&
           binding.rootSequence == graph.rootSequence &&
           binding.manifestReference == graph.active.manifestReference &&
           binding.userConfiguration ==
               graph.active.manifest.userConfiguration &&
           binding.serviceConfiguration ==
               graph.active.manifest.serviceConfiguration &&
           binding.programCatalog == graph.active.manifest.programCatalog &&
           snapshot.storageEpoch() == binding.storageEpoch &&
           snapshot.manifestReference() == binding.manifestReference &&
           snapshot.userConfigurationRevision() ==
               binding.userConfiguration.version &&
           snapshot.serviceConfigurationRevision() ==
               binding.serviceConfiguration.version &&
           snapshot.programCatalogRevision() ==
               binding.programCatalog.version &&
           configurationContentEquals(*graph.active.userConfiguration,
                                      snapshot.userConfiguration()) &&
           configurationContentEquals(*graph.active.serviceConfiguration,
                                      snapshot.serviceConfiguration()) &&
           configurationContentEquals(*graph.active.programCatalog,
                                      snapshot.programCatalog()) &&
           snapshot.preparedTimeZone().canonicalIdentifier ==
               binding.preparedTimeZoneIdentifier;
}

bool runtimeSnapshotMatchesGraph(const LoadedConfigurationGraph& graph,
                                 const RuntimeConfigurationSnapshot& snapshot) {
    return snapshot.storageEpoch() ==
               graph.active.manifestReference.storageEpoch &&
           snapshot.manifestReference() == graph.active.manifestReference &&
           snapshot.userConfigurationRevision() ==
               graph.active.manifest.userConfiguration.version &&
           snapshot.serviceConfigurationRevision() ==
               graph.active.manifest.serviceConfiguration.version &&
           snapshot.programCatalogRevision() ==
               graph.active.manifest.programCatalog.version &&
           configurationContentEquals(*graph.active.userConfiguration,
                                      snapshot.userConfiguration()) &&
           configurationContentEquals(*graph.active.serviceConfiguration,
                                      snapshot.serviceConfiguration()) &&
           configurationContentEquals(*graph.active.programCatalog,
                                      snapshot.programCatalog()) &&
           snapshot.preparedTimeZone().canonicalIdentifier ==
               graph.active.userConfiguration->timeZoneId;
}

ConfigurationCommitIndeterminateCause mapIndeterminateCause(
    ConfigurationCommitResolutionCause cause) {
    switch (cause) {
        case ConfigurationCommitResolutionCause::RootReadError:
            return ConfigurationCommitIndeterminateCause::RootReadError;
        case ConfigurationCommitResolutionCause::RootCapacityError:
            return ConfigurationCommitIndeterminateCause::RootCapacityError;
        case ConfigurationCommitResolutionCause::GraphReadError:
            return ConfigurationCommitIndeterminateCause::GraphReadError;
        case ConfigurationCommitResolutionCause::GraphCapacityError:
            return ConfigurationCommitIndeterminateCause::GraphCapacityError;
        case ConfigurationCommitResolutionCause::GraphEnvelopeOrCrcFailure:
            return ConfigurationCommitIndeterminateCause::
                GraphEnvelopeOrCrcFailure;
        case ConfigurationCommitResolutionCause::GraphReferenceFailure:
            return ConfigurationCommitIndeterminateCause::GraphReferenceFailure;
        case ConfigurationCommitResolutionCause::GraphSemanticFailure:
            return ConfigurationCommitIndeterminateCause::GraphSemanticFailure;
        case ConfigurationCommitResolutionCause::GraphIntegrityFailure:
            return ConfigurationCommitIndeterminateCause::GraphIntegrityFailure;
        case ConfigurationCommitResolutionCause::IdentityCollision:
        case ConfigurationCommitResolutionCause::UnsupportedNewerSchema:
        case ConfigurationCommitResolutionCause::None:
        case ConfigurationCommitResolutionCause::AmbiguousRootOutcome:
            return ConfigurationCommitIndeterminateCause::AmbiguousRootOutcome;
    }
    return ConfigurationCommitIndeterminateCause::AmbiguousRootOutcome;
}

ConfigurationChangeSummary summarizeChanges(
    const ConfigurationPreviewBuildLease::Candidate& candidate) {
    ConfigurationChangeSummary summary;
    summary.displayLanguageChanged =
        candidate.userConfiguration->displayLanguageId !=
        candidate.baseUserConfiguration->displayLanguageId;
    summary.activeThemeChanged =
        candidate.userConfiguration->activeThemeId !=
        candidate.baseUserConfiguration->activeThemeId;
    summary.timeZoneChanged = candidate.userConfiguration->timeZoneId !=
                              candidate.baseUserConfiguration->timeZoneId;
    summary.deviceNameChanged = candidate.userConfiguration->deviceName !=
                                candidate.baseUserConfiguration->deviceName;
    for (const auto& program : candidate.programCatalog->programs) {
        const auto found =
            std::find_if(candidate.baseProgramCatalog->programs.begin(),
                         candidate.baseProgramCatalog->programs.end(),
                         [&program](const ProgramDocument& entry) {
                             return entry.program.id == program.program.id;
                         });
        if (found == candidate.baseProgramCatalog->programs.end()) {
            ++summary.programsAdded;
        } else {
            const ProgramCatalog left{{program}};
            const ProgramCatalog right{{*found}};
            if (!configurationContentEquals(left, right)) {
                ++summary.programsModified;
            }
        }
    }
    for (const auto& program : candidate.baseProgramCatalog->programs) {
        const auto found =
            std::find_if(candidate.programCatalog->programs.begin(),
                         candidate.programCatalog->programs.end(),
                         [&program](const ProgramDocument& entry) {
                             return entry.program.id == program.program.id;
                         });
        if (found == candidate.programCatalog->programs.end()) {
            ++summary.programsRemoved;
        }
    }
    return summary;
}

bool calculateCandidateIntegrity(
    const ConfigurationPreviewBuildLease::Candidate& candidate,
    const device_platform::ITimeZoneResolver& resolver,
    ConfigurationCandidateIntegrity& integrity) {
    std::string payload;
    if (encodeUserConfigurationPayload(
            *candidate.userConfiguration,
            kCurrentUserConfigurationSchemaVersion, resolver, payload) !=
        ConfigurationCodecStatus::Success) {
        return false;
    }
    integrity.userSchema = kCurrentUserConfigurationSchemaVersion;
    integrity.userPayloadLength = static_cast<std::uint32_t>(payload.size());
    integrity.userPayloadCrc = device_platform::computeCrc32IsoHdlc(payload);
    payload.clear();
    if (encodeServiceConfigurationPayload(*candidate.serviceConfiguration,
                                          payload) !=
        ConfigurationCodecStatus::Success) {
        return false;
    }
    integrity.serviceSchema = 1U;
    integrity.servicePayloadLength = static_cast<std::uint32_t>(payload.size());
    integrity.servicePayloadCrc = device_platform::computeCrc32IsoHdlc(payload);
    payload.clear();
    if (encodeProgramCatalogPayload(*candidate.programCatalog, payload) !=
        ConfigurationCodecStatus::Success) {
        return false;
    }
    integrity.programSchema = 1U;
    integrity.programPayloadLength = static_cast<std::uint32_t>(payload.size());
    integrity.programPayloadCrc = device_platform::computeCrc32IsoHdlc(payload);
    return true;
}

bool candidateIntegrityEquals(const ConfigurationCandidateIntegrity& left,
                              const ConfigurationCandidateIntegrity& right) {
    return left.userSchema == right.userSchema &&
           left.userPayloadLength == right.userPayloadLength &&
           left.userPayloadCrc == right.userPayloadCrc &&
           left.serviceSchema == right.serviceSchema &&
           left.servicePayloadLength == right.servicePayloadLength &&
           left.servicePayloadCrc == right.servicePayloadCrc &&
           left.programSchema == right.programSchema &&
           left.programPayloadLength == right.programPayloadLength &&
           left.programPayloadCrc == right.programPayloadCrc;
}

}  // namespace

RuntimeConfigurationReadLease::RuntimeConfigurationReadLease(
    ConfigurationService& owner,
    std::shared_ptr<const RuntimeConfigurationSnapshot> snapshot,
    std::uint64_t generationId) noexcept
    : owner_(&owner),
      snapshot_(std::move(snapshot)),
      generationId_(generationId) {}

RuntimeConfigurationReadLease::~RuntimeConfigurationReadLease() { release(); }

RuntimeConfigurationReadLease::RuntimeConfigurationReadLease(
    RuntimeConfigurationReadLease&& other) noexcept
    : owner_(std::exchange(other.owner_, nullptr)),
      snapshot_(std::move(other.snapshot_)),
      generationId_(std::exchange(other.generationId_, 0U)) {}

RuntimeConfigurationReadLease& RuntimeConfigurationReadLease::operator=(
    RuntimeConfigurationReadLease&& other) noexcept {
    if (this != &other) {
        release();
        owner_ = std::exchange(other.owner_, nullptr);
        snapshot_ = std::move(other.snapshot_);
        generationId_ = std::exchange(other.generationId_, 0U);
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
        auto* owner = owner_;
        const auto generationId = generationId_;
        snapshot_.reset();
        owner_ = nullptr;
        generationId_ = 0U;
        owner->releaseRuntimeLease(generationId);
    }
}

ConfigurationPreviewBuildLease::ConfigurationPreviewBuildLease(
    ConfigurationService& owner, std::uint64_t reservationId,
    std::uint64_t expectedStateRevision,
    ConfigurationManifestReference expectedActive,
    std::shared_ptr<Candidate> candidate) noexcept
    : owner_(&owner),
      reservationId_(reservationId),
      expectedStateRevision_(expectedStateRevision),
      expectedActive_(expectedActive),
      candidate_(std::move(candidate)) {}

ConfigurationPreviewBuildLease::~ConfigurationPreviewBuildLease() { release(); }

ConfigurationPreviewBuildLease::ConfigurationPreviewBuildLease(
    ConfigurationPreviewBuildLease&& other) noexcept
    : owner_(std::exchange(other.owner_, nullptr)),
      reservationId_(std::exchange(other.reservationId_, 0U)),
      expectedStateRevision_(other.expectedStateRevision_),
      expectedActive_(other.expectedActive_),
      candidate_(std::move(other.candidate_)) {}

ConfigurationPreviewBuildLease& ConfigurationPreviewBuildLease::operator=(
    ConfigurationPreviewBuildLease&& other) noexcept {
    if (this != &other) {
        release();
        owner_ = std::exchange(other.owner_, nullptr);
        reservationId_ = std::exchange(other.reservationId_, 0U);
        expectedStateRevision_ = other.expectedStateRevision_;
        expectedActive_ = other.expectedActive_;
        candidate_ = std::move(other.candidate_);
    }
    return *this;
}

UserConfiguration& ConfigurationPreviewBuildLease::userConfiguration() {
    return *candidate_->userConfiguration;
}

ServiceConfiguration& ConfigurationPreviewBuildLease::serviceConfiguration() {
    return *candidate_->serviceConfiguration;
}

ProgramCatalog& ConfigurationPreviewBuildLease::programCatalog() {
    return *candidate_->programCatalog;
}

void ConfigurationPreviewBuildLease::release() noexcept {
    if (owner_ != nullptr) {
        const auto reservationId = reservationId_;
        candidate_.reset();
        owner_->releasePreviewBuild(reservationId);
        owner_ = nullptr;
        reservationId_ = 0U;
    }
}

ConfigurationService::ConfigurationService(
    ConfigurationMutationCoordinator& mutationCoordinator,
    ConfigurationGraphStore& graphStore,
    const device_platform::ITimeZoneResolver& timeZoneResolver)
    : mutationCoordinator_(mutationCoordinator),
      graphStore_(graphStore),
      timeZoneResolver_(timeZoneResolver) {}

ConfigurationService::~ConfigurationService() = default;

bool ConfigurationService::initializeForTest(
    const LoadedConfigurationGraph& graph) {
    auto prepared = prepareSnapshot(graph, 1U);
    auto preparedGraph = std::make_unique<LoadedConfigurationGraph>(graph);
    const std::lock_guard<std::mutex> lock(stateMutex_);
    if (!prepared || activeRuntime_) {
        mode_ = ConfigurationServiceMode::RuntimeFailure;
        runtimeFailureCause_ =
            ConfigurationRuntimeFailureCause::ServiceStateInvariantViolation;
        return false;
    }
    activeRuntime_ = std::move(prepared);
    currentGraph_ = std::move(preparedGraph);
    nextRuntimeGeneration_ = 2U;
    mode_ = ConfigurationServiceMode::Operational;
    runtimeFailureCause_.reset();
    return true;
}

ConfigurationRecoveryBeginStatus ConfigurationService::beginRecovery(
    ConfigurationServiceMode targetMode, std::uint64_t requiredHeadroom) {
    const std::lock_guard<std::mutex> lock(stateMutex_);
    const bool permitted =
        (targetMode == ConfigurationServiceMode::ResetPreparing)
            ? (mode_ == ConfigurationServiceMode::Operational ||
               mode_ == ConfigurationServiceMode::ResetEligibleNoRuntime)
            : (mode_ == ConfigurationServiceMode::NoRuntime ||
               mode_ == targetMode);
    if (!permitted) {
        return ConfigurationRecoveryBeginStatus::StateTransitionRejected;
    }
    if (!stateRevisionHasHeadroomLocked(requiredHeadroom) ||
        recoveryGeneration_ == std::numeric_limits<std::uint64_t>::max()) {
        return ConfigurationRecoveryBeginStatus::CounterOverflow;
    }
    if (capturedPreview_ || previewBuildReservation_.has_value() ||
        retiredGenerationId_.has_value() || retirementOwnerPending_ ||
        recoveryPreparedRuntime_ || recoveryPreparedGraph_) {
        return ConfigurationRecoveryBeginStatus::ConfigurationModelBudgetBusy;
    }
    clearPreviewLocked();
    previewModelReserved_ = true;
    mode_ = targetMode;
    ++recoveryGeneration_;
    return incrementStateRevisionLocked()
               ? ConfigurationRecoveryBeginStatus::Success
               : ConfigurationRecoveryBeginStatus::CounterOverflow;
}

bool ConfigurationService::prepareRecoveredGraph(
    const LoadedConfigurationGraph& graph) {
    auto prepared = prepareSnapshot(graph, nextRuntimeGeneration_);
    auto preparedGraph = std::make_unique<LoadedConfigurationGraph>(graph);
    const std::lock_guard<std::mutex> lock(stateMutex_);
    if (!prepared ||
        (mode_ != ConfigurationServiceMode::RecoveryPreparing &&
         mode_ != ConfigurationServiceMode::ResetPreparing &&
         mode_ != ConfigurationServiceMode::EpochResetting) ||
        recoveryPreparedRuntime_ || recoveryPreparedGraph_) {
        enterFailClosedLocked(ConfigurationServiceMode::RuntimeFailure,
                              ConfigurationRuntimeFailureCause::
                                  RuntimePreparationAfterResolutionFailure);
        return false;
    }
    recoveryPreparedRuntime_ = std::move(prepared);
    recoveryPreparedGraph_ = std::move(preparedGraph);
    previewModelReserved_ = activeRuntime_ != nullptr;
    return true;
}

bool ConfigurationService::publishRecoveredGraph(
    CommittedRecoveryActivation&& activation) {
    std::shared_ptr<const RuntimeConfigurationSnapshot> retiredRuntime;
    std::unique_ptr<LoadedConfigurationGraph> retiredGraph;
    std::uint64_t retiredGeneration = 0U;
    {
        const std::lock_guard<std::mutex> lock(stateMutex_);
        if ((mode_ != ConfigurationServiceMode::RecoveryPreparing &&
             mode_ != ConfigurationServiceMode::EpochResetting &&
             mode_ != ConfigurationServiceMode::CommitIndeterminate) ||
            !recoveryPreparedRuntime_ || !recoveryPreparedGraph_) {
            enterFailClosedLocked(
                ConfigurationServiceMode::RuntimeFailure,
                ConfigurationRuntimeFailureCause::PublishContractViolation);
            return false;
        }
        if (activation.consumed_ ||
            activation.stateRevision_ != stateRevision_ ||
            activation.recoveryGeneration_ != recoveryGeneration_ ||
            activation.epoch_ !=
                recoveryPreparedGraph_->active.manifestReference.storageEpoch ||
            activation.rootSlot_ != recoveryPreparedGraph_->rootSlot ||
            activation.canonicalRootBytes_ !=
                recoveryPreparedGraph_->canonicalRootRecordBytes) {
            enterFailClosedLocked(
                ConfigurationServiceMode::RuntimeFailure,
                ConfigurationRuntimeFailureCause::PublishContractViolation);
            return false;
        }
        activation.consumed_ = true;
        retiredRuntime = std::move(activeRuntime_);
        retiredGraph = std::move(currentGraph_);
        activeRuntime_ = std::move(recoveryPreparedRuntime_);
        currentGraph_ = std::move(recoveryPreparedGraph_);
        if (retiredRuntime) {
            retiredGeneration = retiredRuntime->volatileGenerationId();
            retiredGenerationId_ = retiredGeneration;
            retiredGenerationReadLeases_ = activeGenerationReadLeases_;
            activeGenerationReadLeases_ = 0U;
            retirementOwnerPending_ = true;
        }
        ++nextRuntimeGeneration_;
        publishedRecoveryEpoch_ = activation.epoch_;
        publishedRecoveryRootBytes_ = activation.canonicalRootBytes_;
        publishedRecoveryPlanIdentity_ = activation.planIdentity_;
        mode_ = ConfigurationServiceMode::BootstrapFinalizationPending;
        if (!incrementStateRevisionLocked()) {
            enterFailClosedLocked(ConfigurationServiceMode::RuntimeFailure,
                                  ConfigurationRuntimeFailureCause::
                                      ServiceStateInvariantViolation);
            return false;
        }
    }
    retiredGraph.reset();
    retiredRuntime.reset();
    if (retiredGeneration != 0U) {
        if (!completeRuntimeRetirement(retiredGeneration)) {
            return false;
        }
    } else {
        const std::lock_guard<std::mutex> lock(stateMutex_);
        previewModelReserved_ = false;
    }
    return true;
}

bool ConfigurationService::transitionRecovery(
    ConfigurationServiceMode targetMode) {
    const std::lock_guard<std::mutex> lock(stateMutex_);
    if (mode_ != ConfigurationServiceMode::RecoveryPreparing &&
        mode_ != ConfigurationServiceMode::ResetPreparing &&
        mode_ != ConfigurationServiceMode::EpochResetting &&
        mode_ != ConfigurationServiceMode::CommitIndeterminate) {
        return false;
    }
    mode_ = targetMode;
    return incrementStateRevisionLocked();
}

bool ConfigurationService::discardPreparedRecovery(
    ConfigurationServiceMode targetMode) {
    std::shared_ptr<const RuntimeConfigurationSnapshot> discardedRuntime;
    std::unique_ptr<LoadedConfigurationGraph> discardedGraph;
    {
        const std::lock_guard<std::mutex> lock(stateMutex_);
        discardedRuntime = std::move(recoveryPreparedRuntime_);
        discardedGraph = std::move(recoveryPreparedGraph_);
        if (!retiredGenerationId_.has_value() && !retirementOwnerPending_) {
            previewModelReserved_ = false;
        }
        mode_ = targetMode;
        if (!incrementStateRevisionLocked()) {
            enterFailClosedLocked(ConfigurationServiceMode::RuntimeFailure,
                                  ConfigurationRuntimeFailureCause::
                                      ServiceStateInvariantViolation);
            return false;
        }
    }
    return true;
}

bool ConfigurationService::cancelRecovery() {
    std::shared_ptr<const RuntimeConfigurationSnapshot> discardedRuntime;
    std::unique_ptr<LoadedConfigurationGraph> discardedGraph;
    {
        const std::lock_guard<std::mutex> lock(stateMutex_);
        discardedRuntime = std::move(recoveryPreparedRuntime_);
        discardedGraph = std::move(recoveryPreparedGraph_);
        if (!retiredGenerationId_.has_value() && !retirementOwnerPending_) {
            previewModelReserved_ = false;
        }
        mode_ = activeRuntime_ && currentGraph_
                    ? ConfigurationServiceMode::Operational
                    : ConfigurationServiceMode::NoRuntime;
        if (!incrementStateRevisionLocked()) {
            enterFailClosedLocked(ConfigurationServiceMode::RuntimeFailure,
                                  ConfigurationRuntimeFailureCause::
                                      ServiceStateInvariantViolation);
            return false;
        }
    }
    return true;
}

bool ConfigurationService::finalizeRecoveredGraph(
    device_platform::StorageEpoch epoch, const std::string& canonicalRootBytes,
    std::uint32_t planIdentity) {
    const std::lock_guard<std::mutex> lock(stateMutex_);
    if (mode_ != ConfigurationServiceMode::BootstrapFinalizationPending ||
        !activeRuntime_ || !currentGraph_ ||
        !publishedRecoveryEpoch_.has_value() ||
        *publishedRecoveryEpoch_ != epoch ||
        publishedRecoveryRootBytes_ != canonicalRootBytes ||
        publishedRecoveryPlanIdentity_ != planIdentity ||
        currentGraph_->active.manifestReference.storageEpoch != epoch ||
        currentGraph_->canonicalRootRecordBytes != canonicalRootBytes) {
        return false;
    }
    mode_ = ConfigurationServiceMode::Operational;
    runtimeFailureCause_.reset();
    publishedRecoveryEpoch_.reset();
    publishedRecoveryRootBytes_.clear();
    publishedRecoveryPlanIdentity_ = 0U;
    return incrementStateRevisionLocked();
}

bool ConfigurationService::finalizeRecoveredGraphForBootstrap(
    device_platform::StorageEpoch epoch) {
    const std::lock_guard<std::mutex> lock(stateMutex_);
    if (mode_ != ConfigurationServiceMode::BootstrapFinalizationPending ||
        !activeRuntime_ || !currentGraph_ ||
        !publishedRecoveryEpoch_.has_value() ||
        *publishedRecoveryEpoch_ != epoch ||
        currentGraph_->active.manifestReference.storageEpoch != epoch ||
        currentGraph_->canonicalRootRecordBytes !=
            publishedRecoveryRootBytes_) {
        return false;
    }
    mode_ = ConfigurationServiceMode::Operational;
    runtimeFailureCause_.reset();
    publishedRecoveryEpoch_.reset();
    publishedRecoveryRootBytes_.clear();
    publishedRecoveryPlanIdentity_ = 0U;
    return incrementStateRevisionLocked();
}

bool ConfigurationService::validateRecoveryBinding(
    std::uint64_t stateRevision, std::uint64_t recoveryGeneration) const {
    const std::lock_guard<std::mutex> lock(stateMutex_);
    return stateRevision_ == stateRevision &&
           recoveryGeneration_ == recoveryGeneration &&
           (mode_ == ConfigurationServiceMode::RecoveryPreparing ||
            mode_ == ConfigurationServiceMode::EpochResetting ||
            mode_ == ConfigurationServiceMode::CommitIndeterminate);
}

bool ConfigurationService::markResetEligibleNoRuntime() {
    const std::lock_guard<std::mutex> lock(stateMutex_);
    if (mode_ != ConfigurationServiceMode::NoRuntime || activeRuntime_ ||
        recoveryPreparedRuntime_ || !incrementStateRevisionLocked()) {
        return false;
    }
    mode_ = ConfigurationServiceMode::ResetEligibleNoRuntime;
    return true;
}

std::uint64_t ConfigurationService::recoveryGeneration() const {
    const std::lock_guard<std::mutex> lock(stateMutex_);
    return recoveryGeneration_;
}

void ConfigurationService::failRecovery(
    ConfigurationRuntimeFailureCause cause) {
    const std::lock_guard<std::mutex> lock(stateMutex_);
    enterFailClosedLocked(ConfigurationServiceMode::RuntimeFailure, cause);
}

ConfigurationServiceMode ConfigurationService::mode() const {
    const std::lock_guard<std::mutex> lock(stateMutex_);
    return mode_;
}

std::uint64_t ConfigurationService::stateRevision() const {
    const std::lock_guard<std::mutex> lock(stateMutex_);
    return stateRevision_;
}

RuntimeConfigurationReadResult ConfigurationService::acquireRuntime() {
    const std::lock_guard<std::mutex> lock(stateMutex_);
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
    ++activeGenerationReadLeases_;
    RuntimeConfigurationReadResult result;
    result.status = RuntimeConfigurationReadStatus::RuntimeLeaseGranted;
    result.lease = RuntimeConfigurationReadLease(
        *this, activeRuntime_, activeRuntime_->volatileGenerationId());
    return result;
}

ConfigurationPreviewBuildResult ConfigurationService::beginPreview() {
    ConfigurationPreviewBuildResult result;
    std::uint64_t reservationId = 0U;
    std::uint64_t expectedRevision = 0U;
    ConfigurationManifestReference expectedActive;
    std::shared_ptr<const UserConfiguration> baseUser;
    std::shared_ptr<const ServiceConfiguration> baseService;
    std::shared_ptr<const ProgramCatalog> baseCatalog;
    {
        const std::lock_guard<std::mutex> lock(stateMutex_);
        if (mode_ != ConfigurationServiceMode::Operational || !activeRuntime_) {
            result.status =
                ConfigurationPreviewStatus::ConfigurationRuntimeUnavailable;
            return result;
        }
        if (previewBuildReservation_.has_value() || previewModelReserved_ ||
            retiredGenerationId_.has_value() || retirementOwnerPending_) {
            result.status =
                ConfigurationPreviewStatus::ConfigurationModelBudgetBusy;
            return result;
        }
        if (nextPreviewBuildReservation_ ==
            std::numeric_limits<std::uint64_t>::max()) {
            enterFailClosedLocked(ConfigurationServiceMode::RuntimeFailure,
                                  ConfigurationRuntimeFailureCause::
                                      ServiceStateInvariantViolation);
            result.status =
                ConfigurationPreviewStatus::ConfigurationRuntimeUnavailable;
            return result;
        }
        reservationId = nextPreviewBuildReservation_++;
        previewBuildReservation_ = reservationId;
        previewBuildRevoked_ = false;
        previewModelReserved_ = true;
        expectedRevision = stateRevision_;
        expectedActive = activeRuntime_->manifestReference_;
        baseUser = activeRuntime_->userConfiguration_;
        baseService = activeRuntime_->serviceConfiguration_;
        baseCatalog = activeRuntime_->programCatalog_;
    }

    auto candidate =
        std::make_shared<ConfigurationPreviewBuildLease::Candidate>();
    candidate->baseUserConfiguration = baseUser;
    candidate->baseServiceConfiguration = baseService;
    candidate->baseProgramCatalog = baseCatalog;
    candidate->userConfiguration =
        std::make_shared<UserConfiguration>(*baseUser);
    candidate->serviceConfiguration =
        std::make_shared<ServiceConfiguration>(*baseService);
    candidate->programCatalog = std::make_shared<ProgramCatalog>(*baseCatalog);

    {
        const std::lock_guard<std::mutex> lock(stateMutex_);
        if (previewBuildReservation_ != reservationId || previewBuildRevoked_ ||
            mode_ != ConfigurationServiceMode::Operational ||
            stateRevision_ != expectedRevision || !activeRuntime_ ||
            activeRuntime_->manifestReference_ != expectedActive) {
            if (previewBuildReservation_ == reservationId) {
                previewBuildReservation_.reset();
                previewBuildRevoked_ = false;
                previewModelReserved_ = false;
            }
            result.status = ConfigurationPreviewStatus::StateChanged;
            return result;
        }
    }
    result.status = ConfigurationPreviewStatus::Success;
    result.lease =
        ConfigurationPreviewBuildLease(*this, reservationId, expectedRevision,
                                       expectedActive, std::move(candidate));
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

    ConfigurationCandidateIntegrity integrity;
    if (!calculateCandidateIntegrity(*candidate, timeZoneResolver_,
                                     integrity)) {
        result.status = ConfigurationPreviewStatus::InvalidCandidate;
        return result;
    }
    const ConfigurationChangeMask changes{
        !configurationContentEquals(*candidate->userConfiguration,
                                    *candidate->baseUserConfiguration),
        !configurationContentEquals(*candidate->serviceConfiguration,
                                    *candidate->baseServiceConfiguration),
        !configurationContentEquals(*candidate->programCatalog,
                                    *candidate->baseProgramCatalog)};
    const bool noChange = !changes.userConfiguration &&
                          !changes.serviceConfiguration &&
                          !changes.programCatalog;
    const auto summary = summarizeChanges(*candidate);
    std::uint64_t handle = 0U;
    {
        const std::lock_guard<std::mutex> lock(stateMutex_);
        if (previewBuildReservation_ != buildLease.reservationId_ ||
            previewBuildRevoked_ ||
            mode_ != ConfigurationServiceMode::Operational ||
            stateRevision_ != buildLease.expectedStateRevision_ ||
            !activeRuntime_ ||
            activeRuntime_->manifestReference_ != buildLease.expectedActive_) {
            result.status = ConfigurationPreviewStatus::StateChanged;
            return result;
        }
        if (nextPreviewHandle_ == std::numeric_limits<std::uint64_t>::max()) {
            enterFailClosedLocked(ConfigurationServiceMode::RuntimeFailure,
                                  ConfigurationRuntimeFailureCause::
                                      ServiceStateInvariantViolation);
            result.status =
                ConfigurationPreviewStatus::ConfigurationRuntimeUnavailable;
            return result;
        }
        handle = nextPreviewHandle_++;
    }
    const ConfigurationPreviewView view{
        handle,  buildLease.expectedActive_,
        changes, integrity,
        summary, ConfigurationActivationEffect::Immediate,
        noChange};
    if (!noChange) {
        auto immutableCandidate =
            std::make_shared<ConfigurationPreviewBuildLease::Candidate>();
        immutableCandidate->baseUserConfiguration =
            candidate->baseUserConfiguration;
        immutableCandidate->baseServiceConfiguration =
            candidate->baseServiceConfiguration;
        immutableCandidate->baseProgramCatalog = candidate->baseProgramCatalog;
        immutableCandidate->userConfiguration =
            std::make_shared<UserConfiguration>(
                std::move(*candidate->userConfiguration));
        immutableCandidate->serviceConfiguration =
            std::make_shared<ServiceConfiguration>(
                *candidate->serviceConfiguration);
        immutableCandidate->programCatalog = std::make_shared<ProgramCatalog>(
            std::move(*candidate->programCatalog));
        candidate = std::move(immutableCandidate);
    }
    auto preview = std::make_shared<const Preview>(
        Preview{view, origin, operation, buildLease.expectedStateRevision_,
                noChange ? nullptr : candidate,
                std::move(*userValidation.preparedTimeZone)});
    std::shared_ptr<ConfigurationPreviewBuildLease::Candidate> discarded;
    invokeTestHook(TestPoint::PreviewBeforeInstall);
    {
        const std::lock_guard<std::mutex> lock(stateMutex_);
        if (previewBuildReservation_ != buildLease.reservationId_ ||
            previewBuildRevoked_ ||
            mode_ != ConfigurationServiceMode::Operational ||
            stateRevision_ != buildLease.expectedStateRevision_ ||
            !activeRuntime_ ||
            activeRuntime_->manifestReference_ != buildLease.expectedActive_) {
            result.status = ConfigurationPreviewStatus::StateChanged;
            return result;
        }
        visiblePreview_ = std::move(preview);
        previewBuildReservation_.reset();
        previewBuildRevoked_ = false;
        if (noChange) {
            previewModelReserved_ = false;
            discarded = std::move(candidate);
        }
        buildLease.candidate_.reset();
        buildLease.owner_ = nullptr;
        buildLease.reservationId_ = 0U;
    }
    discarded.reset();
    result.status = ConfigurationPreviewStatus::Success;
    result.preview = view;
    return result;
}

std::optional<ConfigurationPreviewView> ConfigurationService::visiblePreview()
    const {
    const std::lock_guard<std::mutex> lock(stateMutex_);
    if (!visiblePreview_) {
        return std::nullopt;
    }
    return visiblePreview_->view;
}

ConfigurationPreviewStatus ConfigurationService::cancelPreview(
    std::uint64_t handle) {
    std::shared_ptr<const Preview> cancelled;
    bool fullModel = false;
    {
        const std::lock_guard<std::mutex> lock(stateMutex_);
        if (!visiblePreview_) {
            return ConfigurationPreviewStatus::PreviewNotFound;
        }
        if (visiblePreview_->view.handle != handle) {
            return ConfigurationPreviewStatus::PreviewSuperseded;
        }
        fullModel = !visiblePreview_->view.noChange;
        cancelled = std::move(visiblePreview_);
    }
    cancelled.reset();
    if (fullModel) {
        const std::lock_guard<std::mutex> lock(stateMutex_);
        if (!capturedPreview_ && !previewBuildReservation_.has_value() &&
            !resolutionContext_ && !retiredGenerationId_.has_value() &&
            !retirementOwnerPending_) {
            previewModelReserved_ = false;
        }
    }
    return ConfigurationPreviewStatus::Success;
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
ConfigurationCommitResult ConfigurationService::confirmPreview(
    std::uint64_t handle) {
    std::shared_ptr<const Preview> captured;
    {
        const std::lock_guard<std::mutex> lock(stateMutex_);
        if (mode_ == ConfigurationServiceMode::CommitInProgress ||
            capturedPreview_) {
            return {ConfigurationCommitStatus::ConfigurationMutationBusy};
        }
        if (mode_ != ConfigurationServiceMode::Operational || !currentGraph_ ||
            !activeRuntime_) {
            return {ConfigurationCommitStatus::ConfigurationRuntimeFailure};
        }
        if (!visiblePreview_) {
            return {ConfigurationCommitStatus::PreviewNotFound};
        }
        if (visiblePreview_->view.handle != handle) {
            return {ConfigurationCommitStatus::PreviewSuperseded};
        }
        captured = visiblePreview_;
        if (captured->view.noChange) {
            const bool stale =
                stateRevision_ != captured->installationStateRevision ||
                currentGraph_->active.manifestReference !=
                    captured->view.expectedActive ||
                activeRuntime_->manifestReference() !=
                    captured->view.expectedActive;
            if (visiblePreview_ == captured) {
                clearPreviewLocked();
            }
            return {
                stale ? ConfigurationCommitStatus::ConfigurationConflictFailure
                      : ConfigurationCommitStatus::NoChange};
        }
        visiblePreview_.reset();
        capturedPreview_ = captured;
    }
    invokeTestHook(TestPoint::PreviewCaptured);

    auto mutation = mutationCoordinator_.tryAcquire();
    if (!mutation.lease.valid()) {
        const std::lock_guard<std::mutex> lock(stateMutex_);
        if (capturedPreview_ == captured &&
            mode_ == ConfigurationServiceMode::Operational &&
            !visiblePreview_) {
            visiblePreview_ = captured;
            capturedPreview_.reset();
        }
        return {ConfigurationCommitStatus::ConfigurationMutationBusy};
    }

    ConfigurationCandidateIntegrity confirmedIntegrity;
    if (!calculateCandidateIntegrity(*captured->candidate, timeZoneResolver_,
                                     confirmedIntegrity) ||
        !candidateIntegrityEquals(confirmedIntegrity,
                                  captured->view.integrity)) {
        {
            const std::lock_guard<std::mutex> lock(stateMutex_);
            capturedPreview_.reset();
        }
        captured.reset();
        {
            const std::lock_guard<std::mutex> lock(stateMutex_);
            previewModelReserved_ = false;
        }
        return {ConfigurationCommitStatus::ConfigurationValidationFailure};
    }

    LoadedConfigurationGraph current;
    bool basisConflict = false;
    {
        const std::lock_guard<std::mutex> lock(stateMutex_);
        if (mode_ != ConfigurationServiceMode::Operational || !currentGraph_ ||
            !activeRuntime_) {
            return {ConfigurationCommitStatus::ConfigurationRuntimeFailure};
        }
        if (capturedPreview_ != captured) {
            return {ConfigurationCommitStatus::PreviewSuperseded};
        }
        if (captured->view.expectedActive !=
                currentGraph_->active.manifestReference ||
            captured->view.expectedActive !=
                activeRuntime_->manifestReference()) {
            capturedPreview_.reset();
            basisConflict = true;
        } else if (nextRuntimeGeneration_ ==
                       std::numeric_limits<std::uint64_t>::max() ||
                   !stateRevisionHasHeadroomLocked(2U)) {
            enterFailClosedLocked(ConfigurationServiceMode::RuntimeFailure,
                                  ConfigurationRuntimeFailureCause::
                                      ServiceStateInvariantViolation);
            return {ConfigurationCommitStatus::ConfigurationRuntimeFailure};
        } else {
            mode_ = ConfigurationServiceMode::CommitInProgress;
        }
        if (!basisConflict && !incrementStateRevisionLocked()) {
            enterFailClosedLocked(ConfigurationServiceMode::RuntimeFailure,
                                  ConfigurationRuntimeFailureCause::
                                      ServiceStateInvariantViolation);
            return {ConfigurationCommitStatus::ConfigurationRuntimeFailure};
        }
        if (!basisConflict) {
            current = *currentGraph_;
        }
    }
    if (basisConflict) {
        captured.reset();
        const std::lock_guard<std::mutex> lock(stateMutex_);
        previewModelReserved_ = false;
        return {ConfigurationCommitStatus::ConfigurationConflictFailure};
    }

    const ConfigurationCommitCandidate candidate{
        captured->candidate->userConfiguration,
        captured->candidate->serviceConfiguration,
        captured->candidate->programCatalog};
    auto prepared = graphStore_.prepareCommit(
        current, candidate, captured->origin, captured->operation);
    if (!prepared.prepared.has_value()) {
        const bool failClosed =
            prepared.status ==
                ConfigurationCommitPrepareStatus::IntegrityFailure ||
            prepared.status ==
                ConfigurationCommitPrepareStatus::IdentityCollision ||
            prepared.status ==
                ConfigurationCommitPrepareStatus::UnsupportedNewerSchema ||
            prepared.status ==
                ConfigurationCommitPrepareStatus::PersistenceFailure ||
            prepared.status ==
                ConfigurationCommitPrepareStatus::CapacityFailure;
        {
            const std::lock_guard<std::mutex> lock(stateMutex_);
            capturedPreview_.reset();
            if (failClosed) {
                auto cause = ConfigurationRuntimeFailureCause::
                    PersistentGraphVerificationFailure;
                if (prepared.status ==
                    ConfigurationCommitPrepareStatus::IdentityCollision) {
                    cause = ConfigurationRuntimeFailureCause::
                        PersistentConfigurationIdentityCollision;
                } else if (prepared.status == ConfigurationCommitPrepareStatus::
                                                  UnsupportedNewerSchema) {
                    cause = ConfigurationRuntimeFailureCause::
                        UnsupportedNewerConfigurationSchema;
                } else if (prepared.status ==
                           ConfigurationCommitPrepareStatus::IntegrityFailure) {
                    cause = ConfigurationRuntimeFailureCause::
                        PersistentGraphIntegrityFailure;
                } else if (prepared.status == ConfigurationCommitPrepareStatus::
                                                  PersistenceFailure ||
                           prepared.status == ConfigurationCommitPrepareStatus::
                                                  CapacityFailure) {
                    cause = ConfigurationRuntimeFailureCause::
                        PersistentStoreReadFailure;
                }
                enterFailClosedLocked(ConfigurationServiceMode::RuntimeFailure,
                                      cause);
            }
        }
        captured.reset();
        {
            const std::lock_guard<std::mutex> lock(stateMutex_);
            if (!resolutionContext_ && !retiredGenerationId_.has_value() &&
                !retirementOwnerPending_) {
                previewModelReserved_ = false;
            }
            if (!failClosed) {
                mode_ = ConfigurationServiceMode::Operational;
                if (!incrementStateRevisionLocked()) {
                    enterFailClosedLocked(
                        ConfigurationServiceMode::RuntimeFailure,
                        ConfigurationRuntimeFailureCause::
                            ServiceStateInvariantViolation);
                    return {
                        ConfigurationCommitStatus::ConfigurationRuntimeFailure};
                }
            }
        }
        if (failClosed) {
            return {ConfigurationCommitStatus::ConfigurationRuntimeFailure};
        }
        if (prepared.status == ConfigurationCommitPrepareStatus::Conflict) {
            return {ConfigurationCommitStatus::ConfigurationConflictFailure};
        }
        if (prepared.status ==
            ConfigurationCommitPrepareStatus::InvalidCandidate) {
            return {ConfigurationCommitStatus::ConfigurationValidationFailure};
        }
        if (prepared.status ==
                ConfigurationCommitPrepareStatus::CapacityFailure ||
            prepared.status ==
                ConfigurationCommitPrepareStatus::HighWaterOverflow ||
            prepared.status ==
                ConfigurationCommitPrepareStatus::NoUnreferencedSlotAvailable) {
            return {ConfigurationCommitStatus::CapacityFailure};
        }
        return {ConfigurationCommitStatus::PersistenceFailure};
    }

    auto preparedRuntime =
        prepareSnapshot(prepared.prepared->newGraph, nextRuntimeGeneration_);
    if (!preparedRuntime) {
        {
            const std::lock_guard<std::mutex> lock(stateMutex_);
            capturedPreview_.reset();
        }
        captured.reset();
        const std::lock_guard<std::mutex> lock(stateMutex_);
        previewModelReserved_ = false;
        mode_ = ConfigurationServiceMode::Operational;
        if (!incrementStateRevisionLocked()) {
            enterFailClosedLocked(ConfigurationServiceMode::RuntimeFailure,
                                  ConfigurationRuntimeFailureCause::
                                      ServiceStateInvariantViolation);
            return {ConfigurationCommitStatus::ConfigurationRuntimeFailure};
        }
        return {ConfigurationCommitStatus::ConfigurationValidationFailure};
    }

    auto publishedGraph =
        std::make_unique<LoadedConfigurationGraph>(prepared.prepared->newGraph);
    const auto runtimeBinding =
        makeRuntimeBinding(*publishedGraph, *preparedRuntime);
    auto resolution = std::make_unique<ResolutionContext>(ResolutionContext{
        std::move(*prepared.prepared), std::move(publishedGraph),
        std::move(preparedRuntime), runtimeBinding,
        ConfigurationCommitIndeterminateCause::AmbiguousRootOutcome, false});
    const auto execution =
        graphStore_.executePreparedCommit(resolution->persistent);
    if (execution.status == ConfigurationCommitExecutionStatus::Activated) {
        invokeTestHook(TestPoint::BeforePublish);
        std::shared_ptr<const RuntimeConfigurationSnapshot> retired;
        std::unique_ptr<LoadedConfigurationGraph> retiredGraph;
        std::uint64_t retiredGeneration = 0U;
        {
            const std::lock_guard<std::mutex> lock(stateMutex_);
            if (mode_ != ConfigurationServiceMode::CommitInProgress) {
                enterFailClosedLocked(
                    ConfigurationServiceMode::RuntimeFailure,
                    ConfigurationRuntimeFailureCause::PublishContractViolation);
                return {ConfigurationCommitStatus::ConfigurationRuntimeFailure};
            }
            publishPreparedLocked(
                resolution->persistent, resolution->preparedGraph,
                std::move(resolution->preparedRuntime), retired, retiredGraph);
            retiredGeneration = retired->volatileGenerationId();
            ++nextRuntimeGeneration_;
            capturedPreview_.reset();
        }
        invokeTestHook(TestPoint::BeforeRetirementRelease);
        retired.reset();
        retiredGraph.reset();
        resolution.reset();
        captured.reset();
        (void)completeRuntimeRetirement(retiredGeneration);
        {
            const std::lock_guard<std::mutex> lock(stateMutex_);
            if (mode_ == ConfigurationServiceMode::RuntimeFailure) {
                return {ConfigurationCommitStatus::ConfigurationRuntimeFailure};
            }
            mode_ = ConfigurationServiceMode::Operational;
            runtimeFailureCause_.reset();
            if (!incrementStateRevisionLocked()) {
                enterFailClosedLocked(ConfigurationServiceMode::RuntimeFailure,
                                      ConfigurationRuntimeFailureCause::
                                          ServiceStateInvariantViolation);
                return {ConfigurationCommitStatus::ConfigurationRuntimeFailure};
            }
        }
        return {ConfigurationCommitStatus::Activated};
    }

    if (execution.status ==
        ConfigurationCommitExecutionStatus::CommitIndeterminate) {
        const std::lock_guard<std::mutex> lock(stateMutex_);
        resolutionContext_ = std::move(resolution);
        resolutionContext_->indeterminateCause =
            mapIndeterminateCause(execution.resolutionCause);
        capturedPreview_.reset();
        enterFailClosedLocked(ConfigurationServiceMode::CommitIndeterminate,
                              ConfigurationRuntimeFailureCause::
                                  PersistentGraphVerificationFailure);
        return {ConfigurationCommitStatus::ConfigurationCommitIndeterminate};
    }
    if (execution.status ==
        ConfigurationCommitExecutionStatus::RuntimeFailure) {
        const std::lock_guard<std::mutex> lock(stateMutex_);
        resolutionContext_ = std::move(resolution);
        capturedPreview_.reset();
        auto failureCause =
            ConfigurationRuntimeFailureCause::PostCommitVerificationFailure;
        if (execution.phase ==
            ConfigurationCommitFailurePhase::RootVerification) {
            failureCause =
                ConfigurationRuntimeFailureCause::PostCommitVerificationFailure;
        } else if (execution.resolutionCause ==
                   ConfigurationCommitResolutionCause::IdentityCollision) {
            failureCause = ConfigurationRuntimeFailureCause::
                PersistentConfigurationIdentityCollision;
        } else if (execution.resolutionCause ==
                   ConfigurationCommitResolutionCause::UnsupportedNewerSchema) {
            failureCause = ConfigurationRuntimeFailureCause::
                UnsupportedNewerConfigurationSchema;
        } else if (execution.resolutionCause ==
                       ConfigurationCommitResolutionCause::GraphReadError ||
                   execution.resolutionCause ==
                       ConfigurationCommitResolutionCause::GraphCapacityError) {
            failureCause =
                ConfigurationRuntimeFailureCause::PersistentStoreReadFailure;
        } else if (
            execution.resolutionCause ==
                ConfigurationCommitResolutionCause::GraphIntegrityFailure ||
            execution.resolutionCause ==
                ConfigurationCommitResolutionCause::GraphEnvelopeOrCrcFailure ||
            execution.resolutionCause ==
                ConfigurationCommitResolutionCause::GraphReferenceFailure ||
            execution.resolutionCause ==
                ConfigurationCommitResolutionCause::GraphSemanticFailure) {
            failureCause = ConfigurationRuntimeFailureCause::
                PersistentGraphIntegrityFailure;
        } else if (execution.phase ==
                   ConfigurationCommitFailurePhase::TargetGraphVerification) {
            failureCause = ConfigurationRuntimeFailureCause::
                PersistentGraphVerificationFailure;
        }
        enterFailClosedLocked(ConfigurationServiceMode::RuntimeFailure,
                              failureCause);
        return {ConfigurationCommitStatus::ConfigurationRuntimeFailure};
    }

    {
        const std::lock_guard<std::mutex> lock(stateMutex_);
        capturedPreview_.reset();
    }
    resolution.reset();
    captured.reset();
    {
        const std::lock_guard<std::mutex> lock(stateMutex_);
        previewModelReserved_ = false;
        mode_ = ConfigurationServiceMode::Operational;
        if (!incrementStateRevisionLocked()) {
            enterFailClosedLocked(ConfigurationServiceMode::RuntimeFailure,
                                  ConfigurationRuntimeFailureCause::
                                      ServiceStateInvariantViolation);
            return {ConfigurationCommitStatus::ConfigurationRuntimeFailure};
        }
    }
    return {execution.status ==
                    ConfigurationCommitExecutionStatus::CapacityFailure
                ? ConfigurationCommitStatus::CapacityFailure
                : ConfigurationCommitStatus::PersistenceFailure};
}

ConfigurationCommitResolutionStatus ConfigurationService::
    resolveIndeterminate() {  // NOLINT(readability-function-cognitive-complexity):
                              // Die expliziten fail-closed Aufloesungszweige
                              // bilden den persistierten Commitausgang ab.
    auto mutation = mutationCoordinator_.tryAcquire();
    if (!mutation.lease.valid()) {
        return ConfigurationCommitResolutionStatus::
            ResolutionStillIndeterminate;
    }
    ResolutionContext* context = nullptr;
    {
        const std::lock_guard<std::mutex> lock(stateMutex_);
        if (mode_ != ConfigurationServiceMode::CommitIndeterminate ||
            !resolutionContext_ || !stateRevisionHasHeadroomLocked(1U)) {
            return ConfigurationCommitResolutionStatus::
                ResolutionRuntimeFailure;
        }
        context = resolutionContext_.get();
    }
    const auto resolution =
        graphStore_.resolveCommitDetailed(context->persistent);
    if (resolution.status ==
        ConfigurationCommitResolutionStatus::ResolutionStillIndeterminate) {
        const std::lock_guard<std::mutex> lock(stateMutex_);
        if (resolutionContext_.get() == context) {
            resolutionContext_->indeterminateCause =
                mapIndeterminateCause(resolution.cause);
        }
        return resolution.status;
    }
    if (resolution.status ==
        ConfigurationCommitResolutionStatus::ResolutionRecoveredOld) {
        std::unique_ptr<ResolutionContext> discarded;
        {
            const std::lock_guard<std::mutex> lock(stateMutex_);
            if (resolutionContext_.get() != context) {
                return ConfigurationCommitResolutionStatus::
                    ResolutionRuntimeFailure;
            }
            discarded = std::move(resolutionContext_);
        }
        invokeTestHook(TestPoint::BeforeResolutionContextRelease);
        discarded.reset();
        {
            const std::lock_guard<std::mutex> lock(stateMutex_);
            previewModelReserved_ = false;
            mode_ = ConfigurationServiceMode::Operational;
            runtimeFailureCause_.reset();
            if (!incrementStateRevisionLocked()) {
                return ConfigurationCommitResolutionStatus::
                    ResolutionRuntimeFailure;
            }
        }
        return resolution.status;
    }
    if (resolution.status ==
        ConfigurationCommitResolutionStatus::ResolutionRecoveredNew) {
        if (!runtimeBindingMatches(context->runtimeBinding,
                                   context->persistent.newGraph,
                                   *context->preparedRuntime)) {
            std::shared_ptr<const RuntimeConfigurationSnapshot> invalid;
            {
                const std::lock_guard<std::mutex> lock(stateMutex_);
                if (resolutionContext_.get() != context) {
                    return ConfigurationCommitResolutionStatus::
                        ResolutionRuntimeFailure;
                }
                invalid = std::move(resolutionContext_->preparedRuntime);
            }
            invalid.reset();
            auto rebuilt = prepareSnapshot(context->persistent.newGraph,
                                           nextRuntimeGeneration_);
            if (!rebuilt) {
                const std::lock_guard<std::mutex> lock(stateMutex_);
                enterFailClosedLocked(
                    ConfigurationServiceMode::RuntimeFailure,
                    ConfigurationRuntimeFailureCause::
                        RuntimePreparationAfterResolutionFailure);
                return ConfigurationCommitResolutionStatus::
                    ResolutionRuntimeFailure;
            }
            const auto rebuiltBinding =
                makeRuntimeBinding(context->persistent.newGraph, *rebuilt);
            const std::lock_guard<std::mutex> lock(stateMutex_);
            if (resolutionContext_.get() != context ||
                mode_ != ConfigurationServiceMode::CommitIndeterminate) {
                return ConfigurationCommitResolutionStatus::
                    ResolutionRuntimeFailure;
            }
            resolutionContext_->preparedRuntime = std::move(rebuilt);
            resolutionContext_->runtimeBinding = rebuiltBinding;
        }
        std::shared_ptr<const RuntimeConfigurationSnapshot> retired;
        std::unique_ptr<LoadedConfigurationGraph> retiredGraph;
        std::unique_ptr<ResolutionContext> completed;
        std::uint64_t retiredGeneration = 0U;
        {
            const std::lock_guard<std::mutex> lock(stateMutex_);
            if (resolutionContext_.get() != context ||
                !runtimeBindingMatches(resolutionContext_->runtimeBinding,
                                       resolutionContext_->persistent.newGraph,
                                       *resolutionContext_->preparedRuntime)) {
                enterFailClosedLocked(
                    ConfigurationServiceMode::RuntimeFailure,
                    ConfigurationRuntimeFailureCause::
                        RuntimePreparationAfterResolutionFailure);
                return ConfigurationCommitResolutionStatus::
                    ResolutionRuntimeFailure;
            }
            publishPreparedLocked(
                resolutionContext_->persistent,
                resolutionContext_->preparedGraph,
                std::move(resolutionContext_->preparedRuntime), retired,
                retiredGraph);
            retiredGeneration = retired->volatileGenerationId();
            ++nextRuntimeGeneration_;
            completed = std::move(resolutionContext_);
        }
        invokeTestHook(TestPoint::BeforeRetirementRelease);
        retired.reset();
        retiredGraph.reset();
        completed.reset();
        (void)completeRuntimeRetirement(retiredGeneration);
        {
            const std::lock_guard<std::mutex> lock(stateMutex_);
            if (mode_ == ConfigurationServiceMode::RuntimeFailure) {
                return ConfigurationCommitResolutionStatus::
                    ResolutionRuntimeFailure;
            }
            mode_ = ConfigurationServiceMode::Operational;
            runtimeFailureCause_.reset();
            if (!incrementStateRevisionLocked()) {
                return ConfigurationCommitResolutionStatus::
                    ResolutionRuntimeFailure;
            }
        }
        return resolution.status;
    }
    {
        const std::lock_guard<std::mutex> lock(stateMutex_);
        enterFailClosedLocked(
            ConfigurationServiceMode::RuntimeFailure,
            ConfigurationRuntimeFailureCause::PostCommitVerificationFailure);
    }
    return ConfigurationCommitResolutionStatus::ResolutionRuntimeFailure;
}

ConfigurationCommitResolutionStatus
ConfigurationService::recoverRuntimeFailure() {
    auto mutation = mutationCoordinator_.tryAcquire();
    if (!mutation.lease.valid()) {
        return ConfigurationCommitResolutionStatus::ResolutionRuntimeFailure;
    }
    ResolutionContext* context = nullptr;
    ConfigurationRuntimeFailureCause failureCause =
        ConfigurationRuntimeFailureCause::ServiceStateInvariantViolation;
    {
        const std::lock_guard<std::mutex> lock(stateMutex_);
        const bool inProcessAllowed =
            runtimeFailureCause_ == ConfigurationRuntimeFailureCause::
                                        PersistentGraphVerificationFailure ||
            runtimeFailureCause_ == ConfigurationRuntimeFailureCause::
                                        PostCommitVerificationFailure ||
            runtimeFailureCause_ ==
                ConfigurationRuntimeFailureCause::
                    RuntimePreparationAfterResolutionFailure;
        if (mode_ != ConfigurationServiceMode::RuntimeFailure ||
            !resolutionContext_ || !inProcessAllowed ||
            !stateRevisionHasHeadroomLocked(1U)) {
            return ConfigurationCommitResolutionStatus::
                ResolutionRuntimeFailure;
        }
        context = resolutionContext_.get();
        failureCause = *runtimeFailureCause_;
        if (failureCause == ConfigurationRuntimeFailureCause::
                                RuntimePreparationAfterResolutionFailure) {
            if (context->runtimePreparationRetryConsumed) {
                return ConfigurationCommitResolutionStatus::
                    ResolutionRuntimeFailure;
            }
            context->runtimePreparationRetryConsumed = true;
        }
    }
    if (failureCause ==
        ConfigurationRuntimeFailureCause::PersistentGraphVerificationFailure) {
        const auto oldScan =
            graphStore_.validationScan(context->persistent.oldGraph);
        if (oldScan.status != ConfigurationScanStatus::Success) {
            return ConfigurationCommitResolutionStatus::
                ResolutionRuntimeFailure;
        }
        std::unique_ptr<ResolutionContext> completed;
        {
            const std::lock_guard<std::mutex> lock(stateMutex_);
            if (!activeRuntime_ || !currentGraph_ ||
                currentGraph_->canonicalRootRecordBytes !=
                    context->persistent.oldGraph.canonicalRootRecordBytes ||
                !runtimeSnapshotMatchesGraph(context->persistent.oldGraph,
                                             *activeRuntime_)) {
                return ConfigurationCommitResolutionStatus::
                    ResolutionRuntimeFailure;
            }
            completed = std::move(resolutionContext_);
        }
        invokeTestHook(TestPoint::BeforeResolutionContextRelease);
        completed.reset();
        {
            const std::lock_guard<std::mutex> lock(stateMutex_);
            previewModelReserved_ = false;
            mode_ = ConfigurationServiceMode::Operational;
            runtimeFailureCause_.reset();
            if (!incrementStateRevisionLocked()) {
                return ConfigurationCommitResolutionStatus::
                    ResolutionRuntimeFailure;
            }
        }
        return ConfigurationCommitResolutionStatus::ResolutionRecoveredOld;
    }

    const auto resolution =
        graphStore_.resolveCommitDetailed(context->persistent);
    if (resolution.status !=
        ConfigurationCommitResolutionStatus::ResolutionRecoveredNew) {
        return ConfigurationCommitResolutionStatus::ResolutionRuntimeFailure;
    }
    auto preparedRuntime =
        prepareSnapshot(context->persistent.newGraph, nextRuntimeGeneration_);
    if (!preparedRuntime) {
        return ConfigurationCommitResolutionStatus::ResolutionRuntimeFailure;
    }
    const auto binding =
        makeRuntimeBinding(context->persistent.newGraph, *preparedRuntime);
    std::shared_ptr<const RuntimeConfigurationSnapshot> retired;
    std::unique_ptr<LoadedConfigurationGraph> retiredGraph;
    std::unique_ptr<ResolutionContext> completed;
    std::uint64_t retiredGeneration = 0U;
    {
        const std::lock_guard<std::mutex> lock(stateMutex_);
        if (resolutionContext_.get() != context) {
            return ConfigurationCommitResolutionStatus::
                ResolutionRuntimeFailure;
        }
        resolutionContext_->preparedRuntime = std::move(preparedRuntime);
        resolutionContext_->runtimeBinding = binding;
        publishPreparedLocked(resolutionContext_->persistent,
                              resolutionContext_->preparedGraph,
                              std::move(resolutionContext_->preparedRuntime),
                              retired, retiredGraph);
        retiredGeneration = retired->volatileGenerationId();
        ++nextRuntimeGeneration_;
        completed = std::move(resolutionContext_);
    }
    invokeTestHook(TestPoint::BeforeRetirementRelease);
    retired.reset();
    retiredGraph.reset();
    completed.reset();
    const bool retirementCompleted =
        completeRuntimeRetirement(retiredGeneration);
    {
        const std::lock_guard<std::mutex> lock(stateMutex_);
        if (!retirementCompleted || runtimeFailureCause_ != failureCause) {
            return ConfigurationCommitResolutionStatus::
                ResolutionRuntimeFailure;
        }
        mode_ = ConfigurationServiceMode::Operational;
        runtimeFailureCause_.reset();
        if (!incrementStateRevisionLocked()) {
            return ConfigurationCommitResolutionStatus::
                ResolutionRuntimeFailure;
        }
    }
    return resolution.status;
}

std::optional<ConfigurationRuntimeFailureCause>
ConfigurationService::runtimeFailureCause() const {
    const std::lock_guard<std::mutex> lock(stateMutex_);
    return runtimeFailureCause_;
}

std::optional<ConfigurationCommitIndeterminateCause>
ConfigurationService::commitIndeterminateCause() const {
    const std::lock_guard<std::mutex> lock(stateMutex_);
    if (mode_ != ConfigurationServiceMode::CommitIndeterminate ||
        !resolutionContext_) {
        return std::nullopt;
    }
    return resolutionContext_->indeterminateCause;
}

std::size_t ConfigurationService::activeReadLeaseCount() const {
    const std::lock_guard<std::mutex> lock(stateMutex_);
    return readLeaseCount_;
}

std::size_t ConfigurationService::fullModelGenerationCount() const {
    const std::lock_guard<std::mutex> lock(stateMutex_);
    const auto active = static_cast<std::size_t>(activeRuntime_ != nullptr);
    const auto second = static_cast<std::size_t>(
        recoveryPreparedRuntime_ != nullptr || previewModelReserved_ ||
        retiredGenerationId_.has_value() || retirementOwnerPending_ ||
        resolutionContext_ != nullptr);
    return active + second;
}

void ConfigurationService::releaseRuntimeLease(
    std::uint64_t generationId) noexcept {
    const std::lock_guard<std::mutex> lock(stateMutex_);
    if (readLeaseCount_ > 0U) {
        --readLeaseCount_;
    }
    if (activeRuntime_ &&
        generationId == activeRuntime_->volatileGenerationId()) {
        if (activeGenerationReadLeases_ > 0U) {
            --activeGenerationReadLeases_;
        }
        return;
    }
    if (retiredGenerationId_.has_value() &&
        generationId == *retiredGenerationId_) {
        if (retiredGenerationReadLeases_ > 0U) {
            --retiredGenerationReadLeases_;
        }
        if (retiredGenerationReadLeases_ == 0U && !retirementOwnerPending_) {
            retiredGenerationId_.reset();
            if (!previewBuildReservation_.has_value() && !visiblePreview_ &&
                !capturedPreview_ && !resolutionContext_) {
                previewModelReserved_ = false;
            }
        }
    }
}

void ConfigurationService::releasePreviewBuild(
    std::uint64_t reservationId) noexcept {
    const std::lock_guard<std::mutex> lock(stateMutex_);
    if (previewBuildReservation_ == reservationId) {
        previewBuildReservation_.reset();
        previewBuildRevoked_ = false;
        if (!visiblePreview_ && !capturedPreview_ && !resolutionContext_ &&
            !retiredGenerationId_.has_value() && !retirementOwnerPending_ &&
            !recoveryPreparedRuntime_) {
            previewModelReserved_ = false;
        }
    }
}

bool ConfigurationService::incrementStateRevisionLocked() noexcept {
    if (stateRevision_ == std::numeric_limits<std::uint64_t>::max()) {
        mode_ = ConfigurationServiceMode::RuntimeFailure;
        runtimeFailureCause_ =
            ConfigurationRuntimeFailureCause::ServiceStateInvariantViolation;
        return false;
    }
    ++stateRevision_;
    return true;
}

bool ConfigurationService::stateRevisionHasHeadroomLocked(
    std::uint64_t steps) const noexcept {
    return stateRevision_ <= std::numeric_limits<std::uint64_t>::max() - steps;
}

std::shared_ptr<RuntimeConfigurationSnapshot>
ConfigurationService::prepareSnapshot(const LoadedConfigurationGraph& graph,
                                      std::uint64_t generationId) const {
    invokeTestHook(TestPoint::BeforeRuntimePreparation);
    if (rejectRuntimePreparationForTest_.load(std::memory_order_acquire)) {
        return {};
    }
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
    const bool releasedFullModel =
        visiblePreview_ && !visiblePreview_->view.noChange;
    visiblePreview_.reset();
    if (releasedFullModel && !capturedPreview_ &&
        !previewBuildReservation_.has_value() && !resolutionContext_ &&
        !retiredGenerationId_.has_value() && !retirementOwnerPending_) {
        previewModelReserved_ = false;
    }
}

void ConfigurationService::enterFailClosedLocked(
    ConfigurationServiceMode mode, ConfigurationRuntimeFailureCause cause) {
    mode_ = mode;
    runtimeFailureCause_ = cause;
    if (!incrementStateRevisionLocked()) {
        mode_ = ConfigurationServiceMode::RuntimeFailure;
        runtimeFailureCause_ =
            ConfigurationRuntimeFailureCause::ServiceStateInvariantViolation;
    }
    if (previewBuildReservation_.has_value()) {
        previewBuildRevoked_ = true;
    }
    clearPreviewLocked();
}

void ConfigurationService::publishPreparedLocked(
    PreparedConfigurationCommit& persistent,
    std::unique_ptr<LoadedConfigurationGraph>& preparedGraph,
    std::shared_ptr<const RuntimeConfigurationSnapshot> preparedRuntime,
    std::shared_ptr<const RuntimeConfigurationSnapshot>& retiredRuntime,
    std::unique_ptr<LoadedConfigurationGraph>& retiredGraph) noexcept {
    retiredRuntime.swap(activeRuntime_);
    activeRuntime_.swap(preparedRuntime);
    retiredGenerationId_ = retiredRuntime->volatileGenerationId();
    retiredGenerationReadLeases_ = activeGenerationReadLeases_;
    activeGenerationReadLeases_ = 0U;
    retirementOwnerPending_ = true;
    retiredGraph.swap(currentGraph_);
    currentGraph_.swap(preparedGraph);
    (void)persistent;
}

bool ConfigurationService::completeRuntimeRetirement(
    std::uint64_t generationId) noexcept {
    const std::lock_guard<std::mutex> lock(stateMutex_);
    if (!retiredGenerationId_.has_value() ||
        *retiredGenerationId_ != generationId) {
        enterFailClosedLocked(ConfigurationServiceMode::RuntimeFailure,
                              ConfigurationRuntimeFailureCause::
                                  ConfigurationModelBudgetInvariantViolation);
        return false;
    }
    retirementOwnerPending_ = false;
    if (retiredGenerationReadLeases_ == 0U) {
        retiredGenerationId_.reset();
        if (!previewBuildReservation_.has_value() && !visiblePreview_ &&
            !capturedPreview_ && !resolutionContext_) {
            previewModelReserved_ = false;
        }
    }
    return true;
}

void ConfigurationService::invokeTestHook(TestPoint point) const {
    if (testHook_ != nullptr) {
        testHook_(testHookContext_, point);
    }
}

void ConfigurationService::invalidateRuntimePreparationBindingForTest() {
    const std::lock_guard<std::mutex> lock(stateMutex_);
    if (resolutionContext_) {
        resolutionContext_->runtimeBinding.preparedTimeZoneIdentifier =
            "test-invalid-binding";
    }
}

void ConfigurationService::rejectRuntimePreparationForTest(
    bool reject) noexcept {
    rejectRuntimePreparationForTest_.store(reject, std::memory_order_release);
}

bool ConfigurationService::runtimePreparationRetryConsumedForTest()
    const noexcept {
    const std::lock_guard<std::mutex> lock(stateMutex_);
    return resolutionContext_ &&
           resolutionContext_->runtimePreparationRetryConsumed;
}

}  // namespace fermentation
