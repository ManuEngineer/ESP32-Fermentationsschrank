#include <unity.h>

#include "run_commands.hpp"
#include "run_persistence_contract.hpp"
#include "sensor_selection.hpp"
#include "sensor_selection_types.hpp"

// #21, Plan Abschnitt 9.7: Architektur-/Compile-Guard fuer die vier
// Sensorselektions-/Kommandovertragsheader. Die order_*.cpp-Dateien in
// diesem Verzeichnis sind reine Kompilierbarkeitsnachweise (keine
// Produktionslogik, kein RUN_TEST) - jede bindet dieselben vier Header in
// einer anderen Reihenfolge ein und beweist damit ohne gegenseitigen
// Include-Zyklus, dass keine Datei stillschweigend von einer bestimmten
// vorherigen Include-Reihenfolge abhaengt. Die eigentliche Fachlogik ist
// bereits durch test_sensor_selection, test_run_commands und
// test_run_persistence_coordinator abgedeckt; dieser Test beweist nur
// zusaetzlich, dass alle vier Header in derselben Uebersetzungseinheit
// nutzbare, unterscheidbare Typen liefern.

namespace {

using namespace fermentation;

void test_all_four_headers_yield_distinct_usable_types() {
    SensorSelectionRuntimeState runtime;
    TEST_ASSERT_TRUE(runtime.phase == SensorSelectionPhase::NoActiveRun);
    TEST_ASSERT_TRUE(runtime.permission == SensorPeltierPermission::Blocked);

    RunCommandState state;
    TEST_ASSERT_EQUAL_UINT32(0U, state.runRevision);
    TEST_ASSERT_FALSE(state.sensorSelection.has_value());

    RunPersistenceSnapshot snapshot;
    TEST_ASSERT_EQUAL_UINT32(0U, snapshot.runRevision);

    SensorSelectionDecision decision;
    TEST_ASSERT_FALSE(decision.userAction.has_value());
}

}  // namespace

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_all_four_headers_yield_distinct_usable_types);
    return UNITY_END();
}
