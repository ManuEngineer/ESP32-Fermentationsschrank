// #21, Plan Abschnitt 9.7: isolierte Include-TU, Reihenfolge invertiert -
// die am weitesten abhaengige Datei (run_persistence_contract.hpp, die
// selbst run_commands.hpp einbindet) zuerst, danach die uebrigen drei in
// einer dritten Ordnung. Beweist, dass keine der vier Dateien stillschweigend
// von einer bestimmten vorherigen Include-Reihenfolge abhaengt.
#include "run_persistence_contract.hpp"

#include "sensor_selection_types.hpp"

#include "sensor_selection.hpp"

#include "run_commands.hpp"
