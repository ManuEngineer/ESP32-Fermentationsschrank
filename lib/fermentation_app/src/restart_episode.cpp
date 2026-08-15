#include "restart_episode.hpp"

#include <limits>

namespace fermentation {
namespace {

bool isAbnormal(RestartCauseEvent cause) {
    return cause == RestartCauseEvent::WatchdogOrPanic ||
           cause == RestartCauseEvent::Brownout ||
           cause == RestartCauseEvent::ExternalOrOther;
}

bool isValidIntent(RestartIntentType intent) {
    return intent == RestartIntentType::AutomaticSafetyRecovery ||
           intent == RestartIntentType::AuthorizedTechnicalRestart ||
           intent == RestartIntentType::AuthorizedSafeBootExit;
}

void setSafeBoot(SafetyStateRecord& record) { record.safeBootRequired = true; }

bool incrementEpisode(SafetyStateRecord& record) {
    const bool newEpisode = !record.restartEpisode.open;
    if (record.restartEpisode.nextRestartEvidenceId == 0U ||
        record.restartEpisode.nextRestartEvidenceId ==
            std::numeric_limits<std::uint32_t>::max() ||
        record.restartEpisode.abnormalRestartCount ==
            std::numeric_limits<std::uint32_t>::max() ||
        (newEpisode && record.restartEpisode.episodeId ==
                           std::numeric_limits<std::uint32_t>::max())) {
        return false;
    }
    if (newEpisode) ++record.restartEpisode.episodeId;
    ++record.restartEpisode.nextRestartEvidenceId;
    ++record.restartEpisode.abnormalRestartCount;
    record.restartEpisode.lastRestartEvidenceId =
        record.restartEpisode.nextRestartEvidenceId - 1U;
    record.restartEpisode.open = true;
    record.restartEpisode.stableWindowRunning = false;
    record.restartEpisode.stableWindowStartedAtMillis = 0U;
    return true;
}

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

    result.cause = classifyRestartCause(snapshot.cause);
    if (record.lastResetObservationId == snapshot.observationId) {
        if (record.lastResetCause != snapshot.cause) {
            setSafeBoot(record);
            result.status = RestartBootStatus::EvidenceMismatch;
            result.safeBootRequired = true;
            return result;
        }
        result.evidenceId = record.restartEvidence.evidenceId;
        if (result.cause == RestartCauseEvent::Unknown) {
            result.status = RestartBootStatus::UnknownFailClosed;
            setSafeBoot(record);
        } else if (result.cause == RestartCauseEvent::SoftwareRestart) {
            result.status =
                record.restartEvidence.state == RestartEvidenceState::Consumed
                    ? RestartBootStatus::ControlledEvidenceConsumed
                    : RestartBootStatus::EvidenceMismatch;
            if (result.status == RestartBootStatus::EvidenceMismatch) {
                setSafeBoot(record);
            }
        } else if (record.restartEvidence.state ==
                       RestartEvidenceState::Pending ||
                   record.restartEvidence.state ==
                       RestartEvidenceState::Committed) {
            setSafeBoot(record);
            result.status = RestartBootStatus::EvidenceMismatch;
        } else if (isAbnormal(result.cause) &&
                   record.restartEpisode.abnormalRestartCount >= 3U) {
            result.status = RestartBootStatus::SafeBootRequired;
            setSafeBoot(record);
        } else {
            result.status = RestartBootStatus::Normal;
        }
        result.safeBootRequired = record.safeBootRequired;
        return result;
    }

    record.lastResetCause = snapshot.cause;
    record.lastResetObservationId = snapshot.observationId;
    result.recordNeedsCommit = true;

    if (result.cause == RestartCauseEvent::Unknown) {
        setSafeBoot(record);
        result.status = RestartBootStatus::UnknownFailClosed;
        result.safeBootRequired = true;
        return result;
    }

    if (result.cause == RestartCauseEvent::SoftwareRestart) {
        const bool matchingEvidence =
            record.restartEvidence.state == RestartEvidenceState::Committed &&
            record.restartEvidence.cause ==
                RestartCauseEvent::SoftwareRestart &&
            record.restartEvidence.evidenceId != 0U &&
            isValidIntent(record.restartEvidence.intent) &&
            record.restartEvidence.episodeId == record.restartEpisode.episodeId;
        if (!matchingEvidence) {
            setSafeBoot(record);
            result.status = RestartBootStatus::EvidenceMismatch;
            result.safeBootRequired = true;
            return result;
        }
        const auto intent = record.restartEvidence.intent;
        record.restartEvidence.state = RestartEvidenceState::Consumed;
        result.evidenceId = record.restartEvidence.evidenceId;
        result.status = intent == RestartIntentType::AutomaticSafetyRecovery
                            ? RestartBootStatus::ControlledEvidenceConsumed
                            : RestartBootStatus::AuthorizedReset;
        result.safeBootRequired = record.safeBootRequired;
        return result;
    }

