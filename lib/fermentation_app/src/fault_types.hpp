#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

#include "event_journal.hpp"

namespace fermentation {

class SafetyFaultService;

// The in-memory core retains 17 persistent safety/system slots plus four
// simultaneously possible non-latched observations: P1-001, one product
// O2-001, and one O2-002 per independent safety sensor role (cabinet air and
// cooling). Y4-006 is a record marker and never consumes a persistent slot.
inline constexpr std::size_t kMaximumActiveFaults = 21U;

enum class FaultClass : std::uint8_t {
    ProcessWarning = 1U,
    OperatingFault = 2U,
    LatchedSafetyFault = 3U,
    LatchedSystemFault = 4U,
    Unknown = 0xFFU,
    PROCESS_WARNING = ProcessWarning,
    OPERATING_FAULT = OperatingFault,
    LATCHED_SAFETY_FAULT = LatchedSafetyFault,
    LATCHED_SYSTEM_FAULT = LatchedSystemFault,
};

// Numerische Werte sind stabiler Wire-/Diagnosebestandteil. Unbekannte Werte
// werden niemals in einen harmlosen Code umgedeutet.
enum class FaultCode : std::uint16_t {
    P1_001 = 0x1001U,
    O2_001 = 0x2001U,
    O2_002 = 0x2002U,
    S3_001 = 0x3001U,
    S3_002 = 0x3002U,
    S3_003 = 0x3003U,
    S3_004 = 0x3004U,
    S3_005 = 0x3005U,
    S3_006 = 0x3006U,
    S3_007 = 0x3007U,
    S3_008 = 0x3008U,
    S3_009 = 0x3009U,
    Y4_001 = 0x4001U,
    Y4_002 = 0x4002U,
    Y4_003 = 0x4003U,
    Y4_004 = 0x4004U,
    Y4_005 = 0x4005U,
    Y4_006 = 0x4006U,
    Y4_007 = 0x4007U,
    Y4_008 = 0x4008U,
    Y4_009 = 0x4009U,
    Unknown = 0xFFFFU,
};

struct FaultInstanceId {
    std::uint32_t value{0U};

    [[nodiscard]] bool valid() const { return value != 0U; }
    friend bool operator==(FaultInstanceId left, FaultInstanceId right) {
        return left.value == right.value;
    }
    friend bool operator!=(FaultInstanceId left, FaultInstanceId right) {
        return !(left == right);
    }
};

enum class FaultStatus : std::uint8_t {
    ActiveUnacknowledged,
    ActiveAcknowledged,
    CauseClearedLocked,
    Cleared,
};

// Bounded diagnostic origin for the single active Y4-008 instance. It never
// participates in the bounded correlation identity (Y4-008 always dedupes to
// one system-wide instance); it only records which real domain most recently
// reported the unknown/mismatched evidence, so only that domain's matching
// real clearance path may resolve the cause.
enum class FaultDiagnosticOrigin : std::uint8_t {
    Unknown = 0U,
    Boot = 1U,
    Configuration = 2U,
    Sensor = 3U,
    Process = 4U,
};

enum class SafetyDisposition : std::uint8_t {
    Allowed,
    ImmediateStop,
    SafetyRecovery,
};

struct FaultRecord {
    FaultInstanceId instanceId;
    FaultCode code{FaultCode::Unknown};
    FaultClass faultClass{FaultClass::LatchedSystemFault};
    std::uint32_t sourceKey{0U};
    std::uint32_t correlationKey{0U};
    std::uint32_t creationSequence{0U};
    std::uint64_t createdAtMonotonicMillis{0U};
    FaultStatus status{FaultStatus::ActiveUnacknowledged};
    SafetyDisposition disposition{SafetyDisposition::ImmediateStop};
    bool causeActive{true};
    bool latched{true};
    bool automaticRecoveryRestartUsed{false};
    std::uint32_t faultRevision{0U};
    std::optional<FaultInstanceId> primaryFaultId;
    // #23 diagnostic evidence is deliberately separate from the bounded
    // correlation key and is never truncated.
    std::uint64_t diagnosticSequenceHighWatermark{0U};
    // Only meaningful for Y4-008; ignored for every other code.
    FaultDiagnosticOrigin diagnosticOrigin{FaultDiagnosticOrigin::Unknown};
};

enum class FaultRaiseStatus : std::uint8_t {
    Created,
    Existing,
    Reactivated,
    InvalidInput,
    CapacityReached,
    RevisionOverflow,
};

struct FaultRaiseRequest {
    FaultCode code{FaultCode::Unknown};
    std::uint32_t sourceKey{0U};
    std::uint32_t correlationKey{0U};
    std::uint64_t monotonicMillis{0U};
    std::optional<FaultInstanceId> primaryFaultId;
    std::uint64_t diagnosticSequenceHighWatermark{0U};
    FaultDiagnosticOrigin diagnosticOrigin{FaultDiagnosticOrigin::Unknown};
};

struct FaultRaiseResult {
    FaultRaiseStatus status{FaultRaiseStatus::InvalidInput};
    FaultInstanceId instanceId;
};

struct FaultCoreSnapshot {
    std::array<FaultRecord, kMaximumActiveFaults> records{};
    std::size_t count{0U};
    std::uint32_t revision{0U};
    std::uint32_t instanceSequenceHighWatermark{0U};
    bool criticalSafetyEventPending{false};
};

[[nodiscard]] bool isKnownFaultClass(FaultClass value);
[[nodiscard]] bool isKnownFaultCode(FaultCode value);
[[nodiscard]] FaultCode normalizeFaultCode(FaultCode value);
[[nodiscard]] FaultClass faultClassForCode(FaultCode value);
[[nodiscard]] std::uint8_t faultCodePriority(FaultCode value);
[[nodiscard]] const char* faultCodeText(FaultCode value);
[[nodiscard]] bool isLatchedFaultClass(FaultClass value);
[[nodiscard]] bool allowsAutomaticRecoveryRestart(FaultCode value);
[[nodiscard]] bool isBlockingFault(const FaultRecord& record);
[[nodiscard]] bool equalFaultCoreSnapshot(const FaultCoreSnapshot& left,
                                          const FaultCoreSnapshot& right);

class FaultCore final {
   public:
    FaultCore() = default;

