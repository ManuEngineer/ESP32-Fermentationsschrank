#include "run_recovery_time.hpp"

#include <limits>

namespace fermentation {
namespace {

[[nodiscard]] std::optional<std::uint64_t> checkedAddUint64(
    std::uint64_t left, std::uint64_t right) {
    if (right > std::numeric_limits<std::uint64_t>::max() - left) {
        return std::nullopt;
    }
    return left + right;
}

// Computes a non-negative int64 difference as uint64 without ever invoking
// signed-overflow undefined behaviour. Values with opposite signs can span
// the complete uint64 range, hence the result type is deliberately uint64.
[[nodiscard]] std::optional<std::uint64_t> checkedNonNegativeInt64Difference(
    std::int64_t later, std::int64_t earlier) {
    if (later < earlier) {
        return std::nullopt;
    }

    if (earlier < 0 && later >= 0) {
        const auto earlierMagnitude =
            static_cast<std::uint64_t>(-(earlier + 1)) + 1U;
        return static_cast<std::uint64_t>(later) + earlierMagnitude;
    }

    return static_cast<std::uint64_t>(later - earlier);
}

[[nodiscard]] std::optional<std::int64_t> checkedAddSignedUnsigned(
    std::int64_t base, std::uint64_t offset) {
    constexpr auto kSignedMaximum = std::numeric_limits<std::int64_t>::max();

    if (base >= 0) {
        const auto baseUnsigned = static_cast<std::uint64_t>(base);
        if (offset >
            static_cast<std::uint64_t>(kSignedMaximum) - baseUnsigned) {
            return std::nullopt;
        }
        return base + static_cast<std::int64_t>(offset);
    }

    // Avoid negating INT64_MIN directly. The magnitude is at most 2^63, so
    // adding it to INT64_MAX stays representable as uint64_t.
    const auto negativeMagnitude = static_cast<std::uint64_t>(-(base + 1)) + 1U;
    const auto maximumOffset =
        static_cast<std::uint64_t>(kSignedMaximum) + negativeMagnitude;
    if (offset > maximumOffset) {
        return std::nullopt;
    }

    if (offset >= negativeMagnitude) {
        return static_cast<std::int64_t>(offset - negativeMagnitude);
    }

    return base + static_cast<std::int64_t>(offset);
}

}  // namespace

std::optional<RecoveryOutageBounds> computeRecoveryOutageBounds(
    const RecoveryOutageBoundsInput& input) {
    if (!input.utcAtLastCheckpoint.has_value() ||
        !input.utcAtRestartBoundary.has_value()) {
        return std::nullopt;
    }

    const auto upperBound = checkedNonNegativeInt64Difference(
        *input.utcAtRestartBoundary, *input.utcAtLastCheckpoint);
    if (!upperBound.has_value()) {
        return std::nullopt;
    }

    const std::uint64_t lowerBound =
        input.maxCheckpointGapSeconds.has_value() &&
                *input.maxCheckpointGapSeconds < *upperBound
            ? *upperBound - *input.maxCheckpointGapSeconds
            : 0U;

    return RecoveryOutageBounds{*upperBound, lowerBound};
}

std::optional<RecoveredPhaseElapsed> computeRecoveredPhaseElapsed(
    const RecoveredPhaseElapsedInput& input) {
    const auto lowerBound =
        input.outage.has_value()
            ? checkedAddUint64(input.knownSecondsBeforeCheckpoint,
                               input.outage->outageSecondsLowerBound)
            : std::optional<std::uint64_t>{input.knownSecondsBeforeCheckpoint};
    if (!lowerBound.has_value()) {
        return std::nullopt;
    }

    std::optional<std::uint64_t> upperBound;
    if (input.outage.has_value()) {
        upperBound = checkedAddUint64(input.knownSecondsBeforeCheckpoint,
                                      input.outage->outageSecondsUpperBound);
        if (!upperBound.has_value()) {
            return std::nullopt;
        }
    }

    return RecoveredPhaseElapsed{input.knownSecondsBeforeCheckpoint,
                                 *lowerBound, upperBound};
}

RecoveryTimeVerdict evaluateRecoveryTimeVerdict(
    const RecoveredPhaseElapsed& elapsed, std::uint32_t limitSeconds) {
    if (elapsed.totalSecondsLowerBound >= limitSeconds) {
        return RecoveryTimeVerdict::DefinitelyExpired;
    }
    if (elapsed.totalSecondsUpperBound.has_value() &&
        *elapsed.totalSecondsUpperBound < limitSeconds) {
        return RecoveryTimeVerdict::DefinitelyStillValid;
    }
    return RecoveryTimeVerdict::Uncertain;
}

RecoveryConfidence deriveRecoveryConfidence(RecoveryTimeVerdict verdict,
                                            bool outageBoundsKnown) {
    if (verdict != RecoveryTimeVerdict::Uncertain) {
        return RecoveryConfidence::Strong;
    }
    return outageBoundsKnown ? RecoveryConfidence::Bounded
                             : RecoveryConfidence::Unknown;
}

std::optional<std::int64_t> deriveUtcAtRecoveryBootAnchor(
    std::optional<std::int64_t> utcNow, std::uint64_t nowMonotonicMillis,
    std::uint64_t recoveryBootAnchorMonotonicMillis) {
    if (!utcNow.has_value() || *utcNow < 0 ||
        nowMonotonicMillis < recoveryBootAnchorMonotonicMillis) {
        return std::nullopt;
    }

    const auto elapsedMillis =
        nowMonotonicMillis - recoveryBootAnchorMonotonicMillis;
    const auto elapsedSeconds = elapsedMillis / 1000U;
    if (elapsedSeconds >
        static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
        return std::nullopt;
    }

    const auto elapsedSecondsSigned = static_cast<std::int64_t>(elapsedSeconds);
    if (*utcNow < elapsedSecondsSigned) {
        return std::nullopt;
    }
    return *utcNow - elapsedSecondsSigned;
}

std::optional<EffectiveAnchorTimeBasis> deriveEffectiveAnchorTimeBasis(
    const PendingRecoveryAnchor& anchor) {
    const auto effectiveKnownSeconds =
        checkedAddUint64(anchor.knownPhaseSecondsAtOriginalCheckpoint,
                         anchor.knownSecondsSinceOriginalCheckpoint);
    if (!effectiveKnownSeconds.has_value()) {
        return std::nullopt;
    }

    std::optional<std::int64_t> effectiveCheckpointUtc;
    if (anchor.originalCheckpointUtc.has_value()) {
        effectiveCheckpointUtc = checkedAddSignedUnsigned(
            *anchor.originalCheckpointUtc,
            anchor.knownSecondsSinceOriginalCheckpoint);
    }

    return EffectiveAnchorTimeBasis{effectiveCheckpointUtc,
                                    *effectiveKnownSeconds};
}

std::optional<RecoveryTimeContext> deriveRecoveryTimeContext(
    const PendingRecoveryAnchor& anchor, std::optional<std::int64_t> utcNow,
    std::uint64_t nowMonotonicMillis,
    std::uint64_t recoveryBootAnchorMonotonicMillis) {
    const auto basis = deriveEffectiveAnchorTimeBasis(anchor);
    if (!basis.has_value()) return std::nullopt;

    const auto utcAtRecoveryBoot = deriveUtcAtRecoveryBootAnchor(
        utcNow, nowMonotonicMillis, recoveryBootAnchorMonotonicMillis);
    auto outage = computeRecoveryOutageBounds(RecoveryOutageBoundsInput{
        basis->effectiveCheckpointUtc, utcAtRecoveryBoot, std::nullopt,
        anchor.originalCheckpointTrigger});
    if (outage.has_value() && anchor.knownSecondsSinceOriginalCheckpoint > 0U) {
        outage->outageSecondsLowerBound = 0U;
    }
    const auto knownWithAccumulated =
        checkedAddUint64(anchor.accumulatedBeforeEpisode.lowerBoundSeconds,
                         basis->effectiveKnownSecondsBeforeCheckpoint);
    if (!knownWithAccumulated.has_value()) return std::nullopt;
    std::optional<RecoveryOutageBounds> boundedOutage = outage;
    if (boundedOutage.has_value() &&
        anchor.accumulatedBeforeEpisode.upperBoundSeconds.has_value()) {
        const auto accumulatedUpper =
            checkedAddUint64(*anchor.accumulatedBeforeEpisode.upperBoundSeconds,
                             basis->effectiveKnownSecondsBeforeCheckpoint);
        if (!accumulatedUpper.has_value()) return std::nullopt;
        const auto upper = checkedAddUint64(
            *accumulatedUpper, boundedOutage->outageSecondsUpperBound);
        if (!upper.has_value()) return std::nullopt;
        // `computeRecoveredPhaseElapsed` takes a single known base and adds
        // the outage interval symmetrically. The lower side is handled below
        // because an accumulated upper bound is required before an upper
        // result can be exposed.
        const auto lower = checkedAddUint64(
            *knownWithAccumulated, boundedOutage->outageSecondsLowerBound);
        if (!lower.has_value()) return std::nullopt;
        return RecoveryTimeContext{
            *basis, boundedOutage,
            RecoveredPhaseElapsed{basis->effectiveKnownSecondsBeforeCheckpoint,
                                  *lower, upper}};
    }
    const auto elapsed = computeRecoveredPhaseElapsed(
        RecoveredPhaseElapsedInput{*knownWithAccumulated, outage});
    if (!elapsed.has_value()) return std::nullopt;
    return RecoveryTimeContext{*basis, outage, *elapsed};
}

}  // namespace fermentation
