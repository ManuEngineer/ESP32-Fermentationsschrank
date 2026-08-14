#include "restart_episode.hpp"

#include <limits>

namespace fermentation {
namespace {

bool isAbnormal(RestartCauseEvent cause) {
    return cause == RestartCauseEvent::ControlledSafety ||
           cause == RestartCauseEvent::WatchdogOrPanic ||
           cause == RestartCauseEvent::Brownout;
}

bool sameControlledEvidence(const SafetyStateRecord& record) {
    return record.restartEvidence.state == RestartEvidenceState::Committed &&
           record.restartEvidence.cause ==
               RestartCauseEvent::ControlledSafety &&
           record.restartEvidence.evidenceId != 0U &&
           record.restartEvidence.faultInstanceId.valid();
}

void setSafeBoot(SafetyStateRecord& record) { record.safeBootRequired = true; }

}  // namespace

RestartBootEvaluation RestartEpisodeCoordinator::evaluateBoot(
    SafetyStateRecord& record,
    const device_platform::ResetCauseSnapshot& snapshot) {
    RestartBootEvaluation result;
    if (!snapshot.valid || snapshot.observationId == 0U) {
        setSafeBoot(record);
        result.status = RestartBootStatus::UnknownFailClosed;
        result.safeBootRequired = true;
        return result;
    }
    // The reset port exposes one boot-local stable observation. Re-evaluating
    // it must never create another episode/evidence record or consume another
    // intent. A changed cause under the same observation id is itself unsafe.
    if (record.lastResetObservationId == snapshot.observationId) {
        result.cause = classifyRestartCause(snapshot.cause);
        if (snapshot.cause != record.lastResetCause) {
            setSafeBoot(record);
            result.status = RestartBootStatus::EvidenceMismatch;
            result.safeBootRequired = true;
            result.recordNeedsCommit = false;
            return result;
        }
        result.evidenceId = record.restartEvidence.evidenceId;
        switch (result.cause) {
            case RestartCauseEvent::Authorized:
                result.status = record.faultResetBootIntent.pending
                                    ? RestartBootStatus::EvidenceMismatch
                                    : RestartBootStatus::AuthorizedReset;
                break;
            case RestartCauseEvent::ControlledSafety:
                result.status =
                    record.restartEvidence.state ==
                            RestartEvidenceState::Consumed
                        ? RestartBootStatus::ControlledEvidenceConsumed
                        : RestartBootStatus::EvidenceMismatch;
                break;
            case RestartCauseEvent::WatchdogOrPanic:
            case RestartCauseEvent::Brownout:
                result.status = record.restartEpisode.abnormalRestartCount >= 3U
                                    ? RestartBootStatus::SafeBootRequired
                                    : RestartBootStatus::AbnormalRecorded;
                break;
            case RestartCauseEvent::PowerOn:
                result.status = RestartBootStatus::Normal;
                break;
            case RestartCauseEvent::Unknown:
                result.status = RestartBootStatus::UnknownFailClosed;
                break;
        }
        result.safeBootRequired = record.safeBootRequired;
        return result;
    }
    record.lastResetCause = snapshot.cause;
    record.lastResetObservationId = snapshot.observationId;
    result.cause = classifyRestartCause(snapshot.cause);
    result.recordNeedsCommit = true;

    if (result.cause == RestartCauseEvent::Unknown) {
        setSafeBoot(record);
        result.status = RestartBootStatus::UnknownFailClosed;
        result.safeBootRequired = true;
        return result;
    }

    if (record.faultResetBootIntent.pending &&
        result.cause != RestartCauseEvent::Authorized) {
        setSafeBoot(record);
        result.status = RestartBootStatus::EvidenceMismatch;
        result.safeBootRequired = true;
        return result;
    }
    if (record.restartEvidence.state == RestartEvidenceState::Pending ||
        (record.restartEvidence.state == RestartEvidenceState::Committed &&
         result.cause != RestartCauseEvent::ControlledSafety)) {
        setSafeBoot(record);
        result.status = RestartBootStatus::EvidenceMismatch;
        result.safeBootRequired = true;
        result.recordNeedsCommit = true;
        return result;
    }

    if (result.cause == RestartCauseEvent::Authorized) {
        if (record.faultResetBootIntent.pending) {
            record.faultResetBootIntent.pending = false;
            result.status = RestartBootStatus::AuthorizedReset;
        } else {
            result.status = RestartBootStatus::Normal;
        }
        result.safeBootRequired = record.safeBootRequired;
        return result;
    }

    if (result.cause == RestartCauseEvent::ControlledSafety) {
        if (!sameControlledEvidence(record)) {
            setSafeBoot(record);
            result.status = RestartBootStatus::EvidenceMismatch;
            result.safeBootRequired = true;
            result.recordNeedsCommit = true;
            return result;
        }
        record.restartEvidence.state = RestartEvidenceState::Consumed;
        result.status = RestartBootStatus::ControlledEvidenceConsumed;
        result.evidenceId = record.restartEvidence.evidenceId;
        result.safeBootRequired = record.safeBootRequired;
        return result;
    }

    if (!isAbnormal(result.cause)) {
        result.status = RestartBootStatus::Normal;
        result.safeBootRequired = record.safeBootRequired;
        return result;
    }

    // A watchdog/Brownout has no pre-write evidence. It is recorded exactly
    // once by this boot observation. A committed controlled evidence was
    // handled above and therefore cannot be incremented again.
    const bool newEpisode = !record.restartEpisode.open;
    if (record.restartEpisode.nextRestartEvidenceId == 0U ||
        (newEpisode && record.restartEpisode.episodeId ==
                           std::numeric_limits<std::uint32_t>::max()) ||
        record.restartEpisode.nextRestartEvidenceId ==
            std::numeric_limits<std::uint32_t>::max() ||
        record.restartEpisode.abnormalRestartCount ==
            std::numeric_limits<std::uint32_t>::max()) {
        setSafeBoot(record);
        result.status = RestartBootStatus::Overflow;
        result.safeBootRequired = true;
        return result;
    }
    if (newEpisode) ++record.restartEpisode.episodeId;
    ++record.restartEpisode.nextRestartEvidenceId;
    ++record.restartEpisode.abnormalRestartCount;
    record.restartEpisode.lastRestartEvidenceId =
        record.restartEpisode.nextRestartEvidenceId - 1U;
    record.restartEpisode.open = true;
    record.restartEvidence.evidenceId =
        record.restartEpisode.lastRestartEvidenceId;
    record.restartEvidence.cause = result.cause;
    record.restartEvidence.state = RestartEvidenceState::Consumed;
    result.evidenceId = record.restartEvidence.evidenceId;
    result.status = RestartBootStatus::AbnormalRecorded;
    result.recordNeedsCommit = true;
    if (record.restartEpisode.abnormalRestartCount >= 3U) {
        setSafeBoot(record);
        result.status = RestartBootStatus::SafeBootRequired;
        result.safeBootRequired = true;
    }
    return result;
}

bool RestartEpisodeCoordinator::prepareControlledRestart(
    SafetyStateRecord& record, FaultInstanceId faultId,
    std::uint32_t faultRevision) {
    const bool newEpisode = !record.restartEpisode.open;
    if (!faultId.valid() || faultRevision == 0U ||
        record.restartEvidence.state == RestartEvidenceState::Pending ||
        record.restartEvidence.state == RestartEvidenceState::Committed ||
        record.restartEpisode.nextRestartEvidenceId == 0U ||
        (newEpisode && record.restartEpisode.episodeId ==
                           std::numeric_limits<std::uint32_t>::max()) ||
        record.restartEpisode.nextRestartEvidenceId ==
            std::numeric_limits<std::uint32_t>::max() ||
        record.restartEpisode.abnormalRestartCount ==
            std::numeric_limits<std::uint32_t>::max()) {
        return false;
    }
    if (newEpisode) ++record.restartEpisode.episodeId;
    ++record.restartEpisode.nextRestartEvidenceId;
    ++record.restartEpisode.abnormalRestartCount;
    record.restartEpisode.open = true;
    record.restartEpisode.lastRestartEvidenceId =
        record.restartEpisode.nextRestartEvidenceId - 1U;
    record.restartEvidence.evidenceId =
        record.restartEpisode.lastRestartEvidenceId;
    record.restartEvidence.cause = RestartCauseEvent::ControlledSafety;
    record.restartEvidence.state = RestartEvidenceState::Committed;
    record.restartEvidence.faultInstanceId = faultId;
    record.faultRevision = faultRevision;
    if (record.restartEpisode.abnormalRestartCount >= 3U) {
        record.safeBootRequired = true;
    }
    return true;
}

bool RestartEpisodeCoordinator::prepareFaultResetBootIntent(
    SafetyStateRecord& record, FaultInstanceId faultId,
    std::uint32_t faultRevision) {
    if (!faultId.valid() || faultRevision == 0U ||
        record.faultResetBootIntent.pending ||
        record.recordRevision == std::numeric_limits<std::uint32_t>::max()) {
        return false;
    }
    record.faultResetBootIntent.targetFault = faultId;
    record.faultResetBootIntent.expectedFaultRevision = faultRevision;
    record.faultResetBootIntent.intentRevision = record.recordRevision + 1U;
    record.faultResetBootIntent.pending = true;
    return true;
}

bool RestartEpisodeCoordinator::advanceStableWindow(SafetyStateRecord& record,
                                                    std::uint64_t now,
                                                    bool stable) {
    if (!record.restartEpisode.open || record.safeBootRequired || !stable) {
        record.restartEpisode.stableWindowRunning = false;
        return false;
    }
    if (!record.restartEpisode.stableWindowRunning) {
        record.restartEpisode.stableWindowRunning = true;
        record.restartEpisode.stableWindowStartedAtMillis = now;
        return false;
    }
    if (now < record.restartEpisode.stableWindowStartedAtMillis ||
        now - record.restartEpisode.stableWindowStartedAtMillis <
            kStableRestartWindowMillis) {
        return false;
    }
    const std::uint32_t episodeId = record.restartEpisode.episodeId;
    const std::uint32_t nextEvidenceId =
        record.restartEpisode.nextRestartEvidenceId;
    record.restartEpisode = RestartEpisodeEvidence{};
    record.restartEpisode.episodeId = episodeId;
    record.restartEpisode.nextRestartEvidenceId = nextEvidenceId;
    return true;
}

}  // namespace fermentation
