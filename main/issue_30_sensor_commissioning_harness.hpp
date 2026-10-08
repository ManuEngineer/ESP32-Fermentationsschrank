#pragma once

#if defined(APP_ISSUE_30_SENSOR_COMMISSIONING)

#include <array>
#include <cstddef>

#include "ds18b20_sampler_task.hpp"
#include "fermentation_application.hpp"

namespace fermentation::issue_30_commissioning {

// Bring-up-only Schreibpfad des Sensor-Inbetriebnahmedatensatzes (Issue #30,
// Plan 5.4, O2). Liest Zeilenkommandos von der Konsole und ruft den
// Application-Owner `applySensorCommissioning`; das Release-Profil enthaelt
// weder diese Datei noch einen Aufrufer. Kein Aktorpfad.
class Harness {
   public:
    Harness(FermentationApplication& application,
            const device_platform_esp_idf::Ds18b20Sampler& sampler) noexcept
        : application_(application), sampler_(sampler) {}

    void start() noexcept;
    void update() noexcept;

   private:
    void processLine() noexcept;
    void report() noexcept;

    FermentationApplication& application_;
    const device_platform_esp_idf::Ds18b20Sampler& sampler_;
    std::array<char, 160U> line_{};
    std::size_t lineLength_{0U};
    bool lineOverflow_{false};
};

}  // namespace fermentation::issue_30_commissioning

#endif
