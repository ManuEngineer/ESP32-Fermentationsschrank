#pragma once

#include <cstdint>
#include <optional>

// Mehrstufiger lokaler Bedienablauf des vollstaendigen Werksresets (Issue #19,
// Plan Abschnitt 4). Reiner Zustandsautomat ohne Speicher-, Netzwerk- oder
// Aktorzugriff: er sammelt nur die bewusst gesetzten Bestaetigungsschritte und
// meldet, wann der Resetkern aufgerufen werden darf. Vorbedingungen (Lauf,
// PIN, Ursprung) und der Aufruf des Resetkerns liegen beim
// `FermentationApplication`-Einstieg unter dem ApplicationCallSerializer.
namespace fermentation {

// A: normaler, PIN-geschuetzter Vollreset; B: PIN-unabhaengiger lokaler
// Vollreset bei vergessener Service-PIN. Beide fuehren in denselben Ablauf und
// denselben Resetkern; B ist ausschliesslich ein vollstaendiger Werksreset und
// setzt die PIN nie isoliert zurueck.
enum class FactoryResetKind : std::uint8_t {
    PinProtected,
    PinIndependent,
};

enum class FactoryResetStage : std::uint8_t {
    Idle,
    // Nur Ablauf A: wartet auf die extern verifizierte PIN.
    PinRequired,
    // Ausdrueckliche Datenverlustwarnung.
    Warning,
    // Bewusste Bestaetigung.
    Confirm,
    // Langes Gedrueckthalten.
    Hold,
    // Der Resetkern wird gerade aufgerufen (kurzlebig).
    Executing,
    // Ausgang liegt vor, bis der Nutzer ihn quittiert.
    Finished,
};

enum class FactoryResetOutcome : std::uint8_t {
    None,
    // Reset abgeschlossen und Netzwerk/HTTP geordnet beendet.
    Completed,
    // Reset ohne geladene Konfigurations-Runtime (`ResetEligibleNoRuntime`,
    // Issue #19 S1) abgeschlossen: die Boot-Subsysteme (u. a. Netzwerk) wurden
    // nie komponiert, der Betrieb bleibt bis zum Neustart gesperrt.
    CompletedRestartRequired,
    // Reset abgeschlossen, aber Netzwerk/HTTP liess sich nicht sicher beenden:
    // kein bestaetigter Widerruf, Geraet vollstaendig aus- und einschalten.
    CompletedNetworkNotConfirmed,
    // Reset-Grenze ueberschritten, Laufpersistenz-Uebergabe nicht verfuegbar;
    // Netzwerk/HTTP geordnet beendet.
    HandoffUnavailable,
    HandoffUnavailableNetworkNotConfirmed,
    // Die Konfiguration ist nicht verfuegbar (z. B. keine geladene Runtime).
    Unavailable,
    // Die Vorbedingung (Lauf/Ursprung) war im Moment der Ausfuehrung nicht
    // mehr erfuellt; der Resetkern wurde nicht aufgerufen.
    Rejected,
    // Der Resetkern lehnte vor der irreversiblen Grenze ab.
    Failed,
};

// Ergebnis des Resetkerns in der fuer den Ablauf relevanten Gruppierung.
// `Completed` und `HandoffUnavailable` bedeuten, dass die irreversible Grenze
// ueberschritten ist (Netzwerk und HTTP muessen danach beendet werden);
// `Unavailable` und `Failed` liegen davor.
enum class FactoryResetCoreResult : std::uint8_t {
    Completed,
    HandoffUnavailable,
    Unavailable,
    Failed,
};

[[nodiscard]] constexpr bool factoryResetBoundaryCrossed(
    FactoryResetCoreResult result) noexcept {
    return result == FactoryResetCoreResult::Completed ||
           result == FactoryResetCoreResult::HandoffUnavailable;
}

// Gesamtausgang aus Kernergebnis und bestaetigtem Beenden von Netzwerk/HTTP.
// Ein nicht bestaetigtes Beenden wird nie als Erfolg gemeldet.
[[nodiscard]] constexpr FactoryResetOutcome factoryResetOutcomeFor(
    FactoryResetCoreResult result, bool networkEnded,
    bool restartRequired = false) noexcept {
    switch (result) {
        case FactoryResetCoreResult::Completed:
            if (!networkEnded) {
                return FactoryResetOutcome::CompletedNetworkNotConfirmed;
            }
            return restartRequired
                       ? FactoryResetOutcome::CompletedRestartRequired
                       : FactoryResetOutcome::Completed;
        case FactoryResetCoreResult::HandoffUnavailable:
            return networkEnded ? FactoryResetOutcome::HandoffUnavailable
                                : FactoryResetOutcome::
                                      HandoffUnavailableNetworkNotConfirmed;
        case FactoryResetCoreResult::Unavailable:
            return FactoryResetOutcome::Unavailable;
        case FactoryResetCoreResult::Failed:
            return FactoryResetOutcome::Failed;
    }
    return FactoryResetOutcome::Failed;
}

class FactoryResetFlow {
   public:
    // `holdMillis`: Dauer des langen Gedrueckthaltens. Ein noch nicht vom Owner
    // festgelegter Bedienparameter bleibt `nullopt`: der Ablauf ist dann nicht
    // verfuegbar (fail-closed); es gibt keinen Standardwert.
    explicit FactoryResetFlow(
        std::optional<std::uint32_t> holdMillis = std::nullopt) noexcept
        : holdMillis_(holdMillis.has_value() && *holdMillis > 0U
                          ? holdMillis
                          : std::nullopt) {}

