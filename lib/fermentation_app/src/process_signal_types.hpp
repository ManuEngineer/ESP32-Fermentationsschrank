#pragma once

#include <cstdint>

namespace fermentation {

// Prozesssignal aus dem Qualifikationskern. Die Zustandsmaschine entscheidet
// weiterhin allein ueber die Topologie; dieser Typ beschreibt nur die
// beobachtete Evidenz des aktuellen Zyklus.
enum class QualificationProgress : std::uint8_t {
    Unavailable,
    Invalid,
    OutsideBand,
    Grace,
    InBand,
    Complete,
};

struct ProcessSignals {
    // Kompatibilitaetsfeld fuer bereits bestehende Aufrufer des
    // Zustandsautomaten. Neue Orchestratoren verwenden qualificationProgress
    // und coolingTargetConditionValid. Es wird intern nicht als Cooling-Signal
    // wiederverwendet.
    bool qualificationConditionValid{false};
    bool criticalFault{false};
    QualificationProgress qualificationProgress{
        QualificationProgress::Unavailable};
    bool coolingTargetConditionValid{false};
};

[[nodiscard]] inline bool qualificationHasPositiveEvidence(
    const ProcessSignals& signals) {
    switch (signals.qualificationProgress) {
        case QualificationProgress::Grace:
        case QualificationProgress::InBand:
        case QualificationProgress::Complete:
            return true;
        case QualificationProgress::Unavailable:
        case QualificationProgress::Invalid:
        case QualificationProgress::OutsideBand:
            return signals.qualificationConditionValid;
    }
    return false;
}

[[nodiscard]] inline bool qualificationIsComplete(
    const ProcessSignals& signals) {
    return signals.qualificationProgress == QualificationProgress::Complete ||
           (signals.qualificationProgress ==
                QualificationProgress::Unavailable &&
            signals.qualificationConditionValid);
}

[[nodiscard]] inline bool qualificationIsInterrupted(
    const ProcessSignals& signals) {
    switch (signals.qualificationProgress) {
        case QualificationProgress::Unavailable:
        case QualificationProgress::Invalid:
        case QualificationProgress::OutsideBand:
            return !signals.qualificationConditionValid;
        case QualificationProgress::Grace:
        case QualificationProgress::InBand:
        case QualificationProgress::Complete:
            return false;
    }
    return true;
}

}  // namespace fermentation
