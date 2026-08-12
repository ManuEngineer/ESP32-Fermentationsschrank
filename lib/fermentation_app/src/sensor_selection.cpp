#include "sensor_selection.hpp"

#include <limits>

namespace fermentation {
namespace {

using device_platform::SensorQuality;
using device_platform::SensorQualitySnapshot;

bool usable(const SensorQualitySnapshot& snapshot) {
    return snapshot.quality == SensorQuality::Valid;
}

// Checked-Zeitvertrag (6.4.12): eine Ueberlaufpruefung liefert false statt
// eines unbemerkten Wraparounds. std::uint32_t Sekunden * 1000 passt immer
// in std::uint64_t, die Pruefung bleibt dennoch explizit statt implizit
// angenommen (keine stille Annahme ueber Zielplattformbreiten).
[[nodiscard]] bool checkedMillisFromSeconds(std::uint32_t seconds,
                                            std::uint64_t& outMillis) {
    constexpr std::uint64_t kMillisPerSecond = 1000ULL;
    constexpr std::uint64_t kMaxSeconds =
        std::numeric_limits<std::uint64_t>::max() / kMillisPerSecond;
    if (static_cast<std::uint64_t>(seconds) > kMaxSeconds) {
        return false;
    }
    outMillis = static_cast<std::uint64_t>(seconds) * kMillisPerSecond;
    return true;
}

[[nodiscard]] bool checkedAddMillis(std::uint64_t a, std::uint64_t b,
                                    std::uint64_t& outSum) {
    if (a > std::numeric_limits<std::uint64_t>::max() - b) {
        return false;
    }
    outSum = a + b;
    return true;
}

// 6.10-Invarianten fuer ThermalCompatibilityEvidence: nur die reine
// Kombinationspruefung, keine eigene Alters-/Stale-Bewertung (Regressionsziel
// aus 9.2). evaluatedAtMonotonicMillis wird gegen die Auswertungszeit des
// CrossRolePlausibilityContext geprueft, NICHT gegen nowMonotonicMillis.
// Bewusst KEIN globales Gate vor dem Phasen-Dispatch: eine strukturell
// ungueltige externe #22/#23-Evidenz (z. B. Compatible mit profileRevision
// == 0) darf ausschliesslich den Rueckkehrpfad blockieren, nicht den
// gesamten Automaten fail-open in InvalidContext einfrieren - sonst wuerde
// eine defekte Fremdevidenz waehrend NormalProduct einen echten Produkt-
// sensorausfall verdecken (kein ProductFailureBlock mehr moeglich).
bool thermalEvidenceStructurallyValid(const CrossRolePlausibilityContext& ctx) {
    const auto& evidence = ctx.thermalCompatibility;
    if (evidence.status != ThermalCompatibility::Unavailable &&
        evidence.profileRevision == 0U) {
        return false;
    }
    if (evidence.evaluatedAtMonotonicMillis > ctx.evaluationMonotonicMillis) {
        return false;
    }
    return true;
}

// Korrekturauftrag Befund 5: Whitelist statt Blacklist.
// AbstractControlDirection ist ein std::uint8_t-Enum ohne Bereichspruefung an
// der #22/#23-Grenze; ein unbekannter Rohwert ist `!= Unknown`, muss aber
// trotzdem keine Rueckkehr freigeben. Nur die drei bekannten
// Nicht-Unknown-Werte gelten als belastbar.
bool knownControlDirection(AbstractControlDirection direction) {
    switch (direction) {
        case AbstractControlDirection::Heating:
        case AbstractControlDirection::Cooling:
        case AbstractControlDirection::Idle:
            return true;
        case AbstractControlDirection::Unknown:
            return false;
    }
    return false;
}

// 6.10: automatisch validierte Rueckkehr verlangt zusaetzlich zu 6.7s
// Kriterien direction != Unknown, controlDemandAgeMs vorhanden UND
// thermalCompatibility.status == Compatible. #20s SensorQuality::Valid
// deckt bereits "mehrere plausible Proben und die geforderte Stabilitaet"
// (6.7) ab - keine zweite #21-eigene Stabilitaetsbewertung. Eine strukturell
// ungueltige Evidenz (siehe thermalEvidenceStructurallyValid) verhindert
// hier ausschliesslich die Rueckkehr selbst, fail-closed wie Unavailable.
bool fullyPositiveReturnEvidence(const CrossRolePlausibilityContext& ctx,
                                 bool productValid, bool airValid,
                                 bool coolingValid) {
    return productValid && airValid && coolingValid &&
           knownControlDirection(ctx.direction) &&
           ctx.controlDemandAgeMs.has_value() &&
           thermalEvidenceStructurallyValid(ctx) &&
           ctx.thermalCompatibility.status == ThermalCompatibility::Compatible;
}

// Ob eine als Incompatible gemeldete Evidenz tatsaechlich als definitiver
// Abbruchgrund gilt. Strukturell ungueltige "Incompatible"-Meldungen (z. B.
// profileRevision == 0) werden NICHT als definitiv behandelt - sie bleiben
// fail-closed wie Unavailable (keine Rueckkehr, aber auch kein Abbruch mit
// ReturnValidationAborted).
bool definitivelyIncompatible(const CrossRolePlausibilityContext& ctx) {
    return thermalEvidenceStructurallyValid(ctx) &&
           ctx.thermalCompatibility.status ==
               ThermalCompatibility::Incompatible;
}

// Bewusst konservative Strukturpruefung: deckt den in 9.2 explizit
// verlangten Fall (SafeLocked mit Allowed unerreichbar) sowie die uebrigen
// aus 6.4.1/6.4.11 ableitbaren Phasen-/Feld-Kombinationsregeln ab.
bool validRuntimeCombination(const SensorSelectionRuntimeState& runtime) {
    if (runtime.phase == SensorSelectionPhase::NoActiveRun) {
        return runtime.permission == SensorPeltierPermission::Blocked &&
               !runtime.fallbackWaitStartedAtMonotonicMillis.has_value() &&
               !runtime.lastAppliedMonotonicMillis.has_value() &&
               !runtime.returnValidation.enteredAtMonotonicMillis.has_value() &&
               !runtime.returnValidation.lastObservedProfileRevision
                    .has_value() &&
               !runtime.productReArmPending;
    }
    const bool mustBeBlocked =
        runtime.phase == SensorSelectionPhase::SafeLocked ||
        runtime.phase == SensorSelectionPhase::ProductFailureDetected ||
        runtime.phase == SensorSelectionPhase::UserDecisionRequired ||
        runtime.phase == SensorSelectionPhase::RestartRevalidationPending;
    if (mustBeBlocked &&
        runtime.permission != SensorPeltierPermission::Blocked) {
        return false;
    }
    if (runtime.returnValidation.enteredAtMonotonicMillis.has_value() &&
        runtime.phase != SensorSelectionPhase::ReturnValidationPending) {
        return false;
    }
    // lastObservedProfileRevision bleibt nach einem Abbruch bewusst in
    // AirFallbackActive erhalten - sie ist die Re-Arm-Grundlage (6.4.12),
    // nicht nur ein ReturnValidationPending-lokaler Wert.
    if (runtime.returnValidation.lastObservedProfileRevision.has_value() &&
        runtime.phase != SensorSelectionPhase::ReturnValidationPending &&
        runtime.phase != SensorSelectionPhase::AirFallbackActive) {
        return false;
    }
    if (runtime.phase != SensorSelectionPhase::ProductFailureDetected &&
        runtime.fallbackWaitStartedAtMonotonicMillis.has_value()) {
        return false;
    }
    // Korrekturauftrag Befund 3: der Re-Arm-Marker ist ausschliesslich
    // innerhalb AirFallbackActive gueltig.
    if (runtime.phase != SensorSelectionPhase::AirFallbackActive &&
        runtime.productReArmPending) {
        return false;
    }
    return true;
}

// Vorschlag eines Phasen-Handlers: die vollstaendige neue Runtime/Aktiv-
// modus-Kombination (nicht nur ein Diff), plus Klassifikation fuer 6.4.9.
// Korrekturauftrag Befund 7: `persistWorthy` ist keine zweite, von Hand an
// jedem Ruf gepflegte Wahrheitsquelle mehr - applySensorSelectionDecision
// leitet sie unten kanonisch aus `cause != None` ab. Jeder persistenzwuerdige
// Uebergang setzt bereits eine Ursache (das war schon vorher so), ein reiner
// RAM-Uebergang (z. B. der 6.4.12-Sonderfall) laesst sie auf None.
struct Proposal {
    SensorSelectionRuntimeState runtime;
    std::optional<RunSensorMode> activeMode;
    SensorSelectionDecisionCause cause{SensorSelectionDecisionCause::None};
    SensorSelectionBlockReason blockReason{SensorSelectionBlockReason::None};
    bool rejected{false};
    SensorSelectionApplyStatus rejectionStatus{
        SensorSelectionApplyStatus::InvalidDecision};
};

Proposal reject(SensorSelectionApplyStatus status) {
    Proposal proposal;
    proposal.rejected = true;
    proposal.rejectionStatus = status;
    return proposal;
}

SensorSelectionBlockReason fixedSensorBlockReason(bool airValid,
                                                  bool coolingValid) {
    if (!airValid && !coolingValid) {
        return SensorSelectionBlockReason::SimultaneousFixedSensorFailure;
    }
    return !airValid ? SensorSelectionBlockReason::AirSensorUnusable
                     : SensorSelectionBlockReason::CoolingSensorUnusable;
}

// Korrekturauftrag Befund 2: der gemeinsame Eintrittsguard fuer jede
// NormalProduct-Rueckkehr (automatisch oder ueber RecheckProduct) - nicht nur
// das Produkt, auch Air und Cooling muessen gueltig sein, bevor Permission
// wieder Allowed werden darf. AirFallbackActive/NormalAir sind davon bewusst
// nicht betroffen (dort ist Allowed bei ungueltigem Produkt weiterhin
// zulaessig, 6.4.12).
bool productReturnEligible(bool productValid, bool airValid,
                           bool coolingValid) {
    return productValid && airValid && coolingValid;
}

// Korrekturauftrag Befund 6: benannter Alias fuer die Negation von
// thermalEvidenceStructurallyValid, ausschliesslich an den drei
// evidenzverbrauchenden Rueckkehrpfaden ausgewertet (AirFallbackActive
// RecheckProduct/automatischer Re-Arm, ReturnValidationPending) - niemals als
// globales Gate. thermalEvidenceStructurallyValid selbst behandelt Unavailable
// bereits korrekt als den legitimen "noch nicht befuellt"-Wert (die
// profileRevision-Pflicht gilt nur fuer Compatible/Incompatible/Stale); die
// Zukunfts-Zeitstempel-Pruefung gilt dagegen unabhaengig vom Status.
bool thermalEvidenceMalformed(const CrossRolePlausibilityContext& ctx) {
    return !thermalEvidenceStructurallyValid(ctx);
}

// Korrekturauftrag Befund 2 (DRY, Commit-6-Review): gemeinsame Ausnahme fuer
// die vorgezogene Air-/Cooling-Sicherheitsreaktion in
// evaluateProductFailureDetected/evaluateUserDecisionRequired/
// evaluateAirFallbackActive - ContinueWithAir bleibt in allen drei
// Funktionen eine reine Ablehnung statt einer durch den Vorrang
// "aufgewerteten" persistenzwuerdigen Mutation (6.4.14-Matrix).
bool isContinueWithAirAction(
    const std::optional<SensorSelectionUserAction>& action) {
    return action.has_value() &&
           *action == SensorSelectionUserAction::ContinueWithAir;
}

// 6.4.1 NormalProduct: Permission verlangt alle drei Rollen gueltig. Ein
// einzelner Air-/Cooling-Ausfall (nicht gleichzeitig) blockiert die
// Permission nur in-place (Klassifikation (a)); erst ein gleichzeitiger
// Air-UND-Cooling-Ausfall oder Policy StopToSafeState fuehrt zu SafeLocked
// (6.4.9-Eintrittsspalte, keine "gleichzeitig"-Qualifikation fuer StopTo-
// SafeState). Diese Asymmetrie zu AirFallbackActive/ReturnValidationPending
// (dort genuegt ein einzelner Ausfall) ist beabsichtigt: NormalProduct ist
// der stabile Primaerzustand, kein bereits kompensierter Ersatzbetrieb.
Proposal evaluateNormalProduct(const SensorSelectionStateView& current,
                               const SensorSelectionDecision& decision,
                               std::uint64_t now) {
    if (decision.userAction.has_value()) {
        return reject(SensorSelectionApplyStatus::InvalidDecision);
    }
    const auto& ctx = decision.plausibility;
    const bool airValid = usable(ctx.air);
    const bool productValid = usable(ctx.product);
    const bool coolingValid = usable(ctx.cooling);

    Proposal proposal;
    proposal.runtime = current.runtime;
    proposal.activeMode = current.activeMode;

    if (!airValid && !coolingValid) {
        proposal.runtime.phase = SensorSelectionPhase::SafeLocked;
        proposal.runtime.permission = SensorPeltierPermission::Blocked;
        proposal.runtime.fallbackWaitStartedAtMonotonicMillis = std::nullopt;
        proposal.cause = SensorSelectionDecisionCause::SafeStateEntry;
        proposal.blockReason =
            SensorSelectionBlockReason::SimultaneousFixedSensorFailure;
        return proposal;
    }

    if (!productValid) {
        if (decision.program.policy ==
            ProductSensorFailurePolicy::StopToSafeState) {
            proposal.runtime.phase = SensorSelectionPhase::SafeLocked;
            proposal.runtime.permission = SensorPeltierPermission::Blocked;
            proposal.cause = SensorSelectionDecisionCause::SafeStateEntry;
            proposal.blockReason =
                SensorSelectionBlockReason::ProductSensorUnusable;
            return proposal;
        }
        proposal.runtime.permission = SensorPeltierPermission::Blocked;
        proposal.cause = SensorSelectionDecisionCause::ProductFailureBlock;
        proposal.blockReason =
            SensorSelectionBlockReason::ProductSensorUnusable;
        if (decision.program.policy ==
            ProductSensorFailurePolicy::WaitForUser) {
            // 6.4.10-Korrektur: keine Wartezeit fuer WaitForUser - sofortiger
            // fluechtiger Uebergang im selben Bewertungszyklus.
            proposal.runtime.phase = SensorSelectionPhase::UserDecisionRequired;
            proposal.runtime.fallbackWaitStartedAtMonotonicMillis =
                std::nullopt;
        } else {
            proposal.runtime.phase =
                SensorSelectionPhase::ProductFailureDetected;
            proposal.runtime.fallbackWaitStartedAtMonotonicMillis = now;
        }
        return proposal;
    }

    const bool singleFixedFailure = !airValid || !coolingValid;
    if (singleFixedFailure) {
        if (current.runtime.permission == SensorPeltierPermission::Allowed) {
            proposal.runtime.permission = SensorPeltierPermission::Blocked;
            proposal.cause = SensorSelectionDecisionCause::ProductFailureBlock;
            proposal.blockReason =
                fixedSensorBlockReason(airValid, coolingValid);
        }
        return proposal;
    }

    if (current.runtime.permission == SensorPeltierPermission::Blocked) {
        proposal.runtime.permission = SensorPeltierPermission::Allowed;
        proposal.cause = SensorSelectionDecisionCause::RecoveryRevalidation;
    }
    return proposal;
}

// 6.4.1 NormalAir: verlaesst die Phase laut Tabelle "nie automatisch" - nur
// die Permission toggelt. Anders als AirFallbackActive/ReturnValidation-
// Pending (dort bereits ein kompensierter Ersatzbetrieb) ist NormalAir der
// beabsichtigte Primaerzustand fuer AirOnly-/direkte Luftlaeufe; ein
// vorbeigehender Sensorausfall braucht keine volle SafeLocked-Wiederherstel-
// lungszeremonie.
Proposal evaluateNormalAir(const SensorSelectionStateView& current,
                           const SensorSelectionDecision& decision,
                           std::uint64_t /*now*/) {
    if (decision.userAction.has_value()) {
        return reject(SensorSelectionApplyStatus::InvalidDecision);
    }
    const auto& ctx = decision.plausibility;
    const bool airValid = usable(ctx.air);
    const bool coolingValid = usable(ctx.cooling);

    Proposal proposal;
    proposal.runtime = current.runtime;
    proposal.activeMode = current.activeMode;

    const bool allValid = airValid && coolingValid;
    if (!allValid &&
        current.runtime.permission == SensorPeltierPermission::Allowed) {
        proposal.runtime.permission = SensorPeltierPermission::Blocked;
        proposal.cause = SensorSelectionDecisionCause::ProductFailureBlock;
        proposal.blockReason = fixedSensorBlockReason(airValid, coolingValid);
    } else if (allValid &&
               current.runtime.permission == SensorPeltierPermission::Blocked) {
        proposal.runtime.permission = SensorPeltierPermission::Allowed;
        proposal.cause = SensorSelectionDecisionCause::RecoveryRevalidation;
    }
    return proposal;
}

Proposal continueWithAirFromProductPhase(
    const SensorSelectionStateView& current,
    const SensorSelectionDecision& decision, bool airValid, bool coolingValid) {
    const bool allowed =
        decision.program.sensorPreference != SensorPreference::ProductRequired;
    if (!allowed || !airValid || !coolingValid) {
        return reject(SensorSelectionApplyStatus::InvalidDecision);
    }
    Proposal proposal;
    proposal.runtime = current.runtime;
    proposal.runtime.phase = SensorSelectionPhase::AirFallbackActive;
    proposal.runtime.permission = SensorPeltierPermission::Allowed;
    proposal.runtime.fallbackWaitStartedAtMonotonicMillis = std::nullopt;
    proposal.activeMode = RunSensorMode::Air;
    proposal.cause = SensorSelectionDecisionCause::ManualUserFallback;
    return proposal;
}

// 6.4.1/6.4.10 ProductFailureDetected: nur fuer FallbackToAirAfterTimeout
// beobachtbar (WaitForUser/StopToSafeState verlassen NormalProduct sofort in
// UserDecisionRequired bzw. SafeLocked). Manuelles ContinueWithAir/Recheck-
// Product ist hier zusaetzlich zur automatischen Wartezeitbewertung gueltig
// (6.4.10 vereinheitlichte ContinueWithAir-Regel).
Proposal evaluateProductFailureDetected(const SensorSelectionStateView& current,
                                        const SensorSelectionDecision& decision,
                                        std::uint64_t now) {
    const auto& ctx = decision.plausibility;
    const bool airValid = usable(ctx.air);
    const bool productValid = usable(ctx.product);
    const bool coolingValid = usable(ctx.cooling);

    Proposal proposal;
    proposal.runtime = current.runtime;
    proposal.activeMode = current.activeMode;

    // Korrekturauftrag Befund 2: Air-/Cooling-Sicherheitsreaktion vor jeder
    // Benutzeraktion und vor der automatischen Rueckkehrpruefung auswerten.
    // ProductFailureDetected bleibt bei der 6.4.1-Asymmetrie (nur der
    // gleichzeitige Ausfall verlaesst die Phase nach SafeLocked); ein
    // einzelner Ausfall blockiert die Rueckkehr nach NormalProduct weiter
    // unten ueber productReturnEligible, ohne die Phase zu verlassen.
    // ContinueWithAir bleibt von diesem Vorrang ausgenommen (6.4.14-Matrix:
    // "ContinueWithAir ohne gueltiges Air/Cooling" -> CommandStatus::
    // InvalidInput, keine Mutation) - continueWithAirFromProductPhase lehnt
    // diesen Fall selbst ab; ein SafeLocked-Uebergang wuerde die geplante
    // Ablehnung in eine persistenzwuerdige Mutation verwandeln.
    const bool isContinueWithAir = isContinueWithAirAction(decision.userAction);
    if (!isContinueWithAir && !airValid && !coolingValid) {
        proposal.runtime.phase = SensorSelectionPhase::SafeLocked;
        proposal.runtime.permission = SensorPeltierPermission::Blocked;
        proposal.runtime.fallbackWaitStartedAtMonotonicMillis = std::nullopt;
        proposal.cause = SensorSelectionDecisionCause::SafeStateEntry;
        proposal.blockReason =
            SensorSelectionBlockReason::SimultaneousFixedSensorFailure;
        return proposal;
    }

    if (decision.userAction.has_value()) {
        switch (*decision.userAction) {
            case SensorSelectionUserAction::ContinueWithAir:
                return continueWithAirFromProductPhase(current, decision,
                                                       airValid, coolingValid);
            case SensorSelectionUserAction::RecheckProduct:
                if (!productValid) {
                    return proposal;  // weiterhin ungueltig -> NoChange
                }
                // Korrekturauftrag Befund 2: keine Rueckkehr nach
                // NormalProduct, solange nicht Product, Air UND Cooling
                // gueltig sind - ein einzelner Air-/Cooling-Ausfall haelt die
                // Phase, statt blind Allowed zu vergeben.
                if (productReturnEligible(productValid, airValid,
                                          coolingValid)) {
                    proposal.runtime.phase =
                        SensorSelectionPhase::NormalProduct;
                    proposal.runtime.permission =
                        SensorPeltierPermission::Allowed;
                    proposal.runtime.fallbackWaitStartedAtMonotonicMillis =
                        std::nullopt;
                    proposal.cause =
                        SensorSelectionDecisionCause::RecoveryRevalidation;
                }
                return proposal;
            case SensorSelectionUserAction::ReturnToProduct:
                return reject(SensorSelectionApplyStatus::InvalidDecision);
        }
    }

    if (productValid) {
        if (productReturnEligible(productValid, airValid, coolingValid)) {
            proposal.runtime.phase = SensorSelectionPhase::NormalProduct;
            proposal.runtime.permission = SensorPeltierPermission::Allowed;
            proposal.runtime.fallbackWaitStartedAtMonotonicMillis =
                std::nullopt;
            proposal.cause = SensorSelectionDecisionCause::RecoveryRevalidation;
        }
        // Produkt valide, aber Air/Cooling (einzeln) noch nicht: bleibt
        // ProductFailureDetected und wartet auf die verbleibende Rolle
        // (NoChange) - kein Fallback-Timeout-Pfad, der ist ausschliesslich
        // fuer weiterhin ungueltiges Produkt gedacht (siehe unten).
        return proposal;
    }

    if (decision.program.policy !=
        ProductSensorFailurePolicy::FallbackToAirAfterTimeout) {
        // WaitForUser/StopToSafeState erreichen diesen Zweig strukturell nie
        // (siehe evaluateNormalProduct); fail-closed statt stille Annahme.
        return reject(SensorSelectionApplyStatus::InvalidContext);
    }
    if (!current.runtime.fallbackWaitStartedAtMonotonicMillis.has_value()) {
        return reject(SensorSelectionApplyStatus::InvalidContext);
    }
    std::uint64_t delayMillis = 0U;
    if (!decision.program.fallbackDelaySeconds.has_value() ||
        !checkedMillisFromSeconds(*decision.program.fallbackDelaySeconds,
                                  delayMillis)) {
        return reject(SensorSelectionApplyStatus::InvalidContext);
    }
    std::uint64_t deadline = 0U;
    if (!checkedAddMillis(*current.runtime.fallbackWaitStartedAtMonotonicMillis,
                          delayMillis, deadline)) {
        return reject(SensorSelectionApplyStatus::InvalidContext);
    }
    if (now < deadline) {
        return proposal;  // weiterhin warten -> NoChange
    }
    if (!airValid || !coolingValid) {
        proposal.runtime.phase = SensorSelectionPhase::SafeLocked;
        proposal.runtime.permission = SensorPeltierPermission::Blocked;
        proposal.runtime.fallbackWaitStartedAtMonotonicMillis = std::nullopt;
        proposal.cause = SensorSelectionDecisionCause::SafeStateEntry;
        proposal.blockReason = fixedSensorBlockReason(airValid, coolingValid);
        return proposal;
    }
    proposal.runtime.phase = SensorSelectionPhase::AirFallbackActive;
    proposal.runtime.permission = SensorPeltierPermission::Allowed;
    proposal.runtime.fallbackWaitStartedAtMonotonicMillis = std::nullopt;
    proposal.activeMode = RunSensorMode::Air;
    proposal.cause = SensorSelectionDecisionCause::FallbackToAir;
    return proposal;
}

// 6.4.1/6.4.10 UserDecisionRequired: kein Wartetimer (WaitForUser hat keinen).
// Produktrueckkehr ist hier automatisch (keine Benutzeraktion noetig,
// 6.4.10-Vereinheitlichung mit FallbackToAirAfterTimeout).
Proposal evaluateUserDecisionRequired(const SensorSelectionStateView& current,
                                      const SensorSelectionDecision& decision,
                                      std::uint64_t /*now*/) {
    const auto& ctx = decision.plausibility;
    const bool airValid = usable(ctx.air);
    const bool productValid = usable(ctx.product);
    const bool coolingValid = usable(ctx.cooling);

    Proposal proposal;
    proposal.runtime = current.runtime;
    proposal.activeMode = current.activeMode;

    // Korrekturauftrag Befund 2: Air-/Cooling-Sicherheitsreaktion vor jeder
    // Benutzeraktion auswerten. Kein "gleichzeitig"-Qualifier in 6.4.2 fuer
    // diese Zeile - ein einzelner Ausfall genuegt (anders als bei
    // ProductFailureDetected). Air/Cooling sind ab hier fuer RecheckProduct/
    // ReturnToProduct und den automatischen Pfad garantiert gueltig.
    // ContinueWithAir bleibt ausgenommen (6.4.14-Matrix: "ContinueWithAir
    // ohne gueltiges Air/Cooling" -> CommandStatus::InvalidInput, keine
    // Mutation) - continueWithAirFromProductPhase lehnt diesen Fall selbst ab.
    const bool isContinueWithAir = isContinueWithAirAction(decision.userAction);
    if (!isContinueWithAir && (!airValid || !coolingValid)) {
        proposal.runtime.phase = SensorSelectionPhase::SafeLocked;
        proposal.runtime.permission = SensorPeltierPermission::Blocked;
        proposal.cause = SensorSelectionDecisionCause::SafeStateEntry;
        proposal.blockReason = fixedSensorBlockReason(airValid, coolingValid);
        return proposal;
    }

    if (decision.userAction.has_value()) {
        switch (*decision.userAction) {
            case SensorSelectionUserAction::ContinueWithAir:
                return continueWithAirFromProductPhase(current, decision,
                                                       airValid, coolingValid);
            case SensorSelectionUserAction::RecheckProduct:
                if (productValid) {
                    proposal.runtime.phase =
                        SensorSelectionPhase::NormalProduct;
                    proposal.runtime.permission =
                        SensorPeltierPermission::Allowed;
                    proposal.cause =
                        SensorSelectionDecisionCause::RecoveryRevalidation;
                }
                return proposal;
            case SensorSelectionUserAction::ReturnToProduct:
                return reject(SensorSelectionApplyStatus::InvalidDecision);
        }
    }

    if (productValid) {
        proposal.runtime.phase = SensorSelectionPhase::NormalProduct;
        proposal.runtime.permission = SensorPeltierPermission::Allowed;
        proposal.cause = SensorSelectionDecisionCause::RecoveryRevalidation;
    }
    return proposal;
}

// 6.4.7/6.4.12 AirFallbackActive: einzelner Air-/Cooling-Ausfall genuegt fuer
// SafeLocked (bereits kompensierter Ersatzbetrieb). RecheckProduct ist der
// 6.4.12-Sonderfall (RAM-only nach ReturnValidationPending bei unvoll-
// staendiger Evidenz).
Proposal evaluateAirFallbackActive(const SensorSelectionStateView& current,
                                   const SensorSelectionDecision& decision,
                                   std::uint64_t now) {
    const auto& ctx = decision.plausibility;
    const bool airValid = usable(ctx.air);
    const bool productValid = usable(ctx.product);
    const bool coolingValid = usable(ctx.cooling);

    Proposal proposal;
    proposal.runtime = current.runtime;
    proposal.activeMode = current.activeMode;

    // Korrekturauftrag Befund 2: Air-/Cooling-Sicherheitsreaktion vor jeder
    // Benutzeraktion und vor der automatischen Bewertung auswerten - ein
    // bereits kompensierter Ersatzbetrieb verliert seine Freigabe sofort,
    // unabhaengig davon, was gerade angefragt wird. ContinueWithAir bleibt
    // ausgenommen: es ist aus AirFallbackActive strukturell immer ungueltig
    // (bereits im Luftmodus) und bleibt deshalb unabhaengig von Air/Cooling
    // eine reine Ablehnung ohne Mutation statt einer durch den Vorrang
    // "upgegradeten" SafeLocked-Persistenz.
    const bool isContinueWithAir = isContinueWithAirAction(decision.userAction);
    if (!isContinueWithAir && (!airValid || !coolingValid)) {
        proposal.runtime.phase = SensorSelectionPhase::SafeLocked;
        proposal.runtime.permission = SensorPeltierPermission::Blocked;
        proposal.runtime.returnValidation = ReturnValidationRuntimeState{};
        proposal.runtime.productReArmPending = false;
        proposal.cause = SensorSelectionDecisionCause::SafeStateEntry;
        proposal.blockReason = fixedSensorBlockReason(airValid, coolingValid);
        return proposal;
    }

    if (decision.userAction.has_value()) {
        switch (*decision.userAction) {
            case SensorSelectionUserAction::ContinueWithAir:
                return reject(SensorSelectionApplyStatus::InvalidDecision);
            case SensorSelectionUserAction::ReturnToProduct: {
                if (decision.program.returnStrategy !=
                        ReturnStrategy::ManualReturnToProduct ||
                    !productValid) {
                    return reject(SensorSelectionApplyStatus::InvalidDecision);
                }
                proposal.runtime.phase = SensorSelectionPhase::NormalProduct;
                proposal.runtime.permission = SensorPeltierPermission::Allowed;
                proposal.runtime.returnValidation =
                    ReturnValidationRuntimeState{};
                proposal.runtime.productReArmPending = false;
                proposal.activeMode = RunSensorMode::Product;
                proposal.cause = SensorSelectionDecisionCause::ManualUserReturn;
                return proposal;
            }
            case SensorSelectionUserAction::RecheckProduct: {
                if (!productValid) {
                    return proposal;  // NoChange
                }
                if (decision.program.returnStrategy !=
                    ReturnStrategy::AutomaticValidatedReturnToProduct) {
                    return proposal;  // keine automatische Strategie ->
                                      // NoChange
                }
                // Korrekturauftrag Befund 6: strukturell ungueltige Evidenz
                // blockiert nur diesen Rueckkehrversuch (InvalidContext),
                // nicht die oben bereits erfolgte Air-/Cooling-Reaktion.
                if (thermalEvidenceMalformed(ctx)) {
                    return reject(SensorSelectionApplyStatus::InvalidContext);
                }
                if (fullyPositiveReturnEvidence(ctx, productValid, airValid,
                                                coolingValid)) {
                    proposal.runtime.phase =
                        SensorSelectionPhase::NormalProduct;
                    proposal.runtime.permission =
                        SensorPeltierPermission::Allowed;
                    proposal.runtime.returnValidation =
                        ReturnValidationRuntimeState{};
                    proposal.runtime.productReArmPending = false;
                    proposal.activeMode = RunSensorMode::Product;
                    proposal.cause =
                        SensorSelectionDecisionCause::AutomaticValidatedReturn;
                    return proposal;
                }
                if (definitivelyIncompatible(ctx)) {
                    return proposal;  // bekannt inkompatibel -> keine neue
                                      // Chance
                }
                // 6.4.12-Sonderfall: unvollstaendige/Unavailable/Stale-Evidenz.
                proposal.runtime.phase =
                    SensorSelectionPhase::ReturnValidationPending;
                proposal.runtime.returnValidation.enteredAtMonotonicMillis =
                    now;
                proposal.runtime.returnValidation.lastObservedProfileRevision =
                    ctx.thermalCompatibility.profileRevision;
                return proposal;  // AppliedRamOnly
            }
        }
    }

    // Korrekturauftrag Befund 3 (6.4.12 Re-Arm-Bedingung iii): ein waehrend
    // AirFallbackActive automatisch beobachteter Produktausfall markiert eine
    // neue, unabhaengige Evidenzgeneration - unabhaengig von profileRevision.
    // Der Marker wird unten konsumiert, sobald er (oder Bedingung i) einen
    // neuen Eintritt in ReturnValidationPending ausloest.
    if (!productValid) {
        proposal.runtime.productReArmPending = true;
        return proposal;  // AppliedRamOnly ueber die changed-Erkennung.
    }

    if (decision.program.returnStrategy ==
        ReturnStrategy::AutomaticValidatedReturnToProduct) {
        // Re-Arm-Regel (6.4.12): (i) geaenderte profileRevision seit dem
        // letzten Eintritt ODER (iii) ein zwischenzeitlich waehrend
        // AirFallbackActive beobachteter Produktausfall - je nachdem, was
        // zuerst zutrifft. Ohne eine der beiden Bedingungen kein
        // automatischer Re-Arm (verhindert eine unbegrenzte Bewertungs-
        // schleife pro Zyklus).
        const bool profileChanged =
            !current.runtime.returnValidation.lastObservedProfileRevision
                 .has_value() ||
            *current.runtime.returnValidation.lastObservedProfileRevision !=
                ctx.thermalCompatibility.profileRevision;
        if (profileChanged || current.runtime.productReArmPending) {
            // Korrekturauftrag Befund 6: strukturell ungueltige Evidenz
            // blockiert nur den Re-Arm-Versuch selbst.
            if (thermalEvidenceMalformed(ctx)) {
                return reject(SensorSelectionApplyStatus::InvalidContext);
            }
            proposal.runtime.phase =
                SensorSelectionPhase::ReturnValidationPending;
            proposal.runtime.returnValidation.enteredAtMonotonicMillis = now;
            proposal.runtime.returnValidation.lastObservedProfileRevision =
                ctx.thermalCompatibility.profileRevision;
            proposal.runtime.productReArmPending = false;
        }
    }
    return proposal;
}

// 6.4.7 ReturnValidationPending: Regelung laeuft auf Air unveraendert weiter
// (Allowed, solange Air/Cooling gueltig). Einzelner Air-/Cooling-Ausfall
// gewinnt per Vorrangregel gegenueber einem gleichzeitig moeglichen Abbruch
// (c vor d).
Proposal evaluateReturnValidationPending(
    const SensorSelectionStateView& current,
    const SensorSelectionDecision& decision, std::uint64_t /*now*/) {
    if (decision.userAction.has_value()) {
        return reject(SensorSelectionApplyStatus::InvalidDecision);
    }
    const auto& ctx = decision.plausibility;
    const bool airValid = usable(ctx.air);
    const bool productValid = usable(ctx.product);
    const bool coolingValid = usable(ctx.cooling);

    Proposal proposal;
    proposal.runtime = current.runtime;
    proposal.activeMode = current.activeMode;

    if (!airValid || !coolingValid) {
        proposal.runtime.phase = SensorSelectionPhase::SafeLocked;
        proposal.runtime.permission = SensorPeltierPermission::Blocked;
        proposal.runtime.returnValidation = ReturnValidationRuntimeState{};
        proposal.runtime.productReArmPending = false;
        proposal.cause = SensorSelectionDecisionCause::SafeStateEntry;
        proposal.blockReason = fixedSensorBlockReason(airValid, coolingValid);
        return proposal;
    }

    if (!productValid || definitivelyIncompatible(ctx)) {
        proposal.runtime.phase = SensorSelectionPhase::AirFallbackActive;
        // lastObservedProfileRevision bleibt erhalten (Re-Arm-Grundlage,
        // 6.4.12); nur der Eintrittszeitstempel wird verworfen.
        proposal.runtime.returnValidation.enteredAtMonotonicMillis =
            std::nullopt;
        // Korrekturauftrag Befund 3: nur ein tatsaechlicher Produktausfall
        // markiert die Re-Arm-Bedingung (iii) - ein Abbruch allein wegen
        // Incompatible-Evidenz bei weiterhin validem Produkt tut das nicht.
        if (!productValid) {
            proposal.runtime.productReArmPending = true;
        }
        proposal.cause = SensorSelectionDecisionCause::ReturnValidationAborted;
        return proposal;
    }

    // Korrekturauftrag Befund 6: strukturell ungueltige Evidenz blockiert nur
    // die Rueckkehr selbst, nicht die oben bereits erfolgte Air-/Cooling-
    // Sicherheitsreaktion oder den oben bereits erfolgten Abbruch.
    if (thermalEvidenceMalformed(ctx)) {
        return reject(SensorSelectionApplyStatus::InvalidContext);
    }

    if (fullyPositiveReturnEvidence(ctx, productValid, airValid,
                                    coolingValid)) {
        proposal.runtime.phase = SensorSelectionPhase::NormalProduct;
        proposal.runtime.permission = SensorPeltierPermission::Allowed;
        proposal.runtime.returnValidation = ReturnValidationRuntimeState{};
        proposal.runtime.productReArmPending = false;
        proposal.activeMode = RunSensorMode::Product;
        proposal.cause = SensorSelectionDecisionCause::AutomaticValidatedReturn;
        return proposal;
    }

    // Unavailable, unvollstaendig oder Stale: bleibt ReturnValidationPending.
    return proposal;
}

Proposal evaluatePhase(const SensorSelectionStateView& current,
                       const SensorSelectionDecision& decision,
                       std::uint64_t now) {
    switch (current.runtime.phase) {
        case SensorSelectionPhase::NormalProduct:
            return evaluateNormalProduct(current, decision, now);
        case SensorSelectionPhase::NormalAir:
            return evaluateNormalAir(current, decision, now);
        case SensorSelectionPhase::ProductFailureDetected:
            return evaluateProductFailureDetected(current, decision, now);
        case SensorSelectionPhase::UserDecisionRequired:
            return evaluateUserDecisionRequired(current, decision, now);
        case SensorSelectionPhase::AirFallbackActive:
            return evaluateAirFallbackActive(current, decision, now);
        case SensorSelectionPhase::ReturnValidationPending:
            return evaluateReturnValidationPending(current, decision, now);
        case SensorSelectionPhase::SafeLocked: {
            if (decision.userAction.has_value()) {
                return reject(SensorSelectionApplyStatus::InvalidDecision);
            }
            Proposal proposal;
            proposal.runtime = current.runtime;
            proposal.activeMode = current.activeMode;
            return proposal;  // NoChange: nur ueber den Startpfad verlassbar.
        }
        case SensorSelectionPhase::NoActiveRun:
        case SensorSelectionPhase::RestartRevalidationPending:
            return reject(SensorSelectionApplyStatus::InvalidContext);
    }
    return reject(SensorSelectionApplyStatus::InvalidContext);
}

}  // namespace

SensorSelectionStateMutation applySensorSelectionDecision(
    const SensorSelectionStateView& current,
    const SensorSelectionDecision& decision, std::uint64_t nowMonotonicMillis) {
    const auto rejected = [&current](SensorSelectionApplyStatus status) {
        SensorSelectionStateMutation mutation;
        mutation.status = status;
        mutation.runtime = current.runtime;
        mutation.activeMode = current.activeMode;
        mutation.persisted = current.persisted;
        mutation.resultingRunRevision = current.runRevision;
        return mutation;
    };

    if (current.activeRunId != decision.expected.activeRunId ||
        current.runtime != decision.expected.runtime ||
        current.activeMode != decision.expected.activeMode ||
        current.persisted != decision.expected.persisted ||
        current.runRevision != decision.expected.runRevision) {
        return rejected(SensorSelectionApplyStatus::StaleDecision);
    }

    if (!validRuntimeCombination(current.runtime)) {
        return rejected(SensorSelectionApplyStatus::InvalidDecision);
    }

    if (current.runtime.lastAppliedMonotonicMillis.has_value() &&
        nowMonotonicMillis < *current.runtime.lastAppliedMonotonicMillis) {
        return rejected(SensorSelectionApplyStatus::TimeWentBackwards);
    }

    const Proposal proposal =
        evaluatePhase(current, decision, nowMonotonicMillis);
    if (proposal.rejected) {
        return rejected(proposal.rejectionStatus);
    }

    const bool changed = proposal.runtime != current.runtime ||
                         proposal.activeMode != current.activeMode;

    SensorSelectionStateMutation mutation;
    mutation.runtime = proposal.runtime;
    mutation.activeMode = proposal.activeMode;
    mutation.persisted = current.persisted;
    mutation.resultingRunRevision = current.runRevision;

    if (!changed) {
        mutation.status = SensorSelectionApplyStatus::NoChange;
        return mutation;
    }

    mutation.runtime.lastAppliedMonotonicMillis = nowMonotonicMillis;

    // Korrekturauftrag Befund 7: persistWorthy ist keine separat gepflegte
    // zweite Wahrheitsquelle mehr, sondern wird kanonisch aus der Ursache
    // abgeleitet - jeder Phasen-Handler setzt `cause` bereits exakt fuer die
    // Faelle, die eine Laufrevision/einen Store-Write verlangen (siehe die
    // einzelnen evaluate*-Funktionen).
    const bool persistWorthy =
        proposal.cause != SensorSelectionDecisionCause::None;
    if (!persistWorthy) {
        mutation.status = SensorSelectionApplyStatus::AppliedRamOnly;
        return mutation;
    }

    if (current.runRevision == std::numeric_limits<std::uint32_t>::max()) {
        return rejected(SensorSelectionApplyStatus::CapacityReached);
    }

    mutation.resultingRunRevision = current.runRevision + 1U;
    mutation.status = SensorSelectionApplyStatus::AppliedPersistentCandidate;

    PersistedSensorSelectionState persisted =
        current.persisted.value_or(PersistedSensorSelectionState{});
    switch (proposal.cause) {
        case SensorSelectionDecisionCause::FallbackToAir:
        case SensorSelectionDecisionCause::ManualUserFallback:
            persisted.provenance = SensorSelectionProvenance::FallbackActive;
            break;
        case SensorSelectionDecisionCause::AutomaticValidatedReturn:
        case SensorSelectionDecisionCause::ManualUserReturn:
            persisted.provenance = SensorSelectionProvenance::ReturnedToProduct;
            break;
        default:
            break;  // Provenienz bleibt unveraendert (6.12.1).
    }
    persisted.lastDecisionCause = proposal.cause;
    persisted.lastDecisionRunRevision = mutation.resultingRunRevision;
    mutation.persisted = persisted;

    const bool modeChanged = proposal.activeMode != current.activeMode;
    if (modeChanged) {
        // Korrekturauftrag Befund 5: keine Dereferenzierung eines leeren
        // Modus. Ein persistenzwuerdiger Moduswechsel ohne zwei tatsaechliche
        // Werte ist strukturell unmoeglich (jeder aktive Lauf traegt einen
        // Modus), aber ein Automat verlaesst sich dafuer nicht auf eine
        // fremde Invariante - fail-closed statt UB.
        if (!current.activeMode.has_value() ||
            !proposal.activeMode.has_value()) {
            return rejected(SensorSelectionApplyStatus::InvalidDecision);
        }
        mutation.event = SensorSelectionEvent{
            *current.activeMode,           *proposal.activeMode, proposal.cause,
            mutation.resultingRunRevision, nowMonotonicMillis,   std::nullopt};
    } else {
        mutation.notice = SensorSelectionNotice{
            proposal.cause, nowMonotonicMillis, mutation.resultingRunRevision,
            current.activeMode.value_or(RunSensorMode::Product),
            proposal.blockReason};
    }

    return mutation;
}

RestartSensorSelectionRecommendation computeRestartSensorSelection(
    const PersistedSensorSelectionState& persisted,
    RunSensorMode lastActiveMode, const SensorSelectionProgramContext& program,
    const CrossRolePlausibilityContext& plausibility) {
    RestartSensorSelectionRecommendation recommendation;
    recommendation.runtime.phase =
        SensorSelectionPhase::RestartRevalidationPending;
    recommendation.runtime.permission = SensorPeltierPermission::Blocked;
    recommendation.activeMode = lastActiveMode;

    // Persistierte Provenienz darf den kanonischen aktiven Modus nicht
    // widersprechen. LegacyUnknown bleibt fuer Schema-1-Bestaende zulaessig;
    // die aktuelle Sensor-Evidenz muss den ebenfalls persistierten Modus aber
    // trotzdem vollstaendig neu beweisen.
    if ((persisted.provenance == SensorSelectionProvenance::FallbackActive &&
         lastActiveMode != RunSensorMode::Air) ||
        (persisted.provenance == SensorSelectionProvenance::ReturnedToProduct &&
         lastActiveMode != RunSensorMode::Product)) {
        return recommendation;
    }

    const bool airValid = usable(plausibility.air);
    const bool productValid = usable(plausibility.product);
    const bool coolingValid = usable(plausibility.cooling);

    bool modeAllowedByProgram = false;
    switch (program.sensorPreference) {
        case SensorPreference::ProductIfAvailableElseAir:
        case SensorPreference::AirProductOptional:
            modeAllowedByProgram = true;
            break;
        case SensorPreference::ProductRequired:
            modeAllowedByProgram = lastActiveMode == RunSensorMode::Product;
            break;
        case SensorPreference::AirOnly:
            modeAllowedByProgram = lastActiveMode == RunSensorMode::Air;
            break;
    }
    if (!modeAllowedByProgram) {
        return recommendation;
    }

    const bool fixedSensorsValid = airValid && coolingValid;
    const bool selectedSensorsValid =
        fixedSensorsValid &&
        (lastActiveMode == RunSensorMode::Air || productValid);
    if (!selectedSensorsValid) {
        return recommendation;
    }

    recommendation.runtime.permission = SensorPeltierPermission::Allowed;
    recommendation.runtime.lastAppliedMonotonicMillis =
        plausibility.evaluationMonotonicMillis;
    if (lastActiveMode == RunSensorMode::Air) {
        recommendation.runtime.phase =
            persisted.provenance == SensorSelectionProvenance::FallbackActive
                ? SensorSelectionPhase::AirFallbackActive
                : SensorSelectionPhase::NormalAir;
    } else {
        recommendation.runtime.phase = SensorSelectionPhase::NormalProduct;
    }
    return recommendation;
}

}  // namespace fermentation
