#pragma once

#include <cstdint>

namespace device_platform_test_support {

// Parameter des einfachen thermischen Simulationsmodells. Die Werte sind
// bewusst willkuerlich und dienen ausschliesslich der Pruefung von
// Softwareablaeufen (siehe docs/IMPLEMENTATION_PLAN.md, Abschnitt "Simulierte
// Hardware und Fehler"). Sie sind keine PI-, Prozess- oder
// Sicherheitsparameter und duerfen nicht als `TBD_COMMISSIONING`-Werte
// missverstanden werden.
struct ThermalSimulationConfig {
    double ambientCelsius;
    double heatingRateCelsiusPerSecond;
    double coolingRateCelsiusPerSecond;
    double idleDriftRateCelsiusPerSecond;
};

// Deterministisches, einfaches thermisches Modell fuer native Tests. Bildet
// nach, wie eine simulierte Temperatur auf Heizen, Kuehlen oder Stillstand
// reagiert, angetrieben durch explizit vorgeschaltete virtuelle Zeit.
class ThermalSimulationModel {
   public:
    ThermalSimulationModel(ThermalSimulationConfig config,
                           double initialCelsius);

    // Bei gleichzeitigem Heizen und Kuehlen aendert sich die Temperatur nicht:
    // Diese Kombination ist ausserhalb des Modells (Aktorplaner) ausgeschlossen
    // und wird hier nicht erfunden.
    void advance(uint64_t elapsedMs, bool heating, bool cooling);

    [[nodiscard]] double celsius() const;

   private:
    ThermalSimulationConfig config_;
    double celsius_;
};

}  // namespace device_platform_test_support
