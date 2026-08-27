#include "run_snapshot.hpp"

#include <utility>

namespace fermentation {
namespace {

bool validProgramSourceKind(ProgramSourceKind sourceKind) {
    switch (sourceKind) {
        case ProgramSourceKind::FactoryCatalog:
        case ProgramSourceKind::UserProgram:
            return true;
    }
    return false;
}

bool sourceMatchesProgram(const ProgramDocument& program,
                          ProgramSourceKind sourceKind) {
    if (!validProgramSourceKind(sourceKind)) {
        return false;
    }
    if (sourceKind == ProgramSourceKind::FactoryCatalog) {
        return program.program.builtIn && program.program.factoryCatalogEntry;
    }
    return !program.program.builtIn && !program.program.factoryCatalogEntry;
}

bool validChangeSource(RunChangeSource source) {
    switch (source) {
        case RunChangeSource::LocalDisplay:
        case RunChangeSource::WebInterface:
        case RunChangeSource::Recovery:
            return true;
    }
    return false;
}

bool validChangeReason(RunChangeReason reason) {
    switch (reason) {
        case RunChangeReason::UserAdjustment:
        case RunChangeReason::RecoveryCorrection:
            return true;
    }
    return false;
}

bool validAdjustmentEffect(RunAdjustmentEffect effect) {
    switch (effect) {
        case RunAdjustmentEffect::None:
        case RunAdjustmentEffect::RestartTargetQualification:
        case RunAdjustmentEffect::ContinueFermentationWithoutRequalification:
            return true;
    }
    return false;
}

bool validRunAdjustmentPhaseContext(RunAdjustmentPhaseContext phase) {
    switch (phase) {
        case RunAdjustmentPhaseContext::BeforeFermentation:
        case RunAdjustmentPhaseContext::Fermenting:
            return true;
    }
    return false;
}

RunAdjustmentEffect adjustmentEffectFor(bool targetChanged,
                                        RunAdjustmentPhaseContext phase) {
    if (!targetChanged) {
        return RunAdjustmentEffect::None;
    }
    return phase == RunAdjustmentPhaseContext::Fermenting
               ? RunAdjustmentEffect::ContinueFermentationWithoutRequalification
               : RunAdjustmentEffect::RestartTargetQualification;
}

bool equalValues(const EffectiveRunValues& left,
                 const EffectiveRunValues& right) {
    return left.targetTemperatureCelsius == right.targetTemperatureCelsius &&
           left.remainingDurationMinutes == right.remainingDurationMinutes;
}

bool monotonicTimestampWentBackwards(const RunTimestamp& current,
                                     std::uint32_t currentMonotonicEpoch,
                                     const RunTimestamp& previous,
                                     std::uint32_t previousMonotonicEpoch) {
    return currentMonotonicEpoch < previousMonotonicEpoch ||
           (currentMonotonicEpoch == previousMonotonicEpoch &&
            current.monotonicMillis < previous.monotonicMillis);
}

bool unixTimestampWentBackwards(
    const RunTimestamp& current,
    const std::optional<std::int64_t>& latestUnixTimeSeconds) {
    return current.unixTimeSeconds.has_value() &&
           latestUnixTimeSeconds.has_value() &&
           *current.unixTimeSeconds < *latestUnixTimeSeconds;
}

std::optional<std::int64_t> latestUnixTimeSeconds(
    const std::array<RunRevision, kMaximumRunRevisions>& revisions,
    std::size_t revisionCount) {
    for (std::size_t index = revisionCount; index > 0U; --index) {
        const auto& timestamp = revisions[index - 1U].timestamp;
        if (timestamp.unixTimeSeconds.has_value()) {
            return timestamp.unixTimeSeconds;
        }
    }
    return std::nullopt;
}

bool validMonotonicEpoch(
    const std::array<RunRevision, kMaximumRunRevisions>& revisions,
    std::size_t index) {
    const auto currentEpoch = revisions[index].monotonicEpoch;
    if (index == 0U) {
        return currentEpoch == 0U;
    }
    const auto previousEpoch = revisions[index - 1U].monotonicEpoch;
    return currentEpoch == previousEpoch ||
           (currentEpoch > previousEpoch && currentEpoch - previousEpoch == 1U);
}

bool validTargetTemperature(const RunProgramSnapshot& snapshot,
                            std::size_t stageIndex, double target) {
    if (stageIndex >=
        snapshot.sourceProgram.program.fermentationStages.size()) {
        return false;
    }
    ProgramDocument candidate = snapshot.sourceProgram;
    candidate.program.fermentationStages[stageIndex].targetTemperatureCelsius =
        target;
    return validateProgram(candidate, ValidationPurpose::Runnable).valid();
}

bool validRemainingDuration(const RunProgramSnapshot& snapshot,
                            std::size_t stageIndex,
                            std::uint32_t remainingDurationMinutes) {
    if (stageIndex >=
        snapshot.sourceProgram.program.fermentationStages.size()) {
        return false;
    }
    if (remainingDurationMinutes == 0U) {
        return true;
    }
    ProgramDocument candidate = snapshot.sourceProgram;
    candidate.program.fermentationStages[stageIndex].durationMinutes =
        remainingDurationMinutes;
    return validateProgram(candidate, ValidationPurpose::Runnable).valid();
}

// Verlangt eine eindeutige, phasenkorrekte Entsprechung zwischen
// `targetTemperatureChanged` und `effect`: kein Ziel geaendert -> `None`;
// Ziel geaendert -> genau einer der beiden Requalifikationseffekte. Ein
// unbekannter Enumwert wird ueber `validAdjustmentEffect()` abgelehnt.
bool validAdjustmentEffectForChange(RunAdjustmentEffect effect,
                                    bool targetTemperatureChanged) {
    if (!validAdjustmentEffect(effect)) {
        return false;
    }
    if (!targetTemperatureChanged) {
        return effect == RunAdjustmentEffect::None;
    }
    return effect == RunAdjustmentEffect::RestartTargetQualification ||
           effect ==
               RunAdjustmentEffect::ContinueFermentationWithoutRequalification;
}

bool validRevisionMetadata(const RunRevision& revision) {
    return validChangeSource(revision.source) &&
           validChangeReason(revision.reason) &&
           revision.stageIndex >= revision.completedStageCount &&
           (revision.targetTemperatureChanged ||
            revision.remainingDurationChanged) &&
           revision.targetTemperatureChanged ==
               (revision.before.targetTemperatureCelsius !=
                revision.after.targetTemperatureCelsius) &&
           revision.remainingDurationChanged ==
               (revision.before.remainingDurationMinutes !=
                revision.after.remainingDurationMinutes) &&
           validAdjustmentEffectForChange(revision.effect,
                                          revision.targetTemperatureChanged);
}

bool makeInitialEffectiveRunValues(const ProgramDocument& sourceProgram,
                                   ProgramSourceKind sourceKind,
                                   std::uint32_t sourceProgramRevision,
                                   EffectiveRunValues& destination) {
    if (sourceProgramRevision == 0U ||
        !sourceMatchesProgram(sourceProgram, sourceKind) ||
        !validateProgram(sourceProgram, ValidationPurpose::Runnable).valid()) {
        return false;
    }
    const auto& initialStage = sourceProgram.program.fermentationStages.front();
    if (!initialStage.targetTemperatureCelsius.has_value() ||
        !initialStage.durationMinutes.has_value()) {
        return false;
    }
    destination = {*initialStage.targetTemperatureCelsius,
                   *initialStage.durationMinutes};
    return true;
}

}  // namespace

ActiveRun::ActiveRun(RunProgramSnapshot snapshot,
                     EffectiveRunValues initialValues)
    : snapshot_(std::move(snapshot)), effectiveValues_(initialValues) {}

ActiveRun::ActiveRun(RestoreConstructionTag, RunProgramSnapshot snapshot,
                     EffectiveRunValues initialValues)
    : ActiveRun(std::move(snapshot), initialValues) {}

std::optional<ActiveRun> ActiveRun::start(const ProgramDocument& sourceProgram,
                                          ProgramSourceKind sourceKind,
                                          std::uint32_t sourceProgramRevision) {
    EffectiveRunValues initialValues;
    if (!makeInitialEffectiveRunValues(sourceProgram, sourceKind,
                                       sourceProgramRevision, initialValues)) {
        return std::nullopt;
    }
    return ActiveRun{{sourceProgram, sourceKind, sourceProgramRevision},
                     initialValues};
}

bool validateRunProgramSnapshotInto(
    const RunProgramSnapshot& snapshot,
    const std::array<RunRevision, kMaximumRunRevisions>& revisions,
    std::size_t revisionCount, EffectiveRunValues& effectiveValues) {
    EffectiveRunValues current;
    if (!makeInitialEffectiveRunValues(
            snapshot.sourceProgram, snapshot.sourceKind,
            snapshot.sourceProgramRevision, current) ||
        revisionCount > kMaximumRunRevisions) {
        return false;
    }

    std::optional<std::int64_t> latestUnixTimestamp;
    for (std::size_t index = 0U; index < revisionCount; ++index) {
        const auto& revision = revisions[index];
        if (revision.sequence != index + 1U ||
            !validMonotonicEpoch(revisions, index) ||
            revision.stageIndex >=
                snapshot.sourceProgram.program.fermentationStages.size() ||
            !equalValues(revision.before, current) ||
            !validRevisionMetadata(revision) ||
            !validTargetTemperature(snapshot, revision.stageIndex,
                                    revision.after.targetTemperatureCelsius) ||
            !validRemainingDuration(snapshot, revision.stageIndex,
                                    revision.after.remainingDurationMinutes) ||
            (index > 0U && monotonicTimestampWentBackwards(
                               revision.timestamp, revision.monotonicEpoch,
                               revisions[index - 1U].timestamp,
                               revisions[index - 1U].monotonicEpoch)) ||
            unixTimestampWentBackwards(revision.timestamp,
                                       latestUnixTimestamp)) {
            return false;
        }

        current = revision.after;
        if (revision.timestamp.unixTimeSeconds.has_value()) {
            latestUnixTimestamp = revision.timestamp.unixTimeSeconds;
        }
    }
    effectiveValues = current;
    return true;
}

std::optional<ActiveRun> ActiveRun::restore(
    const RunProgramSnapshot& snapshot,
    const std::array<RunRevision, kMaximumRunRevisions>& revisions,
    std::size_t revisionCount) {
    std::optional<ActiveRun> restored;
    if (!restoreInto(snapshot, revisions, revisionCount, restored)) {
        return std::nullopt;
    }
    return restored;
}

bool ActiveRun::restoreInto(
    const RunProgramSnapshot& snapshot,
    const std::array<RunRevision, kMaximumRunRevisions>& revisions,
    std::size_t revisionCount, std::optional<ActiveRun>& destination) {
    EffectiveRunValues effectiveValues;
    if (!validateRunProgramSnapshotInto(snapshot, revisions, revisionCount,
                                        effectiveValues)) {
        return false;
    }
    destination.emplace(RestoreConstructionTag{}, snapshot, effectiveValues);
    for (std::size_t index = 0U; index < revisionCount; ++index) {
        destination->revisions_[index] = revisions[index];
    }
    destination->revisionCount_ = revisionCount;
    destination->monotonicEpoch_ =
        revisionCount > 0U ? revisions[revisionCount - 1U].monotonicEpoch + 1U
                           : 0U;
    return true;
}

RunAdjustmentDecision ActiveRun::decideAdjustment(
    const RunAdjustmentRequest& request,
    const RunAdjustmentContext& context) const {
    const auto rejected = [this](RunAdjustmentStatus status) {
        return RunAdjustmentDecision{status, revisionCount_, effectiveValues_,
                                     std::nullopt};
    };
    if (!request.confirmed) {
        return rejected(RunAdjustmentStatus::NotConfirmed);
    }
    if (!context.runActive) {
        return rejected(RunAdjustmentStatus::RunInactive);
    }
    if (!context.safetyAllowsChange) {
        return rejected(RunAdjustmentStatus::SafetyRejected);
    }
    if (context.activeStageIndex >=
        snapshot_.sourceProgram.program.fermentationStages.size()) {
        return rejected(RunAdjustmentStatus::InvalidStage);
    }
    if (context.activeStageIndex < context.completedStageCount) {
        return rejected(RunAdjustmentStatus::CompletedStage);
    }
    if (!validChangeSource(request.source) ||
        !validRunAdjustmentPhaseContext(context.phaseContext) ||
        !validChangeReason(request.reason)) {
        return rejected(RunAdjustmentStatus::InvalidMetadata);
    }
    if (revisionCount_ > 0U) {
        const auto& previousTimestamp =
            revisions_[revisionCount_ - 1U].timestamp;
        const auto previousMonotonicEpoch =
            revisions_[revisionCount_ - 1U].monotonicEpoch;
        if (monotonicTimestampWentBackwards(request.timestamp, monotonicEpoch_,
                                            previousTimestamp,
                                            previousMonotonicEpoch) ||
            unixTimestampWentBackwards(
                request.timestamp,
                latestUnixTimeSeconds(revisions_, revisionCount_))) {
            return rejected(RunAdjustmentStatus::TimestampWentBackwards);
        }
    }
    if (!request.targetTemperatureCelsius.has_value() &&
        !request.remainingDurationMinutes.has_value()) {
        return rejected(RunAdjustmentStatus::NoChange);
    }
    if (revisionCount_ >= kMaximumRunRevisions) {
        return rejected(RunAdjustmentStatus::RevisionCapacityReached);
    }

    EffectiveRunValues candidate = effectiveValues_;
    if (request.targetTemperatureCelsius.has_value()) {
        if (!validTargetTemperature(snapshot_, context.activeStageIndex,
                                    *request.targetTemperatureCelsius)) {
            return rejected(RunAdjustmentStatus::InvalidValue);
        }
        candidate.targetTemperatureCelsius = *request.targetTemperatureCelsius;
    }
    if (request.remainingDurationMinutes.has_value()) {
        if (!validRemainingDuration(snapshot_, context.activeStageIndex,
                                    *request.remainingDurationMinutes)) {
            return rejected(RunAdjustmentStatus::InvalidValue);
        }
        candidate.remainingDurationMinutes = *request.remainingDurationMinutes;
    }

    const bool targetChanged = candidate.targetTemperatureCelsius !=
                               effectiveValues_.targetTemperatureCelsius;
    const bool durationChanged = candidate.remainingDurationMinutes !=
                                 effectiveValues_.remainingDurationMinutes;
    if (!targetChanged && !durationChanged) {
        return rejected(RunAdjustmentStatus::NoChange);
    }

    const RunRevision revision{
        static_cast<std::uint32_t>(revisionCount_ + 1U),
        monotonicEpoch_,
        context.activeStageIndex,
        context.completedStageCount,
        effectiveValues_,
        candidate,
        targetChanged,
        durationChanged,
        adjustmentEffectFor(targetChanged, context.phaseContext),
        request.source,
        request.reason,
        request.timestamp,
    };

    return {RunAdjustmentStatus::Proposed, revisionCount_, effectiveValues_,
            revision};
}

RunAdjustmentResult ActiveRun::applyAdjustment(
    const RunAdjustmentDecision& decision) {
    if (!decision.proposed() || !decision.revision.has_value() ||
        decision.expectedRevisionCount != revisionCount_ ||
        !equalValues(decision.expectedValues, effectiveValues_) ||
        revisionCount_ >= kMaximumRunRevisions) {
        return {RunAdjustmentStatus::NoChange, RunAdjustmentEffect::None};
    }

    const auto& revision = decision.revision.value();
    const bool timestampInvalid =
        revisionCount_ > 0U &&
        (monotonicTimestampWentBackwards(
             revision.timestamp, revision.monotonicEpoch,
             revisions_[revisionCount_ - 1U].timestamp,
             revisions_[revisionCount_ - 1U].monotonicEpoch) ||
         unixTimestampWentBackwards(
             revision.timestamp,
             latestUnixTimeSeconds(revisions_, revisionCount_)));
    if (revision.sequence != revisionCount_ + 1U ||
        revision.monotonicEpoch != monotonicEpoch_ ||
        !equalValues(revision.before, effectiveValues_) ||
        !validRevisionMetadata(revision) ||
        !validTargetTemperature(snapshot_, revision.stageIndex,
                                revision.after.targetTemperatureCelsius) ||
        !validRemainingDuration(snapshot_, revision.stageIndex,
                                revision.after.remainingDurationMinutes) ||
        timestampInvalid) {
        return {RunAdjustmentStatus::InvalidValue, RunAdjustmentEffect::None};
    }

    revisions_[revisionCount_] = revision;
    effectiveValues_ = revision.after;
    ++revisionCount_;
    return {RunAdjustmentStatus::Applied, revision.effect};
}

const RunProgramSnapshot& ActiveRun::snapshot() const { return snapshot_; }

const EffectiveRunValues& ActiveRun::effectiveValues() const {
    return effectiveValues_;
}

const std::array<RunRevision, kMaximumRunRevisions>& ActiveRun::revisions()
    const {
    return revisions_;
}

std::size_t ActiveRun::revisionCount() const { return revisionCount_; }

const RunRevision* ActiveRun::revisionAt(std::size_t index) const {
    return index < revisionCount_ ? &revisions_[index] : nullptr;
}

}  // namespace fermentation
