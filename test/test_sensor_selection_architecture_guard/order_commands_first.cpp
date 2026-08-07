// #21, Plan Abschnitt 9.7: isolierte Include-TU, vierte Reihenfolge -
// run_commands.hpp zuerst. run_commands.hpp bindet sensor_selection_types.hpp
// ein, aber bewusst nicht sensor_selection.hpp (Abschnitt 7,
// Vorwaertsdeklaration von CrossRolePlausibilityContext) - dieser Test
// beweist, dass das trotzdem in jeder Reihenfolge kompiliert.
#include "run_commands.hpp"

#include "sensor_selection.hpp"

#include "run_persistence_contract.hpp"

#include "sensor_selection_types.hpp"
