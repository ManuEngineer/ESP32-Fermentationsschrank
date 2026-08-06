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
           ctx.direction != AbstractControlDirection::Unknown &&
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
           ctx.thermalCompatibility.status == ThermalCompatibility::Incompatible;
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
                    .has_value();
    }
    const bool mustBeBlocked =
        runtime.phase == SensorSelectionPhase::SafeLocked ||
        runtime.phase == SensorSelectionPhase::ProductFailureDetected ||
        runtime.phase == SensorSelectionPhase::UserDecisionRequired ||
        runtime.phase == SensorSelectionPhase::RestartRevalidationPending;
    if (mustBeBlocked && runtime.permission != SensorPeltierPermission::Blocked) {
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
    return true;
}

// Vorschlag eines Phasen-Handlers: die vollstaendige neue Runtime/Aktiv-
// modus-Kombination (nicht nur ein Diff), plus Klassifikation fuer 6.4.9.
struct Proposal {
    SensorSelectionRuntimeState runtime;
    std::optional<RunSensorMode> activeMode;
    SensorSelectionDecisionCause cause{SensorSelectionDecisionCause::None};
    SensorSelectionBlockReason blockReason{SensorSelectionBlockReason::None};
    bool persistWorthy{false};
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
        proposal.persistWorthy = true;
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
            proposal.persistWorthy = true;
            return proposal;
        }
        proposal.runtime.permission = SensorPeltierPermission::Blocked;
        proposal.cause = SensorSelectionDecisionCause::ProductFailureBlock;
        proposal.blockReason = SensorSelectionBlockReason::ProductSensorUnusable;
        proposal.persistWorthy = true;
        if (decision.program.policy ==
            ProductSensorFailurePolicy::WaitForUser) {
            // 6.4.10-Korrektur: keine Wartezeit fuer WaitForUser - sofortiger
            // fluechtiger Uebergang im selben Bewertungszyklus.
            proposal.runtime.phase = SensorSelectionPhase::UserDecisionRequired;
            proposal.runtime.fallbackWaitStartedAtMonotonicMillis = std::nullopt;
        } else {
            proposal.runtime.phase = SensorSelectionPhase::ProductFailureDetected;
            proposal.runtime.fallbackWaitStartedAtMonotonicMillis = now;
        }
        return proposal;
    }

    const bool singleFixedFailure = !airValid || !coolingValid;
    if (singleFixedFailure) {
        if (current.runtime.permission == SensorPeltierPermission::Allowed) {
            proposal.runtime.permission = SensorPeltierPermission::Blocked;
            proposal.cause = SensorSelectionDecisionCause::ProductFailureBlock;
            proposal.blockReason = fixedSensorBlockReason(airValid, coolingValid);
            proposal.persistWorthy = true;
        }
        return proposal;
    }

    if (current.runtime.permission == SensorPeltierPermission::Blocked) {
        proposal.runtime.permission = SensorPeltierPermission::Allowed;
        proposal.cause = SensorSelectionDecisionCause::RecoveryRevalidation;
        proposal.persistWorthy = true;
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
    if (!allValid && current.runtime.permission == SensorPeltierPermission::Allowed) {
        proposal.runtime.permission = SensorPeltierPermission::Blocked;
        proposal.cause = SensorSelectionDecisionCause::ProductFailureBlock;
        proposal.blockReason = fixedSensorBlockReason(airValid, coolingValid);
        proposal.persistWorthy = true;
    } else if (allValid &&
              current.runtime.permission == SensorPeltierPermission::Blocked) {
        proposal.runtime.permission = SensorPeltierPermission::Allowed;
        proposal.cause = SensorSelectionDecisionCause::RecoveryRevalidation;
        proposal.persistWorthy = true;
    }
    return proposal;
}

