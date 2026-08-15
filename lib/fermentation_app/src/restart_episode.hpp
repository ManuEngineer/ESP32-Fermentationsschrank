#pragma once

#include <cstdint>

#include "reset_port.hpp"
#include "safety_state_store.hpp"
#include "time_source.hpp"

namespace fermentation {

enum class RestartBootStatus : std::uint8_t {
    Normal,
    AuthorizedReset,
    AbnormalRecorded,
    ControlledEvidenceConsumed,
    SafeBootRequired,
    UnknownFailClosed,
    EvidenceMismatch,
    Overflow,
};

struct RestartBootEvaluation {
    RestartBootStatus status{RestartBootStatus::UnknownFailClosed};
    RestartCauseEvent cause{RestartCauseEvent::Unknown};
    bool recordNeedsCommit{false};
    bool safeBootRequired{false};
    std::uint32_t evidenceId{0U};
};

// Fachlicher Episodekern ohne Plattformwerte und ohne eigenen Persistenzport.
// Der Aufrufer committed die mutierte SafetyStateRecord ueber
// SafetyStateStore; dadurch bleibt Write-before-Apply sichtbar.
class RestartEpisodeCoordinator final {
   public:
    [[nodiscard]] RestartBootEvaluation evaluateBoot(
        SafetyStateRecord& record,
        const device_platform::ResetCauseSnapshot& snapshot);

    [[nodiscard]] bool prepareControlledRestart(SafetyStateRecord& record,
                                                FaultInstanceId faultId,
                                                std::uint32_t faultRevision);

    [[nodiscard]] bool prepareRestartIntent(SafetyStateRecord& record,
                                            RestartIntentType intent,
                                            FaultInstanceId faultId,
                                            std::uint32_t faultRevision);

    [[nodiscard]] bool advanceStableWindow(SafetyStateRecord& record,
                                           std::uint64_t monotonicMillis,
                                           bool stable);
};

}  // namespace fermentation
