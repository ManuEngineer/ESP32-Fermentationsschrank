#pragma once

#include <cstdint>

namespace device_platform {

// Anwendungsneutraler, geschlossener Resetgrund. Konkrete ESP-IDF-Werte
// werden erst im ESP-IDF-Adapter klassifiziert; unbekannte Werte bleiben
// immer Unknown.
enum class ResetCause : std::uint8_t {
    PowerOn,
    SoftwareRestart,
    WatchdogOrPanic,
    Brownout,
    ExternalOrOther,
    Unknown,
};

struct ResetCauseSnapshot {
    ResetCause cause{ResetCause::Unknown};
    bool valid{false};
    // Innerhalb eines Boots unveraenderliche Beobachtungsidentitaet. 0 ist
    // kein gueltiger Nachweis.
    std::uint64_t observationId{0U};
};

enum class RestartRequestResult : std::uint8_t {
    Accepted,
    Rejected,
    OutcomeUnknown,
};

// Der Port kennt weder Faults noch Episodenzaehler. Er stellt nur die
// bootlokal stabile Resetbeobachtung und die zentrale Restart-Anforderung
// bereit.
class IResetController {
   public:
    IResetController() = default;
    virtual ~IResetController() = default;

    IResetController(const IResetController&) = delete;
    IResetController& operator=(const IResetController&) = delete;
    IResetController(IResetController&&) = delete;
    IResetController& operator=(IResetController&&) = delete;

    [[nodiscard]] virtual ResetCauseSnapshot observeBootReset() const = 0;
    [[nodiscard]] virtual RestartRequestResult requestRestart() = 0;
};

}  // namespace device_platform