    // A previous consumed application evidence is no longer eligible for a
    // later boot. Keeping the episode itself is intentional.
    if (record.restartEvidence.state == RestartEvidenceState::Consumed) {
        record.restartEvidence = PersistedRestartEvidence{};
    }
    if (record.restartEvidence.state == RestartEvidenceState::Pending ||
        record.restartEvidence.state == RestartEvidenceState::Committed) {
        setSafeBoot(record);
        result.status = RestartBootStatus::EvidenceMismatch;
        result.safeBootRequired = true;
        return result;
    }

    if (isAbnormal(result.cause)) {
        if (!incrementEpisode(record)) {
            setSafeBoot(record);
            result.status = RestartBootStatus::Overflow;
            result.safeBootRequired = true;
            return result;
        }
        result.evidenceId = record.restartEpisode.lastRestartEvidenceId;
        result.status = record.restartEpisode.abnormalRestartCount >= 3U
                            ? RestartBootStatus::SafeBootRequired
                            : RestartBootStatus::AbnormalRecorded;
        if (result.status == RestartBootStatus::SafeBootRequired) {
            setSafeBoot(record);
        }
        result.safeBootRequired = record.safeBootRequired;
        return result;
    }

    result.status = RestartBootStatus::Normal;
    result.safeBootRequired = record.safeBootRequired;
    return result;
}

bool RestartEpisodeCoordinator::prepareControlledRestart(
    SafetyStateRecord& record, FaultInstanceId faultId,
    std::uint32_t faultRevision) {
    return prepareRestartIntent(record,
                                RestartIntentType::AutomaticSafetyRecovery,
                                faultId, faultRevision);
}

bool RestartEpisodeCoordinator::prepareRestartIntent(
    SafetyStateRecord& record, RestartIntentType intent,
    FaultInstanceId faultId, std::uint32_t faultRevision) {
    if (!isValidIntent(intent) || faultRevision == 0U ||
        (intent == RestartIntentType::AutomaticSafetyRecovery &&
         !faultId.valid()) ||
        record.restartEvidence.state == RestartEvidenceState::Pending ||
        record.restartEvidence.state == RestartEvidenceState::Committed ||
        record.recordRevision == std::numeric_limits<std::uint32_t>::max() ||
        (intent != RestartIntentType::AutomaticSafetyRecovery &&
         record.restartEpisode.abnormalRestartCount >= 3U)) {
        return false;
    }

    if (intent == RestartIntentType::AutomaticSafetyRecovery) {
        if (!incrementEpisode(record)) return false;
    } else {
        const bool newEpisode = !record.restartEpisode.open;
        if (record.restartEpisode.nextRestartEvidenceId == 0U ||
            record.restartEpisode.nextRestartEvidenceId ==
                std::numeric_limits<std::uint32_t>::max() ||
            (newEpisode && record.restartEpisode.episodeId ==
                               std::numeric_limits<std::uint32_t>::max())) {
            return false;
        }
        if (newEpisode) {
            ++record.restartEpisode.episodeId;
            record.restartEpisode.open = true;
        }
        record.restartEpisode.lastRestartEvidenceId =
            record.restartEpisode.nextRestartEvidenceId;
        ++record.restartEpisode.nextRestartEvidenceId;
        record.restartEpisode.stableWindowRunning = false;
        record.restartEpisode.stableWindowStartedAtMillis = 0U;
    }
    if (record.restartEpisode.lastRestartEvidenceId == 0U ||
        record.restartEpisode.episodeId == 0U) {
        return false;
    }
    record.restartEvidence.evidenceId =
        record.restartEpisode.lastRestartEvidenceId;
    record.restartEvidence.cause = RestartCauseEvent::SoftwareRestart;
    record.restartEvidence.state = RestartEvidenceState::Committed;
    record.restartEvidence.intent = intent;
    record.restartEvidence.targetFault = faultId;
    record.restartEvidence.targetFaultRevision = faultRevision;
    record.restartEvidence.episodeId = record.restartEpisode.episodeId;
    record.restartEvidence.evidenceRevision = record.recordRevision + 1U;
    if (record.restartEpisode.abnormalRestartCount >= 3U) {
        setSafeBoot(record);
    }
    return true;
}

bool RestartEpisodeCoordinator::advanceStableWindow(SafetyStateRecord& record,
                                                    std::uint64_t now,
                                                    bool stable) {
    if (!record.restartEpisode.open || record.safeBootRequired || !stable) {
        record.restartEpisode.stableWindowRunning = false;
        record.restartEpisode.stableWindowStartedAtMillis = 0U;
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
    const auto episodeId = record.restartEpisode.episodeId;
    const auto nextEvidenceId = record.restartEpisode.nextRestartEvidenceId;
    record.restartEpisode = RestartEpisodeEvidence{};
    record.restartEpisode.episodeId = episodeId;
    record.restartEpisode.nextRestartEvidenceId = nextEvidenceId;
    return true;
}

}  // namespace fermentation