Proposal continueWithAirFromProductPhase(const SensorSelectionStateView& current,
                                         const SensorSelectionDecision& decision,
                                         bool airValid, bool coolingValid) {
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
    proposal.persistWorthy = true;
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

    if (decision.userAction.has_value()) {
        switch (*decision.userAction) {
            case SensorSelectionUserAction::ContinueWithAir:
                return continueWithAirFromProductPhase(current, decision, airValid,
                                                       coolingValid);
            case SensorSelectionUserAction::RecheckProduct:
                if (productValid) {
                    proposal.runtime.phase = SensorSelectionPhase::NormalProduct;
                    proposal.runtime.permission = SensorPeltierPermission::Allowed;
                    proposal.runtime.fallbackWaitStartedAtMonotonicMillis =
                        std::nullopt;
                    proposal.cause = SensorSelectionDecisionCause::RecoveryRevalidation;
                    proposal.persistWorthy = true;
                }
                return proposal;
            case SensorSelectionUserAction::ReturnToProduct:
                return reject(SensorSelectionApplyStatus::InvalidDecision);
        }
    }

    if (!airValid && !coolingValid) {
        proposal.runtime.phase = SensorSelectionPhase::SafeLocked;
        proposal.runtime.permission = SensorPeltierPermission::Blocked;
        proposal.runtime.fallbackWaitStartedAtMonotonicMillis = std::nullopt;
        proposal.cause = SensorSelectionDecisionCause::SafeStateEntry;
        proposal.blockReason =
            SensorSelectionBlockReason::SimultaneousFixedSensorFailure;
        proposal.persistWorthy = true;
        return proposal;
    }

    if (productValid) {
        proposal.runtime.phase = SensorSelectionPhase::NormalProduct;
        proposal.runtime.permission = SensorPeltierPermission::Allowed;
        proposal.runtime.fallbackWaitStartedAtMonotonicMillis = std::nullopt;
        proposal.cause = SensorSelectionDecisionCause::RecoveryRevalidation;
        proposal.persistWorthy = true;
        return proposal;
    }

    if (decision.program.policy != ProductSensorFailurePolicy::FallbackToAirAfterTimeout) {
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
        proposal.persistWorthy = true;
        return proposal;
    }
    proposal.runtime.phase = SensorSelectionPhase::AirFallbackActive;
    proposal.runtime.permission = SensorPeltierPermission::Allowed;
    proposal.runtime.fallbackWaitStartedAtMonotonicMillis = std::nullopt;
    proposal.activeMode = RunSensorMode::Air;
    proposal.cause = SensorSelectionDecisionCause::FallbackToAir;
    proposal.persistWorthy = true;
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

    if (decision.userAction.has_value()) {
        switch (*decision.userAction) {
            case SensorSelectionUserAction::ContinueWithAir:
                return continueWithAirFromProductPhase(current, decision, airValid,
                                                       coolingValid);
            case SensorSelectionUserAction::RecheckProduct:
                if (productValid) {
                    proposal.runtime.phase = SensorSelectionPhase::NormalProduct;
                    proposal.runtime.permission = SensorPeltierPermission::Allowed;
                    proposal.cause = SensorSelectionDecisionCause::RecoveryRevalidation;
                    proposal.persistWorthy = true;
                }
                return proposal;
            case SensorSelectionUserAction::ReturnToProduct:
                return reject(SensorSelectionApplyStatus::InvalidDecision);
        }
    }

    // Kein "gleichzeitig"-Qualifier in 6.4.2 fuer diese Zeile - ein einzelner
    // Ausfall genuegt (anders als bei ProductFailureDetected).
    if (!airValid || !coolingValid) {
        proposal.runtime.phase = SensorSelectionPhase::SafeLocked;
        proposal.runtime.permission = SensorPeltierPermission::Blocked;
        proposal.cause = SensorSelectionDecisionCause::SafeStateEntry;
        proposal.blockReason = fixedSensorBlockReason(airValid, coolingValid);
        proposal.persistWorthy = true;
        return proposal;
    }
    if (productValid) {
        proposal.runtime.phase = SensorSelectionPhase::NormalProduct;
        proposal.runtime.permission = SensorPeltierPermission::Allowed;
        proposal.cause = SensorSelectionDecisionCause::RecoveryRevalidation;
        proposal.persistWorthy = true;
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
                proposal.runtime.returnValidation = ReturnValidationRuntimeState{};
                proposal.activeMode = RunSensorMode::Product;
                proposal.cause = SensorSelectionDecisionCause::ManualUserReturn;
                proposal.persistWorthy = true;
                return proposal;
            }
            case SensorSelectionUserAction::RecheckProduct: {
                if (!productValid) {
                    return proposal;  // NoChange
                }
                if (decision.program.returnStrategy !=
                    ReturnStrategy::AutomaticValidatedReturnToProduct) {
                    return proposal;  // keine automatische Strategie -> NoChange
                }
                if (fullyPositiveReturnEvidence(ctx, productValid, airValid,
                                                coolingValid)) {
                    proposal.runtime.phase = SensorSelectionPhase::NormalProduct;
                    proposal.runtime.permission = SensorPeltierPermission::Allowed;
                    proposal.runtime.returnValidation =
                        ReturnValidationRuntimeState{};
                    proposal.activeMode = RunSensorMode::Product;
                    proposal.cause =
                        SensorSelectionDecisionCause::AutomaticValidatedReturn;
                    proposal.persistWorthy = true;
                    return proposal;
                }
                if (definitivelyIncompatible(ctx)) {
                    return proposal;  // bekannt inkompatibel -> keine neue Chance
                }
                // 6.4.12-Sonderfall: unvollstaendige/Unavailable/Stale-Evidenz.
                proposal.runtime.phase = SensorSelectionPhase::ReturnValidationPending;
                proposal.runtime.returnValidation.enteredAtMonotonicMillis = now;
                proposal.runtime.returnValidation.lastObservedProfileRevision =
                    ctx.thermalCompatibility.profileRevision;
                return proposal;  // AppliedRamOnly
            }
        }
    }

    if (!airValid || !coolingValid) {
        proposal.runtime.phase = SensorSelectionPhase::SafeLocked;
        proposal.runtime.permission = SensorPeltierPermission::Blocked;
        proposal.runtime.returnValidation = ReturnValidationRuntimeState{};
        proposal.cause = SensorSelectionDecisionCause::SafeStateEntry;
        proposal.blockReason = fixedSensorBlockReason(airValid, coolingValid);
        proposal.persistWorthy = true;
        return proposal;
    }

    if (decision.program.returnStrategy ==
            ReturnStrategy::AutomaticValidatedReturnToProduct &&
        productValid) {
        // Re-Arm-Regel (6.4.12): ohne vorherigen Eintrag oder mit
        // geaenderter profileRevision seit dem letzten Eintritt neu
        // eintreten; sonst kein automatischer Re-Arm (verhindert eine
        // unbegrenzte Bewertungsschleife pro Zyklus).
        const bool profileChanged =
            !current.runtime.returnValidation.lastObservedProfileRevision
                 .has_value() ||
            *current.runtime.returnValidation.lastObservedProfileRevision !=
                ctx.thermalCompatibility.profileRevision;
        if (profileChanged) {
            proposal.runtime.phase = SensorSelectionPhase::ReturnValidationPending;
            proposal.runtime.returnValidation.enteredAtMonotonicMillis = now;
            proposal.runtime.returnValidation.lastObservedProfileRevision =
                ctx.thermalCompatibility.profileRevision;
        }
    }
    return proposal;
}

