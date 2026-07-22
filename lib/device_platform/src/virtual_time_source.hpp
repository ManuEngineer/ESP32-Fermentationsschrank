#pragma once

#include "time_source.hpp"

namespace device_platform {

// Deterministische Zeitquelle fuer native Tests und Simulation. Die Zeit
// schreitet ausschliesslich durch expliziten Aufruf von
// `advanceMonotonicMillis()` voran; es wird nie auf reale Systemzeit oder das
// Netzwerk zugegriffen. Dadurch bleiben Tests reproduzierbar.
class VirtualTimeSource final : public ITimeSource {
   public:
    [[nodiscard]] uint64_t monotonicMillis() const override;
    [[nodiscard]] std::optional<int64_t> unixTimeSeconds() const override;

    // Laesst die monotone Zeit um `deltaMs` voranschreiten. Ein negativer
    // Fortschritt ist nicht vorgesehen; monotone Zeit darf nie zurueckfallen.
    // Bei einem Ueberlauf wird auf UINT64_MAX saettiert, damit die Monotonie
    // auch in diesem Grenzfall gewahrt bleibt.
    void advanceMonotonicMillis(uint64_t deltaMs);

    // Setzt oder loescht die simulierte absolute Zeit, z. B. um einen
    // NTP-Abgleich oder dessen Fehlen nachzubilden. Beeinflusst
    // `monotonicMillis()` nicht.
    void setUnixTimeSeconds(std::optional<int64_t> unixSeconds);

   private:
    uint64_t monotonicMillis_{0};
    std::optional<int64_t> unixTimeSeconds_;
};

}  // namespace device_platform