    [[nodiscard]] FaultRaiseResult raise(const FaultRaiseRequest& request);
    // True when `request` would update an already-active instance (Existing,
    // Reactivated, or a bounded-domain diagnostic update) rather than
    // requiring a new persistent slot. The 17-slot bound must only ever be
    // enforced when this is false, so a capacity gate never misfires against
    // a mere repeat or reactivation of an already-counted cause.
    [[nodiscard]] bool correlatesToExistingInstance(
        const FaultRaiseRequest& request) const;
    [[nodiscard]] bool acknowledge(FaultInstanceId id,
                                   std::uint32_t expectedRevision);
    [[nodiscard]] bool markCauseCleared(FaultInstanceId id,
                                        std::uint32_t expectedRevision);
    [[nodiscard]] bool markControlledRestartUsed(
        FaultInstanceId id, std::uint32_t expectedRevision);

    // Nur ein bereits als Ursache beseitigter, nicht mehr blockierender Fault
    // darf so als CLEARED markiert werden. Persistenter Reset und Boot-Intent
    // liegen darueber in safety_state_store.hpp.
    [[nodiscard]] bool clearAfterVerifiedReset(FaultInstanceId id,
                                               std::uint32_t expectedRevision);
    [[nodiscard]] bool restoreSnapshot(const FaultCoreSnapshot& snapshot);

    [[nodiscard]] const FaultRecord* find(FaultInstanceId id) const;
    [[nodiscard]] const FaultRecord* dominant() const;
    [[nodiscard]] SafetyDisposition disposition() const;
    [[nodiscard]] bool hasBlockingFault() const;
    [[nodiscard]] FaultCoreSnapshot snapshot() const;

   private:
    friend class SafetyFaultService;

    // Y4-009 is the restart-tracking latch itself. It is cleared only by the
    // explicit, technically authorized SAFE_BOOT-exit path; ordinary reboot,
    // cause clearing, and generic re-arm paths cannot call this operation.
    [[nodiscard]] bool clearAfterAuthorizedSafeBootExit(
        FaultInstanceId id, std::uint32_t expectedRevision);
    [[nodiscard]] bool incrementRevision();
    [[nodiscard]] FaultRecord* findMutable(FaultInstanceId id);
    [[nodiscard]] const FaultRecord* findCorrelationConst(
        const FaultRaiseRequest& request) const;
    [[nodiscard]] FaultRecord* findCorrelation(
        const FaultRaiseRequest& request);
    void recomputeProjection();
    void compactClearedRecords();
    void installUnknownPersistenceFault();

    FaultCoreSnapshot state_;
    std::uint32_t nextInstanceId_{1U};
};

enum class FaultEventType : std::uint8_t {
    FaultCreated,
    FaultEscalated,
    FaultCauseCleared,
    FaultAcknowledged,
    FaultResetCommitted,
    FaultResetRejected,
    RestartEpisodeAdvanced,
    RestartEpisodeClosed,
    SafeBootEntered,
    SafeBootExitDecided,
    SafeBootExitRejected,
    SafetyRecoveryAttempted,
    SafetyRecoveryAborted,
    SafetyRecoverySucceeded,
};

struct FaultEventProjection {
    FaultEventType type{FaultEventType::FaultCreated};
    FaultCode code{FaultCode::Unknown};
    FaultInstanceId faultInstanceId;
    std::optional<FaultInstanceId> primaryFaultId;
    std::uint32_t faultRevision{0U};
    std::uint32_t episodeId{0U};
    std::uint32_t restartEvidenceId{0U};
    std::uint64_t diagnosticSequenceHighWatermark{0U};
    bool accepted{false};
};

[[nodiscard]] const char* faultEventTypeText(FaultEventType type);
[[nodiscard]] std::string serializeFaultEvent(
    const FaultEventProjection& projection);
[[nodiscard]] bool recordFaultEvent(device_platform::IEventJournal* journal,
                                    std::uint64_t monotonicMillis,
                                    const FaultEventProjection& projection);

}  // namespace fermentation
