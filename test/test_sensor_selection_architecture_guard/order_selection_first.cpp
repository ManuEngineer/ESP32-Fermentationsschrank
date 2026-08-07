// #21, Plan Abschnitt 9.7: isolierte Include-TU, dritte Reihenfolge -
// sensor_selection.hpp zuerst, bevor sein eigener Werttyp-Header
// (sensor_selection_types.hpp) an dieser Stelle explizit erneut genannt
// wird. #pragma once macht die doppelte Nennung ungefaehrlich; der Test
// beweist, dass sensor_selection.hpp seine eigene Abhaengigkeit tatsaechlich
// selbst mitbringt statt sie von einer externen Reihenfolge zu erwarten.
#include "sensor_selection.hpp"

#include "run_persistence_contract.hpp"

#include "run_commands.hpp"

#include "sensor_selection_types.hpp"
