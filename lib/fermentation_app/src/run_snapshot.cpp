#include "run_snapshot.hpp"

#include <cmath>
#include <utility>

#include "program_limits.hpp"

namespace fermentation {
namespace {

bool validProgramSourceKind(ProgramSourceKind sourceKind) {
    switch (sourceKind) {
        case ProgramSourceKind::FactoryCatalog:
        case ProgramSourceKind::UserProgram:
        case ProgramSourceKind::ManualTimed:
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

bool validCompletion(const ProgramCompletion& completion) {
    switch (completion.mode) {
        case CompletionMode::FinishWithoutCooling:
            return !completion.coolingTargetCelsius.has_value() &&
                   !completion.holdDurationMinutes.has_value();
        case CompletionMode::CoolThenFinish:
        case CompletionMode::CoolAndHoldUntilManualStop:
            return completion.coolingTargetCelsius.has_value() &&
                   std::isfinite(*completion.coolingTargetCelsius) &&
                   *completion.coolingTargetCelsius >=
                       program_limits::kMinimumCoolingTargetCelsius &&
                   *completion.coolingTargetCelsius <=
                       program_limits::kMaximumCoolingTargetCelsius &&
                   !completion.holdDurationMinutes.has_value();
        case CompletionMode::CoolAndHoldForDuration:
            return completion.coolingTargetCelsius.has_value() &&
                   std::isfinite(*completion.coolingTargetCelsius) &&
                   *completion.coolingTargetCelsius >=
                       program_limits::kMinimumCoolingTargetCelsius &&
                   *completion.coolingTargetCelsius <=
                       program_limits::kMaximumCoolingTargetCelsius &&
                   completion.holdDurationMinutes.has_value() &&
                   *completion.holdDurationMinutes >=
                       program_limits::kMinimumHoldDurationMinutes;
    }
    return false;
}

bool validManualTimedRunSourceImpl(const ManualTimedRunSource& source) {
    const bool validStage =
        source.stage.targetTemperatureCelsius.has_value() &&
        std::isfinite(*source.stage.targetTemperatureCelsius) &&
        *source.stage.targetTemperatureCelsius >=
            program_limits::kMinimumFermentationTemperatureCelsius &&
        *source.stage.targetTemperatureCelsius <=
            program_limits::kMaximumFermentationTemperatureCelsius &&
        source.stage.durationMinutes.has_value() &&
        *source.stage.durationMinutes >=
            program_limits::kMinimumFermentationDurationMinutes &&
        *source.stage.durationMinutes <=
            program_limits::kMaximumFermentationDurationMinutes;
    const bool validQualification =
        source.targetQualification.bandCelsius.has_value() &&
        std::isfinite(*source.targetQualification.bandCelsius) &&
        *source.targetQualification.bandCelsius >=
            program_limits::kMinimumQualificationBandCelsius &&
        *source.targetQualification.bandCelsius <=
            program_limits::kMaximumQualificationBandCelsius &&
        source.targetQualification.durationMinutes.has_value() &&
        *source.targetQualification.durationMinutes >=
            program_limits::kMinimumQualificationDurationMinutes;
    const bool validReach = source.maximumTargetReachMinutes.has_value() &&
                            *source.maximumTargetReachMinutes >=
                                program_limits::kMinimumTargetReachMinutes;
    const bool waitMatchesPreheat =
        source.preheatEnabled == source.maximumProductWaitMinutes.has_value();
    const bool validWait = !source.maximumProductWaitMinutes.has_value() ||
                           (*source.maximumProductWaitMinutes >=
                                program_limits::kMinimumProductWaitMinutes &&
                            *source.maximumProductWaitMinutes <=
                                program_limits::kMaximumProductWaitMinutes);
    const bool validHold = !source.completion.holdDurationMinutes.has_value() ||
                           *source.completion.holdDurationMinutes >=
                               program_limits::kMinimumHoldDurationMinutes;
    return validStage && validQualification && validReach &&
           waitMatchesPreheat && validWait && validHold &&
           validCompletion(source.completion);
}

std::size_t sourceStageCount(const ProgramRunSource& source) {
    if (const auto* program = std::get_if<ProgramDocument>(&source)) {
        return program->program.fermentationStages.size();
    }
    return std::holds_alternative<ManualTimedRunSource>(source) ? 1U : 0U;
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
    if (stageIndex >= sourceStageCount(snapshot.source)) return false;
    if (const auto* program = storedProgram(snapshot.source)) {
        ProgramDocument candidate = *program;
        candidate.program.fermentationStages[stageIndex]
            .targetTemperatureCelsius = target;
        return validateProgram(candidate, ValidationPurpose::Runnable).valid();
    }
    auto candidate = *manualTimedSource(snapshot.source);
    candidate.stage.targetTemperatureCelsius = target;
    return validManualTimedRunSourceImpl(candidate);
}

bool validRemainingDuration(const RunProgramSnapshot& snapshot,
                            std::size_t stageIndex,
                            std::uint32_t remainingDurationMinutes) {
    if (stageIndex >= sourceStageCount(snapshot.source)) return false;
    if (remainingDurationMinutes == 0U) return true;
    if (const auto* program = storedProgram(snapshot.source)) {
        ProgramDocument candidate = *program;
        candidate.program.fermentationStages[stageIndex].durationMinutes =
            remainingDurationMinutes;
        return validateProgram(candidate, ValidationPurpose::Runnable).valid();
    }
    auto candidate = *manualTimedSource(snapshot.source);
    candidate.stage.durationMinutes = remainingDurationMinutes;
    return validManualTimedRunSourceImpl(candidate);
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

bool makeInitialEffectiveRunValues(
    const ProgramRunSource& source,
    std::optional<RunProgramSourceRevision> sourceProgramRevision,
    EffectiveRunValues& destination) {
    const auto sourceKind = programSourceKind(source);
    if (const auto* sourceProgram = storedProgram(source)) {
        if (!sourceProgramRevision.has_value() ||
            sourceProgramRevision->value() == 0U ||
            !sourceMatchesProgram(*sourceProgram, sourceKind) ||
            !validateProgram(*sourceProgram, ValidationPurpose::Runnable)
                 .valid()) {
            return false;
        }
    } else {
        if (!manualTimedSource(source) ||
            sourceKind != ProgramSourceKind::ManualTimed ||
            sourceProgramRevision.has_value() ||
            !validManualTimedRunSourceImpl(*manualTimedSource(source))) {
            return false;
        }
    }
    const auto& initialStage =
        storedProgram(source) != nullptr
            ? storedProgram(source)->program.fermentationStages.front()
            : manualTimedSource(source)->stage;
    if (!initialStage.targetTemperatureCelsius.has_value() ||
        !initialStage.durationMinutes.has_value()) {
        return false;
    }
    destination = {*initialStage.targetTemperatureCelsius,
                   *initialStage.durationMinutes};
    return true;
}

}  // namespace

ProgramSourceKind programSourceKind(const ProgramRunSource& source) noexcept {
    return std::holds_alternative<ManualTimedRunSource>(source)
               ? ProgramSourceKind::ManualTimed
               : (std::get<ProgramDocument>(source).program.factoryCatalogEntry
                      ? ProgramSourceKind::FactoryCatalog
                      : ProgramSourceKind::UserProgram);
}

const ProgramDocument* storedProgram(const ProgramRunSource& source) noexcept {
    return std::get_if<ProgramDocument>(&source);
}

const ManualTimedRunSource* manualTimedSource(
    const ProgramRunSource& source) noexcept {
    return std::get_if<ManualTimedRunSource>(&source);
}

bool validateManualTimedRunSource(const ManualTimedRunSource& source) noexcept {
    return validManualTimedRunSourceImpl(source);
}

ActiveRun::ActiveRun(RunProgramSnapshot snapshot,
                     EffectiveRunValues initialValues)
    : snapshot_(std::move(snapshot)), effectiveValues_(initialValues) {}

ActiveRun::ActiveRun(RestoreConstructionTag restoreTag,
                     RunProgramSnapshot snapshot,
                     EffectiveRunValues initialValues)
    : ActiveRun(std::move(snapshot), initialValues) {
    static_cast<void>(restoreTag);
}

std::optional<ActiveRun> ActiveRun::start(
    const ProgramRunSource& source,
    std::optional<RunProgramSourceRevision> sourceProgramRevision) {
    EffectiveRunValues initialValues;
    if (!makeInitialEffectiveRunValues(source, sourceProgramRevision,
                                       initialValues)) {
        return std::nullopt;
    }
    return ActiveRun{{source, programSourceKind(source), sourceProgramRevision},
                     initialValues};
}

std::optional<ActiveRun> ActiveRun::start(
    const ProgramDocument& sourceProgram, ProgramSourceKind sourceKind,
    RunProgramSourceRevision sourceProgramRevision) {
    if (sourceKind == ProgramSourceKind::ManualTimed ||
        sourceKind != programSourceKind(ProgramRunSource{sourceProgram})) {
        return std::nullopt;
    }
    return start(ProgramRunSource{sourceProgram}, sourceProgramRevision);
}

bool validateRunProgramSnapshotInto(
    const RunProgramSnapshot& snapshot,
    const std::array<RunRevision, kMaximumRunRevisions>& revisions,
    std::size_t revisionCount, EffectiveRunValues& effectiveValues) {
    EffectiveRunValues current;
    if (snapshot.sourceKind != programSourceKind(snapshot.source) ||
        !makeInitialEffectiveRunValues(
            snapshot.source, snapshot.sourceProgramRevision, current) ||
        revisionCount > kMaximumRunRevisions) {
        return false;
    }

    std::optional<std::int64_t> latestUnixTimestamp;
    for (std::size_t index = 0U; index < revisionCount; ++index) {
        const auto& revision = revisions[index];
        if (revision.sequence != index + 1U ||
            !validMonotonicEpoch(revisions, index) ||
            revision.stageIndex >= sourceStageCount(snapshot.source) ||
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
    if (context.activeStageIndex >= sourceStageCount(snapshot_.source)) {
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