// 6.4.7 ReturnValidationPending: Regelung laeuft auf Air unveraendert weiter
// (Allowed, solange Air/Cooling gueltig). Einzelner Air-/Cooling-Ausfall
// gewinnt per Vorrangregel gegenueber einem gleichzeitig moeglichen Abbruch
// (c vor d).
Proposal evaluateReturnValidationPending(const SensorSelectionStateView& current,
                                         const SensorSelectionDecision& decision,
                                         std::uint64_t /*now*/) {
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
        proposal.cause = SensorSelectionDecisionCause::SafeStateEntry;
        proposal.blockReason = fixedSensorBlockReason(airValid, coolingValid);
        proposal.persistWorthy = true;
        return proposal;
    }

    if (!productValid || definitivelyIncompatible(ctx)) {
        proposal.runtime.phase = SensorSelectionPhase::AirFallbackActive;
        // lastObservedProfileRevision bleibt erhalten (Re-Arm-Grundlage,
        // 6.4.12); nur der Eintrittszeitstempel wird verworfen.
        proposal.runtime.returnValidation.enteredAtMonotonicMillis = std::nullopt;
        proposal.cause = SensorSelectionDecisionCause::ReturnValidationAborted;
        proposal.persistWorthy = true;
        return proposal;
    }

    if (fullyPositiveReturnEvidence(ctx, productValid, airValid, coolingValid)) {
        proposal.runtime.phase = SensorSelectionPhase::NormalProduct;
        proposal.runtime.permission = SensorPeltierPermission::Allowed;
        proposal.runtime.returnValidation = ReturnValidationRuntimeState{};
        proposal.activeMode = RunSensorMode::Product;
        proposal.cause = SensorSelectionDecisionCause::AutomaticValidatedReturn;
        proposal.persistWorthy = true;
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

    const Proposal proposal = evaluatePhase(current, decision, nowMonotonicMillis);
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

    if (!proposal.persistWorthy) {
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
        mutation.event = SensorSelectionEvent{
            *current.activeMode,       *proposal.activeMode,
            proposal.cause,            mutation.resultingRunRevision,
            nowMonotonicMillis,        std::nullopt};
    } else {
        mutation.notice = SensorSelectionNotice{
            proposal.cause, nowMonotonicMillis, mutation.resultingRunRevision,
            current.activeMode.value_or(RunSensorMode::Product),
            proposal.blockReason};
    }

    return mutation;
}

RestartSensorSelectionRecommendation computeRestartSensorSelection(
    const PersistedSensorSelectionState& persisted, RunSensorMode lastActiveMode,
    const SensorSelectionProgramContext& program) {
    // Reine Datenaufbereitung fuer #18: die tatsaechliche Reaktivierung
    // (LoadedActiveRun -> Ready) bleibt vollstaendig #18 vorbehalten (6.12.3).
    // Permission bleibt fail-closed Blocked, jede laufende Wartezeit-/
    // Rueckkehrvalidierung beginnt nach einem Neustart zwingend bei Null
    // (6.4.7), unabhaengig von provenance/Policy.
    static_cast<void>(persisted);
    static_cast<void>(program);
    RestartSensorSelectionRecommendation recommendation;
    recommendation.runtime.phase = SensorSelectionPhase::RestartRevalidationPending;
    recommendation.runtime.permission = SensorPeltierPermission::Blocked;
    recommendation.activeMode = lastActiveMode;
    return recommendation;
}

}  // namespace fermentation
