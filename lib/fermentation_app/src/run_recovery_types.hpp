#pragma once

#include <cstdint>
#include <optional>

#include "process_state_machine.hpp"
#include "sensor_quality.hpp"
#include "sensor_selection_types.hpp"

// Schema-3-Wertetypen (#18), geteilt zwischen RunCommandState
// (run_commands.hpp, RAM-Zustand) und RunPersistenceSnapshot
// (run_persistence_contract.hpp, Wireformat-Projektion). Analog zu
// sensor_selection_types.hpp: eine schmale, abhaengigkeitsarme Werteschicht,
// damit run_commands.hpp nicht auf run_persistence_contract.hpp
// zurueckverweisen muss - dieses inkludiert bereits run_commands.hpp, ein
// Zurueckverweis waere zirkulaer. `RunCheckpointTrigger` lebt hier statt in
// run_persistence_contract.hpp aus demselben Grund (PendingRecoveryAnchor
// braucht ihn); jeder bisherige Einbinder von run_persistence_contract.hpp
// sieht ihn weiterhin transitiv.
namespace fermentation {

enum class RunCheckpointTrigger : std::uint8_t {
    Command = 1U,
    Transition = 2U,
    Periodic = 3U,
    SensorSelection = 4U,
};

// Plan docs/tasks/issue-18-restart-weighted-progress-plan.md, 5.12:
// unveraenderlicher Ursprungsanker einer Recovery-Zeitfrage ueber Hop-1-
// Commit und mehrere Reboots (frisch oder Carry-Forward, s. dort).
struct PendingRecoveryAnchor {
    ProcessRuntimeState originalProcessState;
    std::uint64_t knownPhaseSecondsAtOriginalCheckpoint{0U};
    std::optional<std::int64_t> originalCheckpointUtc;
    RunCheckpointTrigger originalCheckpointTrigger{
        RunCheckpointTrigger::Command};
    std::uint32_t originalCheckpointIntervalMinutes{0U};
    PriorBootPhaseElapsed accumulatedBeforeEpisode{};
    std::uint64_t knownSecondsSinceOriginalCheckpoint{0U};
};

// 5.23: bereits bekannter Vor-Boot-Anteil, getaggt mit der Phase, fuer die er
// gilt (WaitingForProduct/Fermenting/CoolHolding, oder - Hop-1-only-Ausnahme,
// 5.14 Punkt 4 - die urspruengliche Phase eines noch offenen Ankers).
struct TaggedPriorBootPhaseElapsed {
    ProcessState taggedState{ProcessState::Boot};
    PriorBootPhaseElapsed elapsed;
};

// 5.22: kumulative, tatsaechlich wirksame nominale Korrektur fuer Fermenting.
struct NominalRecoveryAdjustmentState {
    std::uint32_t cumulativeAppliedSeconds{0U};
    std::uint32_t lastAppliedEpisodeRevision{0U};
    std::uint32_t lastAppliedEpisodeDelta{0U};
};

// 5.20: strukturell getrennte Sensorevidenz - laufend fortgeschriebene
// "letzte gueltige" Evidenz vs. eingefrorene Vor-/Nach-Ausfall-Diagnose einer
// Recoveryepisode.
struct RoleTemperatureEvidence {
    std::optional<double> filteredCelsius;
    device_platform::SensorQuality quality{
        device_platform::SensorQuality::Stale};
};

struct CrossRoleEvidence {
    RoleTemperatureEvidence air;
    RoleTemperatureEvidence product;
    RoleTemperatureEvidence cooling;
};

struct FirstAfterRestartEvidence {
    std::optional<RoleTemperatureEvidence> air;
    std::optional<RoleTemperatureEvidence> product;
    std::optional<RoleTemperatureEvidence> cooling;
};

struct RecoveryTemperatureEvidence {
    CrossRoleEvidence lastKnown;
};

struct RecoveryEpisodeEvidence {
    CrossRoleEvidence beforeOutage;
    FirstAfterRestartEvidence firstAfterRestart;
    std::optional<std::uint32_t> weightedProgressSegmentId;
};

// 5.21: ehrliche Fortschrittsbasis (KnownTotal vs. permanent unbekannter
// Alt-Anteil nach Schema-1/2-Migration) und der optionale temperaturgewichtete
// Zustand (Modellgrenze/Beitragsberechnung: 5.25, run_progress_weighting.hpp).
enum class RunProgressBasis : std::uint8_t {
    KnownTotal,
    PartialUnknownHistory,
};

enum class WeightedProgressConfidence : std::uint8_t {
    ProductPreferred,
    AirReduced,
};

enum class WeightedProgressCoverage : std::uint8_t {
    Complete,
    PartialUnknown,
};

struct WeightedProgressBounds {
    std::uint64_t lowerBoundSeconds{0U};
    std::optional<std::uint64_t> upperBoundSeconds;
};

struct WeightedProgressProvenance {
    RunSensorMode lastSourceRole{RunSensorMode::Product};
    WeightedProgressConfidence confidence{
        WeightedProgressConfidence::ProductPreferred};
    std::uint32_t modelRevision{0U};
    std::uint32_t lastAppliedSegmentId{0U};
};

struct WeightedProgressState {
    WeightedProgressBounds cumulative;
    WeightedProgressCoverage coverage{WeightedProgressCoverage::PartialUnknown};
    std::optional<WeightedProgressProvenance> lastApplied;
};

struct RunProgressState {
    RunProgressBasis basis{RunProgressBasis::KnownTotal};
    std::uint32_t observedRunSeconds{0U};
    std::optional<WeightedProgressState> weightedProgress;
};

}  // namespace fermentation
