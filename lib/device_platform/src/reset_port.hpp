#pragma once

#include <cstdint>

namespace device_platform {

// Anwendungsneutraler, geschlossener Resetgrund. Konkrete ESP-IDF-Werte
// werden erst im ESP-IDF-Adapter klassifiziert; unbekannte Werte bleiben
// immer Unknown.
enum class ResetCause : std::uint8_t {
    PowerOn,
    AuthorizedRestart,
    ControlledSafetyRestart,
    WatchdogOrPanic,
    Brownout,
    Unknown,
};

struct ResetCauseSnapshot {
    ResetCause cause{ResetCause::Unknown};
    bool valid{false};
    // Innerhalb eines Boots unveraenderliche Beobachtungsidentitaet. 0 ist
    // kein gueltiger Nachweis.
    std::uint64_t observationId{0U};
};

enum class ControlledRestartPurpose : std::uint8_t {
    ControlledSafetyRestart,
    AuthorizedFaultReset,
};

struct ControlledRestartRequest {
    ControlledRestartPurpose purpose{
        ControlledRestartPurpose::ControlledSafetyRestart};
};

enum class ControlledRestartResult : std::uint8_t {
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
    [[nodiscard]] virtual ControlledRestartResult requestRestart(
        const ControlledRestartRequest& request) = 0;
};

}  // namespace device_platform
