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
    QualificationProgress qualificationProgress{
        QualificationProgress::Unavailable};
    bool coolingTargetConditionValid{false};
    bool criticalFault{false};
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
            return false;
    }
    return false;
}

[[nodiscard]] inline bool qualificationIsComplete(
    const ProcessSignals& signals) {
    return signals.qualificationProgress == QualificationProgress::Complete;
}

[[nodiscard]] inline bool qualificationIsInterrupted(
    const ProcessSignals& signals) {
    switch (signals.qualificationProgress) {
        case QualificationProgress::Unavailable:
        case QualificationProgress::Invalid:
        case QualificationProgress::OutsideBand:
            return true;
        case QualificationProgress::Grace:
        case QualificationProgress::InBand:
        case QualificationProgress::Complete:
            return false;
    }
    return true;
}

}  // namespace fermentation
