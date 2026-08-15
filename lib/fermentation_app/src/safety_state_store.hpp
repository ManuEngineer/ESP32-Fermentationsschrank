#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

#include "fault_types.hpp"
#include "reset_port.hpp"
#include "state_store.hpp"
#include "storage_types.hpp"

namespace fermentation {

inline constexpr std::uint32_t kSafetyStateRecordSchema = 3U;
inline constexpr std::size_t kMaximumPersistedLatches = 17U;
inline constexpr std::size_t kMaximumSafetyRecordBytes = 2048U;
inline constexpr std::uint64_t kStableRestartWindowMillis = 30U * 60U * 1000U;
inline constexpr std::size_t kSafetyRecordBasePayloadBytes = 128U;
inline constexpr std::size_t kSafetyRecordSlotPayloadBytes = 64U;
inline constexpr std::size_t kSafetyRecordPayloadBytes =
    kSafetyRecordBasePayloadBytes +
    kMaximumPersistedLatches * kSafetyRecordSlotPayloadBytes;
static_assert(kSafetyRecordPayloadBytes == 1216U);
static_assert(37U + kSafetyRecordPayloadBytes <= kMaximumSafetyRecordBytes);

enum class RestartCauseEvent : std::uint8_t {
    SoftwareRestart,
    WatchdogOrPanic,
    Brownout,
    PowerOn,
    ExternalOrOther,
    Unknown,
};

enum class RestartIntentType : std::uint8_t {
    None,
    AutomaticSafetyRecovery,
    AuthorizedTechnicalRestart,
    AuthorizedSafeBootExit,
    Unknown,
};

enum class RestartEvidenceState : std::uint8_t {
    None,
    Pending,
    Committed,
    Consumed,
};

struct RestartEpisodeEvidence {
    std::uint32_t episodeId{0U};
    std::uint32_t abnormalRestartCount{0U};
    std::uint32_t lastRestartEvidenceId{0U};
    std::uint32_t nextRestartEvidenceId{1U};
    bool open{false};
    bool stableWindowRunning{false};
    std::uint64_t stableWindowStartedAtMillis{0U};
};

struct PersistedRestartEvidence {
    std::uint32_t evidenceId{0U};
    std::uint64_t authorizationEvidenceId{0U};
    RestartCauseEvent cause{RestartCauseEvent::Unknown};
    RestartEvidenceState state{RestartEvidenceState::None};
    RestartIntentType intent{RestartIntentType::None};
    FaultInstanceId targetFault;
    std::uint32_t targetFaultRevision{0U};
    std::uint32_t episodeId{0U};
    std::uint32_t evidenceRevision{0U};
};

struct SafetyStateRecord {
    std::uint32_t schemaVersion{kSafetyStateRecordSchema};
    std::uint32_t recordRevision{1U};
    std::uint32_t faultRevision{0U};
    std::uint32_t faultInstanceSequence{0U};
    bool safeBootRequired{false};
    FaultCode dominantCode{FaultCode::Unknown};
    device_platform::StorageEpoch storageEpoch{1U};
    std::array<FaultRecord, kMaximumPersistedLatches> latches{};
    std::size_t latchCount{0U};
    bool capacityFailureLatched{false};
    std::uint32_t capacityFailureRevision{0U};
    std::uint32_t capacityFailureSourceKey{0U};
    std::uint32_t capacityFailureCorrelationKey{0U};
    RestartEpisodeEvidence restartEpisode;
    PersistedRestartEvidence restartEvidence;
    device_platform::ResetCause lastResetCause{
        device_platform::ResetCause::Unknown};
    std::uint64_t lastResetObservationId{0U};
};

enum class SafetyRecordValidation : std::uint8_t {
    Valid,
    InvalidSchema,
    InvalidField,
    InvalidCapacity,
    InvalidRelationship,
};

[[nodiscard]] SafetyRecordValidation validateSafetyStateRecord(
    const SafetyStateRecord& record);

enum class SafetyRecordEncodeStatus : std::uint8_t {
    Success,
    InvalidRecord,
    CapacityExceeded,
};

enum class SafetyRecordDecodeStatus : std::uint8_t {
    Success,
    InvalidEnvelope,
    InvalidRecord,
};

[[nodiscard]] SafetyRecordEncodeStatus encodeSafetyStateRecord(
    const SafetyStateRecord& record, std::string& outBytes,
    std::size_t maxBytes = kMaximumSafetyRecordBytes);
[[nodiscard]] SafetyRecordDecodeStatus decodeSafetyStateRecord(
    const std::string& bytes, SafetyStateRecord& outRecord);

struct FactoryNewSafetyProof {
    bool allRequiredBootstrapReadsSuccessful{false};
    bool explicitFactoryNewState{false};
    bool allSafetyRecordReadsNotFound{false};

    [[nodiscard]] bool valid() const {
        return allRequiredBootstrapReadsSuccessful && explicitFactoryNewState &&
               allSafetyRecordReadsNotFound;
    }
};

enum class SafetyRecordLoadStatus : std::uint8_t {
    Loaded,
    FactoryInitialized,
    NotFoundOutsideFactoryBootstrap,
    ReadError,
    CapacityError,
    Corrupt,
};

struct SafetyRecordLoadResult {
    SafetyRecordLoadStatus status{SafetyRecordLoadStatus::Corrupt};
    SafetyStateRecord record;
};

enum class SafetyRecordCommitStatus : std::uint8_t {
    Committed,
    InvalidRecord,
    CapacityError,
    WriteError,
    CommitOutcomeUnknown,
    ReadbackError,
    ReadbackMismatch,
};

struct SafetyRecordCommitResult {
    SafetyRecordCommitStatus status{SafetyRecordCommitStatus::InvalidRecord};
};

// Verwendet den bestehenden IStateStore. Die Anwendung besitzt die
// Schluesselbedeutung und die Record-/Recoverysemantik; der Plattformport
// bleibt generisch.
class SafetyStateStore final {
   public:
    explicit SafetyStateStore(device_platform::IStateStore& store);

    [[nodiscard]] SafetyRecordLoadResult load(
        const FactoryNewSafetyProof& factoryProof = {});
    [[nodiscard]] SafetyRecordCommitResult commit(
        const SafetyStateRecord& record);

    [[nodiscard]] const device_platform::StateStoreKey& key() const {
        return key_;
    }

   private:
    device_platform::IStateStore& store_;
    device_platform::StateStoreKey key_;
};

[[nodiscard]] RestartCauseEvent classifyRestartCause(
    device_platform::ResetCause cause);

}  // namespace fermentation
