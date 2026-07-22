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

bool equalValues(const EffectiveRunValues& left,
                 const EffectiveRunValues& right) {
    return left.targetTemperatureCelsius == right.targetTemperatureCelsius &&
           left.remainingDurationMinutes == right.remainingDurationMinutes;
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
           revision.requiresTargetRequalification ==
               revision.targetTemperatureChanged;
}

}  // namespace

ActiveRun::ActiveRun(RunProgramSnapshot snapshot,
                     EffectiveRunValues initialValues)
    : snapshot_(std::move(snapshot)), effectiveValues_(initialValues) {}

std::optional<ActiveRun> ActiveRun::start(const ProgramDocument& sourceProgram,
                                          ProgramSourceKind sourceKind,
                                          std::uint32_t sourceProgramRevision) {
    if (sourceProgramRevision == 0U ||
        !sourceMatchesProgram(sourceProgram, sourceKind) ||
        !validateProgram(sourceProgram, ValidationPurpose::Runnable).valid()) {
        return std::nullopt;
    }
    const auto& initialStage = sourceProgram.program.fermentationStages.front();
    if (!initialStage.targetTemperatureCelsius.has_value() ||
        !initialStage.durationMinutes.has_value()) {
        return std::nullopt;
    }
    return ActiveRun{{sourceProgram, sourceKind, sourceProgramRevision},
                     {*initialStage.targetTemperatureCelsius,
                      *initialStage.durationMinutes}};
}

std::optional<ActiveRun> ActiveRun::restore(
    const RunProgramSnapshot& snapshot,
    const std::array<RunRevision, kMaximumRunRevisions>& revisions,
    std::size_t revisionCount) {
    auto restored = start(snapshot.sourceProgram, snapshot.sourceKind,
                          snapshot.sourceProgramRevision);
    if (!restored.has_value() || revisionCount > kMaximumRunRevisions) {
        return std::nullopt;
    }

    for (std::size_t index = 0U; index < revisionCount; ++index) {
        const auto& revision = revisions[index];
        if (revision.sequence != index + 1U ||
            revision.stageIndex >=
                snapshot.sourceProgram.program.fermentationStages.size() ||
            !equalValues(revision.before, restored->effectiveValues_) ||
            !validRevisionMetadata(revision) ||
            !validTargetTemperature(snapshot, revision.stageIndex,
                                    revision.after.targetTemperatureCelsius) ||
            !validRemainingDuration(snapshot, revision.stageIndex,
                                    revision.after.remainingDurationMinutes) ||
            (index > 0U &&
             revision.timestamp.monotonicMillis <
                 revisions[index - 1U].timestamp.monotonicMillis)) {
            return std::nullopt;
        }

        restored->revisions_[index] = revision;
        restored->effectiveValues_ = revision.after;
        ++restored->revisionCount_;
    }
    return restored;
}

RunAdjustmentResult ActiveRun::applyAdjustment(
    const RunAdjustmentRequest& request, const RunAdjustmentContext& context) {
    if (!request.confirmed) {
        return {RunAdjustmentStatus::NotConfirmed, false};
    }
    if (!context.runActive) {
        return {RunAdjustmentStatus::RunInactive, false};
    }
    if (!context.safetyAllowsChange) {
        return {RunAdjustmentStatus::SafetyRejected, false};
    }
    if (context.activeStageIndex >=
        snapshot_.sourceProgram.program.fermentationStages.size()) {
        return {RunAdjustmentStatus::InvalidStage, false};
    }
    if (context.activeStageIndex < context.completedStageCount) {
        return {RunAdjustmentStatus::CompletedStage, false};
    }
    if (!validChangeSource(request.source) ||
        !validChangeReason(request.reason)) {
        return {RunAdjustmentStatus::InvalidMetadata, false};
    }
    if (revisionCount_ > 0U &&
        request.timestamp.monotonicMillis <
            revisions_[revisionCount_ - 1U].timestamp.monotonicMillis) {
        return {RunAdjustmentStatus::TimestampWentBackwards, false};
    }
    if (!request.targetTemperatureCelsius.has_value() &&
        !request.remainingDurationMinutes.has_value()) {
        return {RunAdjustmentStatus::NoChange, false};
    }
    if (revisionCount_ >= kMaximumRunRevisions) {
        return {RunAdjustmentStatus::RevisionCapacityReached, false};
    }

    EffectiveRunValues candidate = effectiveValues_;
    if (request.targetTemperatureCelsius.has_value()) {
        if (!validTargetTemperature(snapshot_, context.activeStageIndex,
                                    *request.targetTemperatureCelsius)) {
            return {RunAdjustmentStatus::InvalidValue, false};
        }
        candidate.targetTemperatureCelsius = *request.targetTemperatureCelsius;
    }
    if (request.remainingDurationMinutes.has_value()) {
        if (!validRemainingDuration(snapshot_, context.activeStageIndex,
                                    *request.remainingDurationMinutes)) {
            return {RunAdjustmentStatus::InvalidValue, false};
        }
        candidate.remainingDurationMinutes = *request.remainingDurationMinutes;
    }

    const bool targetChanged = candidate.targetTemperatureCelsius !=
                               effectiveValues_.targetTemperatureCelsius;
    const bool durationChanged = candidate.remainingDurationMinutes !=
                                 effectiveValues_.remainingDurationMinutes;
    if (!targetChanged && !durationChanged) {
        return {RunAdjustmentStatus::NoChange, false};
    }

    const RunRevision revision{
        static_cast<std::uint32_t>(revisionCount_ + 1U),
        context.activeStageIndex,
        context.completedStageCount,
        effectiveValues_,
        candidate,
        targetChanged,
        durationChanged,
        targetChanged,
        request.source,
        request.reason,
        request.timestamp,
    };

    revisions_[revisionCount_] = revision;
    effectiveValues_ = candidate;
    ++revisionCount_;
    return {RunAdjustmentStatus::Applied, targetChanged};
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