    [[nodiscard]] bool configured() const noexcept {
        return holdMillis_.has_value();
    }
    [[nodiscard]] std::optional<std::uint32_t> holdMillis() const noexcept {
        return holdMillis_;
    }
    [[nodiscard]] FactoryResetStage stage() const noexcept { return stage_; }
    [[nodiscard]] FactoryResetKind kind() const noexcept { return kind_; }
    [[nodiscard]] FactoryResetOutcome outcome() const noexcept {
        return outcome_;
    }
    [[nodiscard]] bool active() const noexcept {
        return stage_ != FactoryResetStage::Idle;
    }

    // Startet einen Ablauf aus Idle (auch nach quittiertem Ausgang). Nicht
    // moeglich, solange der Ablauf nicht konfiguriert ist oder bereits laeuft.
    [[nodiscard]] bool begin(FactoryResetKind kind) noexcept;
    // Nur Ablauf A in PinRequired: die PIN wurde extern verifiziert.
    [[nodiscard]] bool pinVerified() noexcept;
    // Warning -> Confirm -> Hold. Ein Schritt kann nicht uebersprungen werden.
    [[nodiscard]] bool acknowledge() noexcept;
    // Abbruch in jeder Stufe ausser Executing: zurueck nach Idle, keine
    // Wirkung.
    void cancel() noexcept;
    // Pro Tick in Hold: `held` = Kontakt liegt auf dem Halteziel. Loslassen
    // setzt den Fortschritt zurueck. Liefert true genau dann, wenn die Dauer
    // erreicht wurde; der Ablauf wechselt dann nach Executing. Rueckwaerts
    // laufende Zeit setzt den Fortschritt zurueck.
    [[nodiscard]] bool updateHold(bool held, std::uint64_t nowMs) noexcept;
    // Executing -> Finished.
    void finish(FactoryResetOutcome outcome) noexcept;
    // Finished -> Idle (Ausgang quittiert).
    void dismiss() noexcept;
    // Bisher gehaltene Zeit in Hold (0 ausserhalb von Hold oder ohne Kontakt).
    [[nodiscard]] std::uint32_t heldMillis(std::uint64_t nowMs) const noexcept;

   private:
    std::optional<std::uint32_t> holdMillis_;
    FactoryResetStage stage_{FactoryResetStage::Idle};
    FactoryResetKind kind_{FactoryResetKind::PinIndependent};
    FactoryResetOutcome outcome_{FactoryResetOutcome::None};
    std::optional<std::uint64_t> holdStartedMs_;
};

// Secret-free, renderer-independent view of the local factory reset flow. It
// is part of the UI snapshot; the hold progress is quantized to tenths so a
// running long press does not republish the snapshot on every tick.
struct FermentationFactoryResetView {
    FactoryResetStage stage{FactoryResetStage::Idle};
    FactoryResetKind kind{FactoryResetKind::PinIndependent};
    FactoryResetOutcome outcome{FactoryResetOutcome::None};
    // A PIN-independent reset may be begun now.
    bool available{false};
    // The configuration has no runtime but the recovery core admits the reset
    // (`ResetEligibleNoRuntime`, Issue #19 S1): local entry on the restricted
    // home page.
    bool recoveryEntry{false};
    // 0 = hold duration not yet configured by the owner.
    std::uint32_t holdRequiredMillis{0U};
    // 0..10, only meaningful in the hold stage.
    std::uint8_t holdProgressTenths{0U};

    friend bool operator==(const FermentationFactoryResetView& left,
                           const FermentationFactoryResetView& right) {
        return left.stage == right.stage && left.kind == right.kind &&
               left.outcome == right.outcome &&
               left.available == right.available &&
               left.recoveryEntry == right.recoveryEntry &&
               left.holdRequiredMillis == right.holdRequiredMillis &&
               left.holdProgressTenths == right.holdProgressTenths;
    }
    friend bool operator!=(const FermentationFactoryResetView& left,
                           const FermentationFactoryResetView& right) {
        return !(left == right);
    }
};

}  // namespace fermentation
