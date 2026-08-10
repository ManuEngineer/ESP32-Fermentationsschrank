#pragma once

#include <cstdint>
#include <optional>

#include "process_state_machine.hpp"
#include "program_model.hpp"
#include "sensor_quality_snapshot.hpp"
#include "sensor_selection_types.hpp"

// Fachliche Entscheidung der Regelsensorauswahl (#21). Bindet nur
// sensor_selection_types.hpp und schmale bestehende Fachtypen
// (ProgramModel-Enums, ProcessState, SensorQualitySnapshot) - keinen
// vollstaendigen RunCommandState und keinen Persistenzkoordinator (Plan
// Abschnitt 6.4.11/7).
namespace fermentation {

// Abstrakte Regelrichtung (6.10). #21 definiert den Typvertrag vollstaendig;
// erst #22/#23 befuellen ihn mit echten Werten. Bis dahin bleibt
// `automatic_validated_return_to_product` mangels Evidenz praktisch inert
// (P21-M4).
enum class AbstractControlDirection : std::uint8_t {
    Unknown,
    Heating,
    Cooling,
    Idle,
};

enum class ThermalCompatibility : std::uint8_t {
    Unavailable,
    Compatible,
    Incompatible,
    Stale,
};

struct ThermalCompatibilityEvidence {
    ThermalCompatibility status{ThermalCompatibility::Unavailable};
    std::uint32_t profileRevision{0U};
    std::uint64_t evaluatedAtMonotonicMillis{0U};
};

// Rollenuebergreifender Plausibilitaetsvertrag (6.10). `thermalCompatibility`
// ist bewusst nicht optional - Unavailable ist selbst der explizite
// "noch nicht befuellt"-Wert.
struct CrossRolePlausibilityContext {
    ProcessState phase{ProcessState::Boot};
    std::uint64_t evaluationMonotonicMillis{0U};
    device_platform::SensorQualitySnapshot air;
    device_platform::SensorQualitySnapshot product;
    device_platform::SensorQualitySnapshot cooling;
    AbstractControlDirection direction{AbstractControlDirection::Unknown};
    std::optional<std::uint64_t> controlDemandAgeMs;
    ThermalCompatibilityEvidence thermalCompatibility;
};

// Unveraenderlicher Programmkontext fuer eine Bewertung (6.1). Nur die drei
// fuer die Auswahl fachlich relevanten Programmfelder - kein vollstaendiges
// ProgramDefinition, um keine zusaetzliche Kopplung zu erzeugen.
struct SensorSelectionProgramContext {
    SensorPreference sensorPreference{
        SensorPreference::ProductIfAvailableElseAir};
    ProductSensorFailurePolicy policy{
        ProductSensorFailurePolicy::FallbackToAirAfterTimeout};
    ReturnStrategy returnStrategy{ReturnStrategy::ManualReturnToProduct};
    std::optional<std::uint32_t> fallbackDelaySeconds;
};

// Vollstaendige erwartete Eingabe fuer applySensorSelectionDecision.
// `expected` traegt den Vorher-Zustand, gegen den die Funktion die
// Staleness-/Konsistenzpruefung durchfuehrt (6.4.11) - unabhaengig vom
// tatsaechlichen `current`-Parameter der Funktion, der denselben Wert oder
// einen inzwischen veraenderten liefern kann. `userAction` ist nur beim
// manuellen Kommandopfad gesetzt (6.14.3); der automatische Aufrufer laesst
// es leer.
struct SensorSelectionDecision {
    SensorSelectionStateView expected;
    SensorSelectionProgramContext program;
    CrossRolePlausibilityContext plausibility;
    std::optional<SensorSelectionUserAction> userAction;
};

// Die eine kanonische Entscheidungs-/Mutationsfunktion (6.4.11). Sowohl der
// automatische Bewertungszyklus als auch der manuelle Kommandopfad
// (decideApplySensorSelectionAction, run_commands.cpp) rufen ausschliesslich
// diese Funktion auf - keine zweite Regelimplementierung. Liefert
// ausschliesslich eine schmale Mutation, niemals einen vollstaendigen
// RunCommandState.
[[nodiscard]] SensorSelectionStateMutation applySensorSelectionDecision(
    const SensorSelectionStateView& current,
    const SensorSelectionDecision& decision, std::uint64_t nowMonotonicMillis);

// Reine, seiteneffektfreie Funktion fuer die #18-Restart-Aktivierung
// (6.12.3). Bewertet den persistierten Sensorselektionszustand zusammen mit
// Programmkontext und aktueller CrossRole-Evidenz. Sie persistiert nicht und
// mutiert keinen Coordinator. Eine blockierte Empfehlung bleibt im
// RestartRevalidationPending-Zustand; der Recovery-Aufrufer entscheidet daraus
// ueber den RecoveryReject-Pfad.
struct RestartSensorSelectionRecommendation {
    SensorSelectionRuntimeState runtime;
    RunSensorMode activeMode{RunSensorMode::Product};
};

[[nodiscard]] RestartSensorSelectionRecommendation
computeRestartSensorSelection(const PersistedSensorSelectionState& persisted,
                              RunSensorMode lastActiveMode,
                              const SensorSelectionProgramContext& program,
                              const CrossRolePlausibilityContext& plausibility);

}  // namespace fermentation
