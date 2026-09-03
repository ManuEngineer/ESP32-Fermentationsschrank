#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

#include "program_model.hpp"
#include "storage_types.hpp"

namespace fermentation {

namespace detail {
struct RunProgramSourceRevisionTag {};
}  // namespace detail

using RunProgramSourceRevision =
    device_platform::StrongId<detail::RunProgramSourceRevisionTag,
                              std::uint64_t>;

inline constexpr std::size_t kMaximumRunRevisions = 32U;

enum class ProgramSourceKind : std::uint8_t {
    FactoryCatalog,
    UserProgram,
};

enum class RunChangeSource : std::uint8_t {
    LocalDisplay,
    WebInterface,
    Recovery,
};

enum class RunChangeReason : std::uint8_t {
    UserAdjustment,
    RecoveryCorrection,
};

struct RunTimestamp {
    std::uint64_t monotonicMillis{0U};
    std::optional<std::int64_t> unixTimeSeconds;
};

struct EffectiveRunValues {
    double targetTemperatureCelsius{0.0};
    std::uint32_t remainingDurationMinutes{0U};
};

struct RunProgramSnapshot {
    ProgramDocument sourceProgram;
    ProgramSourceKind sourceKind{ProgramSourceKind::UserProgram};
    RunProgramSourceRevision sourceProgramRevision;
};

// Phasenkontext einer Laufanpassung, absichtlich unabhaengig von
// `ProcessState` (siehe `process_state_machine.hpp`): `ActiveRun` kennt die
// Zustandsmaschine nicht. Die Kommandoschicht bildet den aktuellen
// `ProcessState` auf diesen schmalen Kontext ab.
enum class RunAdjustmentPhaseContext : std::uint8_t {
    BeforeFermentation,
    Fermenting,
};

// Fachliche Wirkung einer Zieltemperaturaenderung auf die Zielqualifikation.
// Ersetzt einen einzelnen Boolwert, damit `targetTemperatureChanged` und die
// Qualifikationswirkung nicht mehr frei widerspruechlich kombiniert werden
// koennen (siehe docs/RUN_COMMANDS.md, "Zieltemperatur vor/waehrend
// FERMENTING").
enum class RunAdjustmentEffect : std::uint8_t {
    None,
    RestartTargetQualification,
    ContinueFermentationWithoutRequalification,
};

struct RunRevision {
    std::uint32_t sequence{0U};
    std::uint32_t monotonicEpoch{0U};
    std::size_t stageIndex{0U};
    std::size_t completedStageCount{0U};
    EffectiveRunValues before;
    EffectiveRunValues after;
    bool targetTemperatureChanged{false};
    bool remainingDurationChanged{false};
    RunAdjustmentEffect effect{RunAdjustmentEffect::None};
    RunChangeSource source{RunChangeSource::LocalDisplay};
    RunChangeReason reason{RunChangeReason::UserAdjustment};
    RunTimestamp timestamp;
};

struct RunAdjustmentContext {
    bool runActive{true};
    bool safetyAllowsChange{true};
    std::size_t activeStageIndex{0U};
    std::size_t completedStageCount{0U};
    RunAdjustmentPhaseContext phaseContext{
        RunAdjustmentPhaseContext::BeforeFermentation};
};

struct RunAdjustmentRequest {
    std::optional<double> targetTemperatureCelsius;
    std::optional<std::uint32_t> remainingDurationMinutes;
    bool confirmed{false};
    RunChangeSource source{RunChangeSource::LocalDisplay};
    RunChangeReason reason{RunChangeReason::UserAdjustment};
    RunTimestamp timestamp;
};

enum class RunAdjustmentStatus : std::uint8_t {
    Proposed,
    Applied,
    NotConfirmed,
    NoChange,
    RunInactive,
    SafetyRejected,
    InvalidStage,
    CompletedStage,
    InvalidValue,
    InvalidMetadata,
    TimestampWentBackwards,
    RevisionCapacityReached,
};

struct RunAdjustmentDecision {
    RunAdjustmentStatus status{RunAdjustmentStatus::InvalidValue};
    std::size_t expectedRevisionCount{0U};
    EffectiveRunValues expectedValues;
    std::optional<RunRevision> revision;

    [[nodiscard]] bool proposed() const {
        return status == RunAdjustmentStatus::Proposed && revision.has_value();
    }
};

struct RunAdjustmentResult {
    RunAdjustmentStatus status{RunAdjustmentStatus::InvalidValue};
    RunAdjustmentEffect effect{RunAdjustmentEffect::None};

    [[nodiscard]] bool applied() const {
        return status == RunAdjustmentStatus::Applied;
    }
};

class ActiveRun {
   public:
    struct RestoreConstructionTag {
       private:
        constexpr RestoreConstructionTag() = default;
        friend class ActiveRun;
    };

    // Der Tag ist absichtlich nur innerhalb von ActiveRun erzeugbar. Er
    // erlaubt std::optional eine direkte Konstruktion ohne einen
    // stack-schweren ActiveRun-Temporary.
    ActiveRun(RestoreConstructionTag restoreTag, RunProgramSnapshot snapshot,
              EffectiveRunValues initialValues);

    [[nodiscard]] static std::optional<ActiveRun> start(
        const ProgramDocument& sourceProgram, ProgramSourceKind sourceKind,
        RunProgramSourceRevision sourceProgramRevision);

    [[nodiscard]] static std::optional<ActiveRun> restore(
        const RunProgramSnapshot& snapshot,
        const std::array<RunRevision, kMaximumRunRevisions>& revisions,
        std::size_t revisionCount);

    // Validiert die persistierte ProgramRun-Projektion und liefert die daraus
    // resultierenden effektiven Laufwerte, ohne einen ActiveRun zu
    // rekonstruieren. Der Produkt-Restore verwendet dies als gemeinsamen,
    // stack-leichten Validierungskern.
    [[nodiscard]] static bool restoreInto(
        const RunProgramSnapshot& snapshot,
        const std::array<RunRevision, kMaximumRunRevisions>& revisions,
        std::size_t revisionCount, std::optional<ActiveRun>& destination);

    [[nodiscard]] RunAdjustmentDecision decideAdjustment(
        const RunAdjustmentRequest& request,
        const RunAdjustmentContext& context) const;
    [[nodiscard]] RunAdjustmentResult applyAdjustment(
        const RunAdjustmentDecision& decision);

    [[nodiscard]] const RunProgramSnapshot& snapshot() const;
    [[nodiscard]] const EffectiveRunValues& effectiveValues() const;
    [[nodiscard]] const std::array<RunRevision, kMaximumRunRevisions>&
    revisions() const;
    [[nodiscard]] std::size_t revisionCount() const;
    [[nodiscard]] const RunRevision* revisionAt(std::size_t index) const;

   private:
    ActiveRun(RunProgramSnapshot snapshot, EffectiveRunValues initialValues);

    RunProgramSnapshot snapshot_;
    EffectiveRunValues effectiveValues_;
    std::array<RunRevision, kMaximumRunRevisions> revisions_{};
    std::size_t revisionCount_{0U};
    std::uint32_t monotonicEpoch_{0U};
};

// Gemeinsamer Validierungs-/Projektionskern fuer Persistenzvertrag und
// ActiveRun::restoreInto(). `effectiveValues` wird nur bei Erfolg beschrieben.
[[nodiscard]] bool validateRunProgramSnapshotInto(
    const RunProgramSnapshot& snapshot,
    const std::array<RunRevision, kMaximumRunRevisions>& revisions,
    std::size_t revisionCount, EffectiveRunValues& effectiveValues);

}  // namespace fermentation
