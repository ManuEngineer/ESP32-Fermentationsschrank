#pragma once

#include <cstdint>
#include <optional>
#include <string>

// Werttypenreine Schicht fuer die Regelsensorauswahl (#21). Diese Datei
// bindet weder run_commands.hpp noch run_persistence_contract.hpp ein, damit
// keine gegenseitige Include-Abhaengigkeit entsteht (Plan Issue #21,
// Abschnitt 6.4.11/7). RunSensorMode wird hierher verschoben, weil sowohl
// die Laufkommandoschicht als auch der Sensorselektionskern denselben
// vollstaendigen Typ brauchen.

namespace fermentation {

enum class RunSensorMode : std::uint8_t {
    Product,
    Air,
};

enum class SensorSelectionPhase : std::uint8_t {
    NoActiveRun,
    NormalProduct,
    NormalAir,
    ProductFailureDetected,
    UserDecisionRequired,
    AirFallbackActive,
    ReturnValidationPending,
    SafeLocked,
    RestartRevalidationPending,
};

enum class SensorPeltierPermission : std::uint8_t {
    Allowed,
    Blocked,
};

struct ReturnValidationRuntimeState {
    std::optional<std::uint64_t> enteredAtMonotonicMillis;
    std::optional<std::uint32_t> lastObservedProfileRevision;
};

[[nodiscard]] inline bool operator==(const ReturnValidationRuntimeState& lhs,
                                     const ReturnValidationRuntimeState& rhs) {
    return lhs.enteredAtMonotonicMillis == rhs.enteredAtMonotonicMillis &&
           lhs.lastObservedProfileRevision == rhs.lastObservedProfileRevision;
}

[[nodiscard]] inline bool operator!=(const ReturnValidationRuntimeState& lhs,
                                     const ReturnValidationRuntimeState& rhs) {
    return !(lhs == rhs);
}

// RAM-only Laufzeitzustand der Sensorselektion. Eigentuemer ist
// RunCommandState::sensorSelectionRuntime (run_commands.hpp). Der explizite
// NoActiveRun-Default (phase == NoActiveRun, permission == Blocked, alle
// Timer-/Evidenzfelder leer) ist der einzige Inaktivzustand.
struct SensorSelectionRuntimeState {
    SensorSelectionPhase phase{SensorSelectionPhase::NoActiveRun};
    SensorPeltierPermission permission{SensorPeltierPermission::Blocked};
    std::optional<std::uint64_t> fallbackWaitStartedAtMonotonicMillis;
    std::optional<std::uint64_t> lastAppliedMonotonicMillis;
    ReturnValidationRuntimeState returnValidation;
    // Korrekturauftrag Befund 3 (6.4.12 Re-Arm-Bedingung iii): waehrend
    // AirFallbackActive markiert dies einen zwischenzeitlich beobachteten
    // Produktausfall als neue, unabhaengige Evidenzgeneration - unabhaengig
    // von profileRevision. Nur innerhalb AirFallbackActive gueltig; jede
    // andere Phase haelt dies auf false (validRuntimeCombination).
    bool productReArmPending{false};
};

[[nodiscard]] inline bool operator==(const SensorSelectionRuntimeState& lhs,
                                     const SensorSelectionRuntimeState& rhs) {
    return lhs.phase == rhs.phase && lhs.permission == rhs.permission &&
           lhs.fallbackWaitStartedAtMonotonicMillis ==
               rhs.fallbackWaitStartedAtMonotonicMillis &&
           lhs.lastAppliedMonotonicMillis == rhs.lastAppliedMonotonicMillis &&
           lhs.returnValidation == rhs.returnValidation &&
           lhs.productReArmPending == rhs.productReArmPending;
}

[[nodiscard]] inline bool operator!=(const SensorSelectionRuntimeState& lhs,
                                     const SensorSelectionRuntimeState& rhs) {
    return !(lhs == rhs);
}

enum class SensorSelectionProvenance : std::uint8_t {
    InitialSelection,
    FallbackActive,
    ReturnedToProduct,
    LegacyUnknown,
};

enum class SensorSelectionDecisionCause : std::uint8_t {
    None,
    StartSelection,
    ProductFailureBlock,
    FallbackToAir,
    ManualUserFallback,
    AutomaticValidatedReturn,
    ManualUserReturn,
    RecoveryRevalidation,
    SafeStateEntry,
    ReturnValidationAborted,
};

// Persistierter Sensorselektionszustand (Schema 2, #21/#17/#18-Grenze,
// Plan Abschnitt 6.12.1). activeRunSensorMode bleibt die einzige kanonische
// Quelle des aktiven Modus; dieser Typ dupliziert keinen Modus.
struct PersistedSensorSelectionState {
    SensorSelectionProvenance provenance{
        SensorSelectionProvenance::InitialSelection};
    SensorSelectionDecisionCause lastDecisionCause{
        SensorSelectionDecisionCause::None};
    std::uint32_t lastDecisionRunRevision{0U};
};

[[nodiscard]] inline bool operator==(const PersistedSensorSelectionState& lhs,
                                     const PersistedSensorSelectionState& rhs) {
    return lhs.provenance == rhs.provenance &&
           lhs.lastDecisionCause == rhs.lastDecisionCause &&
           lhs.lastDecisionRunRevision == rhs.lastDecisionRunRevision;
}

[[nodiscard]] inline bool operator!=(const PersistedSensorSelectionState& lhs,
                                     const PersistedSensorSelectionState& rhs) {
    return !(lhs == rhs);
}

enum class SensorSelectionBlockReason : std::uint8_t {
    None,
    ProductSensorUnusable,
    AirSensorUnusable,
    CoolingSensorUnusable,
    SimultaneousFixedSensorFailure,
    CrossRoleEvidenceIndeterminate,
    PolicyWait,
    UserActionRequired,
    SafeStateRequired,
    RestartRevalidationRequired,
    InvalidContext,
};

// Tatsaechlicher Wechsel zwischen zwei bereits aktiven Modi waehrend eines
// laufenden Prozesses (6.4.9 (b)). beforeMode ist immer gesetzt - kein
// Startwert (siehe StartSensorSelectionNotice).
struct SensorSelectionEvent {
    RunSensorMode beforeMode;
    RunSensorMode afterMode;
    SensorSelectionDecisionCause cause;
    std::uint32_t runRevision{0U};
    std::uint64_t monotonicMillis{0U};
    std::optional<std::int64_t> utcUnixSeconds;
};

// Laufrelevante Sensorentscheidung ohne Moduswechsel (6.4.9 (a)/(c)/(d)).
struct SensorSelectionNotice {
    SensorSelectionDecisionCause cause;
    std::uint64_t monotonicMillis{0U};
    std::uint32_t runRevision{0U};
    RunSensorMode activeMode;
    SensorSelectionBlockReason blockReason{SensorSelectionBlockReason::None};
};

// Effektive Startauswahl - kein Wechsel zwischen zwei aktiven Modi, haengt
// an der ohnehin beim Start erzeugten ersten Revision (persistCommand).
struct StartSensorSelectionNotice {
    RunSensorMode requestedMode;
    RunSensorMode effectiveMode;
    std::uint32_t runRevision{0U};
};

// Manuelle Sensoraktion (6.11). Dependency-frei und deshalb hier definiert:
// SensorSelectionCommandRequest (run_commands.hpp, Kommandovertrag) bindet
// zusaetzlich CommandEnvelope ein und darf deshalb nicht hier liegen, ohne
// einen Include-Zyklus mit run_commands.hpp zu erzeugen.
enum class SensorSelectionUserAction : std::uint8_t {
    ContinueWithAir,
    ReturnToProduct,
    RecheckProduct,
};

// Schmaler Sensorselektionssichtwert - kein vollstaendiger RunCommandState
// (6.4.11).
struct SensorSelectionStateView {
    std::string activeRunId;
    SensorSelectionRuntimeState runtime;
    std::optional<RunSensorMode> activeMode;
    std::optional<PersistedSensorSelectionState> persisted;
    std::uint32_t runRevision{0U};
};

enum class SensorSelectionApplyStatus : std::uint8_t {
    AppliedPersistentCandidate,
    AppliedRamOnly,
    NoChange,
    StaleDecision,
    InvalidDecision,
    TimeWentBackwards,
    CapacityReached,
    InvalidContext,
};

// Rueckgabewert von applySensorSelectionDecision (sensor_selection.hpp) -
// schmale Mutation, niemals ein vollstaendiger RunCommandState.
// SensorSelectionStateMutation::status ist die einzige Transportquelle;
// es gibt kein zweites Persistenz-Boolean.
struct SensorSelectionStateMutation {
    SensorSelectionApplyStatus status{
        SensorSelectionApplyStatus::InvalidDecision};
    SensorSelectionRuntimeState runtime;
    std::optional<RunSensorMode> activeMode;
    std::optional<PersistedSensorSelectionState> persisted;
    std::uint32_t resultingRunRevision{0U};
    std::optional<SensorSelectionEvent> event;
    std::optional<SensorSelectionNotice> notice;
};

}  // namespace fermentation
