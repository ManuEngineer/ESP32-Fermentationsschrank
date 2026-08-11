#pragma once

#include <cstdint>
#include <optional>

#include "run_recovery_types.hpp"

namespace fermentation {

// Reine Eingabe fuer die Ausfallgrenzen. Die beiden UTC-Werte sind bereits
// auf den fachlich richtigen Boot-Anker abgeleitet; diese Funktion liest keine
// Zeitquelle und kennt keinen Boot-Zustand.
struct RecoveryOutageBoundsInput {
    std::optional<std::int64_t> utcAtLastCheckpoint;
    std::optional<std::int64_t> utcAtRestartBoundary;
    std::optional<std::uint32_t> maxCheckpointGapSeconds;
    RunCheckpointTrigger lastCheckpointTrigger{RunCheckpointTrigger::Command};
};

struct RecoveryOutageBounds {
    std::uint64_t outageSecondsUpperBound{0U};
    std::uint64_t outageSecondsLowerBound{0U};
};

[[nodiscard]] std::optional<RecoveryOutageBounds> computeRecoveryOutageBounds(
    const RecoveryOutageBoundsInput& input);

struct RecoveredPhaseElapsedInput {
    std::uint64_t knownSecondsBeforeCheckpoint{0U};
    std::optional<RecoveryOutageBounds> outage;
};

struct RecoveredPhaseElapsed {
    std::uint64_t knownSecondsBeforeCheckpoint{0U};
    std::uint64_t totalSecondsLowerBound{0U};
    std::optional<std::uint64_t> totalSecondsUpperBound;
};

[[nodiscard]] std::optional<RecoveredPhaseElapsed> computeRecoveredPhaseElapsed(
    const RecoveredPhaseElapsedInput& input);

enum class RecoveryTimeVerdict : std::uint8_t {
    DefinitelyStillValid,
    DefinitelyExpired,
    Uncertain,
};

[[nodiscard]] RecoveryTimeVerdict evaluateRecoveryTimeVerdict(
    const RecoveredPhaseElapsed& elapsed, std::uint32_t limitSeconds);

enum class RecoveryConfidence : std::uint8_t {
    Unknown,
    Bounded,
    Strong,
};

[[nodiscard]] RecoveryConfidence deriveRecoveryConfidence(
    RecoveryTimeVerdict verdict, bool outageBoundsKnown);

[[nodiscard]] std::optional<std::int64_t> deriveUtcAtRecoveryBootAnchor(
    std::optional<std::int64_t> utcNow, std::uint64_t nowMonotonicMillis,
    std::uint64_t recoveryBootAnchorMonotonicMillis);

struct EffectiveAnchorTimeBasis {
    std::optional<std::int64_t> effectiveCheckpointUtc;
    std::uint64_t effectiveKnownSecondsBeforeCheckpoint{0U};
};

// `nullopt` is reserved for checked arithmetic failure. A missing original
// UTC remains a valid, unresolved basis and is represented by
// effectiveCheckpointUtc == nullopt inside the result.
[[nodiscard]] std::optional<EffectiveAnchorTimeBasis>
deriveEffectiveAnchorTimeBasis(const PendingRecoveryAnchor& anchor);

struct RecoveryTimeContext {
    EffectiveAnchorTimeBasis basis;
    std::optional<RecoveryOutageBounds> outage;
    RecoveredPhaseElapsed elapsed;
};

// Gemeinsame, checked Ableitung fuer Hop-1-Verdicts, Benutzer-Reevaluation
// und die spaetere Recovery-Zeitkorrektur. Der UTC-Wert wird am aktuellen
// Boot-Anker abgeleitet; ein Carry-Forward-Anker erhaelt keine kuenstliche
// Ausfall-Untergrenze.
[[nodiscard]] std::optional<RecoveryTimeContext> deriveRecoveryTimeContext(
    const PendingRecoveryAnchor& anchor, std::optional<std::int64_t> utcNow,
    std::uint64_t nowMonotonicMillis,
    std::uint64_t recoveryBootAnchorMonotonicMillis);

}  // namespace fermentation
