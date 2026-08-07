// #21, Plan Abschnitt 9.7: isolierte Include-TU in Reihenfolge
// sensor_selection_types.hpp -> sensor_selection.hpp -> run_commands.hpp ->
// run_persistence_contract.hpp. Reiner Kompilierbarkeitsnachweis - keine
// Produktionslogik, siehe test_all_four_headers_yield_distinct_usable_types
// in main.cpp fuer den ausfuehrbaren Teil dieses Guards.
#include "sensor_selection_types.hpp"

#include "sensor_selection.hpp"

#include "run_commands.hpp"

#include "run_persistence_contract.hpp"
