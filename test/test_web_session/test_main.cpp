#include <unity.h>

#include <cstdint>
#include <limits>
#include <string>
#include <string_view>

#include "web_session.hpp"

namespace fermentation {

struct WebSessionManagerTestAccess {
    static bool setHighWater(WebSessionManager& manager,
                             WebSessionHandle handle, std::uint64_t highWater,
                             bool clearReplayWindow = false) {
        std::lock_guard<std::mutex> lock(manager.mutex_);
        if (handle.slot >= manager.sessions_.size()) return false;
        auto& session = manager.sessions_[handle.slot];
        if (!session.active || session.generation != handle.generation)
            return false;
        if (clearReplayWindow) {
            session.inFlight.reset();
            session.completed = {};
            session.completedCount = 0U;
            session.replayFloor = 0U;
        }
        session.highWater = highWater;
        return true;
    }
};

}  // namespace fermentation

namespace {

class Random final : public device_platform::ISecureRandomSource {
   public:
    bool fill(void* buffer, std::size_t length) override {
        if (length == 0U) return true;
        if (buffer == nullptr) return false;
        auto* bytes = static_cast<std::uint8_t*>(buffer);
        for (std::size_t i = 0U; i < length; ++i) bytes[i] = next_++;
        return true;
    }

   private:
    std::uint8_t next_{1U};
};

fermentation::ReplayDigest testDigest(std::string_view value) {
    fermentation::ReplayDigest digest{};
    std::uint32_t state = 2166136261U;
    for (const auto byte : value) {
        state ^= static_cast<std::uint8_t>(byte);
        state *= 16777619U;
    }
    for (std::size_t index = 0U; index < digest.size(); ++index) {
        state ^= static_cast<std::uint32_t>(index + 1U);
        state *= 16777619U;
        digest[index] = static_cast<std::uint8_t>(state >> 24U);
    }
    return digest;
}

class CapturingDigest final : public fermentation::IReplayDigest {
   public:
    bool digest(const fermentation::ReplayDigestInput& input,
                fermentation::ReplayDigest& out) override {
        stream.clear();
        stream.append(input.method.data(), input.method.size());
        stream.push_back('\0');
        stream.append(input.path.data(), input.path.size());
        stream.push_back('\0');
        stream.append(input.bodyLength.data(), input.bodyLength.size());
        stream.push_back('\0');
        stream.append(input.body.data(), input.body.size());
        out = testDigest(stream);
        return true;
    }

    std::string stream;
};

void test_more_than_eight_mutations_do_not_exhaust_session() {
    Random random;
    fermentation::WebSessionManager sessions(random);
    const auto created = sessions.create(0U);
    TEST_ASSERT_TRUE(created.handle.has_value());
    for (std::uint64_t sequence = 1U; sequence <= 20U; ++sequence) {
        const auto reserved = sessions.reserveMutation(
            *created.handle, sequence, sequence,
            testDigest("fingerprint-" + std::to_string(sequence)));
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(fermentation::MutationReservationStatus::Reserved),
            static_cast<int>(reserved.status));
        TEST_ASSERT_TRUE(sessions.completeMutation(
            *created.handle, sequence, sequence,
            testDigest("fingerprint-" + std::to_string(sequence)),
            fermentation::replayOutcome(
                fermentation::ReplayOutcomeCode::Applied)));
    }
    const auto view = sessions.mutationSequence(*created.handle, 1U);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::MutationSequenceState::Available),
        static_cast<int>(view.state));
    TEST_ASSERT_EQUAL_UINT64(21U, view.nextMutationSeq);
}

void test_replay_and_reuse_are_not_second_mutations() {
    Random random;
    fermentation::WebSessionManager sessions(random);
    const auto created = sessions.create(0U);
    TEST_ASSERT_TRUE(created.handle.has_value());
    const auto first =
        sessions.reserveMutation(*created.handle, 1U, 1U, testDigest("same"));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::MutationReservationStatus::Reserved),
        static_cast<int>(first.status));
    TEST_ASSERT_TRUE(sessions.completeMutation(
        *created.handle, 1U, 1U, testDigest("same"),
        fermentation::replayOutcome(fermentation::ReplayOutcomeCode::Applied)));
    const auto replay =
        sessions.reserveMutation(*created.handle, 2U, 1U, testDigest("same"));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            fermentation::MutationReservationStatus::ReplayOutcome),
        static_cast<int>(replay.status));
    TEST_ASSERT_TRUE(replay.outcome.has_value());
    TEST_ASSERT_EQUAL_UINT16(200U, replay.outcome->statusCode);
    TEST_ASSERT_EQUAL_STRING("application/json; charset=utf-8",
                             replay.outcome->contentType.c_str());
    TEST_ASSERT_EQUAL_STRING("{\"outcome\":\"applied\"}",
                             replay.outcome->body.c_str());
    const auto reused =
        sessions.reserveMutation(*created.handle, 2U, 1U, testDigest("other"));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            fermentation::MutationReservationStatus::SequenceReused),
        static_cast<int>(reused.status));
}

void test_two_tabs_share_csrf_sequence_and_replay_outcome() {
    Random random;
    fermentation::WebSessionManager sessions(random);
    const auto created = sessions.create(0U);
    TEST_ASSERT_TRUE(created.handle.has_value());
    const auto cookie = "FSSESSION=" + created.cookieValue;
    const auto firstTab = sessions.find(cookie, 1U);
    const auto secondTab = sessions.find(cookie, 2U);
    const auto otherSession = sessions.create(2U);
    TEST_ASSERT_TRUE(firstTab.handle.has_value());
    TEST_ASSERT_TRUE(secondTab.handle.has_value());
    TEST_ASSERT_TRUE(otherSession.handle.has_value());
    TEST_ASSERT_TRUE(*firstTab.handle == *secondTab.handle);
    TEST_ASSERT_EQUAL_STRING(firstTab.csrfToken.c_str(),
                             secondTab.csrfToken.c_str());
    TEST_ASSERT_TRUE(
        sessions.validateCsrf(*firstTab.handle, firstTab.csrfToken, 2U));
    TEST_ASSERT_FALSE(
        sessions.validateCsrf(*firstTab.handle, secondTab.csrfToken + "x", 2U));
    TEST_ASSERT_FALSE(
        sessions.validateCsrf(*firstTab.handle, otherSession.csrfToken, 2U));
    TEST_ASSERT_FALSE(
        sessions.validateCsrf(*otherSession.handle, firstTab.csrfToken, 2U));

    const auto fingerprint = testDigest("post-run-payload");
    const auto owner =
        sessions.reserveMutation(*firstTab.handle, 2U, 1U, fingerprint);
    const auto sameRequest =
        sessions.reserveMutation(*secondTab.handle, 2U, 1U, fingerprint);
    const auto changedRequest = sessions.reserveMutation(
        *secondTab.handle, 2U, 1U, testDigest("changed-payload"));
    const auto competingSequence = sessions.reserveMutation(
        *secondTab.handle, 2U, 2U, testDigest("next-payload"));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::MutationReservationStatus::Reserved),
        static_cast<int>(owner.status));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::MutationReservationStatus::InFlight),
        static_cast<int>(sameRequest.status));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            fermentation::MutationReservationStatus::SequenceReused),
        static_cast<int>(changedRequest.status));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            fermentation::MutationReservationStatus::SequenceConflict),
        static_cast<int>(competingSequence.status));

    const fermentation::WebMutationOutcome outcome =
        fermentation::replayOutcome(fermentation::ReplayOutcomeCode::Applied);
    TEST_ASSERT_TRUE(sessions.completeMutation(*firstTab.handle, 2U, 1U,
                                               fingerprint, outcome));
    const auto retry =
        sessions.reserveMutation(*secondTab.handle, 3U, 1U, fingerprint);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            fermentation::MutationReservationStatus::ReplayOutcome),
        static_cast<int>(retry.status));
    TEST_ASSERT_TRUE(retry.outcome.has_value());
    TEST_ASSERT_EQUAL_UINT16(outcome.statusCode, retry.outcome->statusCode);
    TEST_ASSERT_EQUAL_STRING(outcome.contentType.c_str(),
                             retry.outcome->contentType.c_str());
    TEST_ASSERT_EQUAL_STRING(outcome.body.c_str(), retry.outcome->body.c_str());
    TEST_ASSERT_EQUAL_UINT64(
        2U, sessions.mutationSequence(*secondTab.handle, 3U).nextMutationSeq);
}

void test_browser_reload_preserves_live_session_csrf_and_replay_state() {
    Random random;
    fermentation::WebSessionManager sessions(random);
    const auto created = sessions.create(0U);
    TEST_ASSERT_TRUE(created.handle.has_value());
    const auto outcome = sessions.reserveMutation(
        *created.handle, 1U, 1U, testDigest("reload-stable-request"));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::MutationReservationStatus::Reserved),
        static_cast<int>(outcome.status));
    TEST_ASSERT_TRUE(sessions.completeMutation(
        *created.handle, 1U, 1U, testDigest("reload-stable-request"),
        fermentation::replayOutcome(fermentation::ReplayOutcomeCode::Applied)));

    const auto reloaded = sessions.find("FSSESSION=" + created.cookieValue, 2U);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::WebSessionStatus::Found),
        static_cast<int>(reloaded.status));
    TEST_ASSERT_TRUE(reloaded.handle.has_value());
    TEST_ASSERT_TRUE(*created.handle == *reloaded.handle);
    TEST_ASSERT_EQUAL_STRING(created.csrfToken.c_str(),
                             reloaded.csrfToken.c_str());
    const auto sequence = sessions.mutationSequence(*reloaded.handle, 2U);
    TEST_ASSERT_EQUAL_UINT64(2U, sequence.nextMutationSeq);
    const auto retry = sessions.reserveMutation(
        *reloaded.handle, 2U, 1U, testDigest("reload-stable-request"));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            fermentation::MutationReservationStatus::ReplayOutcome),
        static_cast<int>(retry.status));
    TEST_ASSERT_TRUE(retry.outcome.has_value());
    TEST_ASSERT_EQUAL_STRING("{\"outcome\":\"applied\"}",
                             retry.outcome->body.c_str());
}

void test_inflight_and_old_retired_values_fail_closed() {
    Random random;
    fermentation::WebSessionManager sessions(random);
    const auto created = sessions.create(0U);
    TEST_ASSERT_TRUE(created.handle.has_value());
    const auto inFlight =
        sessions.reserveMutation(*created.handle, 0U, 1U, testDigest("a"));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::MutationReservationStatus::Reserved),
        static_cast<int>(inFlight.status));
    const auto loser =
        sessions.reserveMutation(*created.handle, 0U, 2U, testDigest("b"));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            fermentation::MutationReservationStatus::SequenceConflict),
        static_cast<int>(loser.status));
    TEST_ASSERT_TRUE(sessions.completeMutation(
        *created.handle, 0U, 1U, testDigest("a"),
        fermentation::replayOutcome(fermentation::ReplayOutcomeCode::Applied)));
    for (std::uint64_t sequence = 2U; sequence <= 10U; ++sequence) {
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(fermentation::MutationReservationStatus::Reserved),
            static_cast<int>(
                sessions
                    .reserveMutation(*created.handle, 0U, sequence,
                                     testDigest("x" + std::to_string(sequence)))
                    .status));
        TEST_ASSERT_TRUE(sessions.completeMutation(
            *created.handle, 0U, sequence,
            testDigest("x" + std::to_string(sequence)),
            fermentation::replayOutcome(
                fermentation::ReplayOutcomeCode::Applied)));
    }
    const auto expired =
        sessions.reserveMutation(*created.handle, 0U, 1U, testDigest("same"));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            fermentation::MutationReservationStatus::ReplayExpired),
        static_cast<int>(expired.status));
}

void test_session_expiry_and_capacity() {
    Random random;
    fermentation::WebSessionManager sessions(random);
    auto first = sessions.create(0U);
    TEST_ASSERT_TRUE(first.handle.has_value());
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::WebSessionStatus::Found),
        static_cast<int>(
            sessions.find("FSSESSION=" + first.cookieValue, 1U).status));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::WebSessionStatus::Expired),
        static_cast<int>(sessions
                             .find("FSSESSION=" + first.cookieValue,
                                   fermentation::kWebSessionIdleLimitMs + 1U)
                             .status));
    for (std::size_t i = 0U; i < fermentation::kMaximumWebSessions; ++i)
        TEST_ASSERT_TRUE(sessions.create(1U + i).handle.has_value());
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::WebSessionStatus::Capacity),
        static_cast<int>(sessions.create(99U).status));
}

void test_repeated_find_does_not_extend_idle_deadline() {
    Random random;
    fermentation::WebSessionManager sessions(random);
    const auto created = sessions.create(0U);
    TEST_ASSERT_TRUE(created.handle.has_value());
    const auto cookie = "FSSESSION=" + created.cookieValue;
    constexpr std::uint64_t intervalMs = 60U * 1000U;

    for (std::uint64_t nowMs = intervalMs;
         nowMs < fermentation::kWebSessionIdleLimitMs; nowMs += intervalMs) {
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(fermentation::WebSessionStatus::Found),
            static_cast<int>(sessions.find(cookie, nowMs).status));
    }
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::WebSessionStatus::Expired),
        static_cast<int>(
            sessions.find(cookie, fermentation::kWebSessionIdleLimitMs)
                .status));
}

void test_touch_before_idle_expiry_extends_idle_deadline() {
    Random random;
    fermentation::WebSessionManager sessions(random);
    const auto created = sessions.create(0U);
    TEST_ASSERT_TRUE(created.handle.has_value());
    const auto cookie = "FSSESSION=" + created.cookieValue;
    constexpr auto touchAtMs = fermentation::kWebSessionIdleLimitMs / 2U;
    TEST_ASSERT_TRUE(sessions.touch(*created.handle, touchAtMs));

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::WebSessionStatus::Found),
        static_cast<int>(
            sessions.find(cookie, fermentation::kWebSessionIdleLimitMs)
                .status));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::WebSessionStatus::Expired),
        static_cast<int>(
            sessions
                .find(cookie, touchAtMs + fermentation::kWebSessionIdleLimitMs)
                .status));
}

void test_browser_reload_lookup_does_not_extend_idle_deadline() {
    Random random;
    fermentation::WebSessionManager sessions(random);
    const auto created = sessions.create(0U);
    TEST_ASSERT_TRUE(created.handle.has_value());
    const auto cookie = "FSSESSION=" + created.cookieValue;
    const auto reload =
        sessions.find(cookie, fermentation::kWebSessionIdleLimitMs - 1U);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::WebSessionStatus::Found),
        static_cast<int>(reload.status));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::WebSessionStatus::Expired),
        static_cast<int>(
            sessions.find(cookie, fermentation::kWebSessionIdleLimitMs)
                .status));
}

void test_repeated_touch_cannot_extend_absolute_session_limit() {
    Random random;
    fermentation::WebSessionManager sessions(random);
    const auto created = sessions.create(0U);
    TEST_ASSERT_TRUE(created.handle.has_value());
    constexpr auto touchIntervalMs = fermentation::kWebSessionIdleLimitMs - 1U;

    for (std::uint64_t nowMs = touchIntervalMs;
         nowMs < fermentation::kWebSessionAbsoluteLimitMs - 1U;
         nowMs += touchIntervalMs) {
        TEST_ASSERT_TRUE(sessions.touch(*created.handle, nowMs));
    }
    TEST_ASSERT_TRUE(sessions.touch(
        *created.handle, fermentation::kWebSessionAbsoluteLimitMs - 1U));
    TEST_ASSERT_FALSE(sessions.touch(*created.handle,
                                     fermentation::kWebSessionAbsoluteLimitMs));
}

void test_create_retires_all_expired_slots_before_capacity() {
    Random random;
    fermentation::WebSessionManager sessions(random);
    for (std::size_t i = 0U; i < fermentation::kMaximumWebSessions; ++i)
        TEST_ASSERT_TRUE(sessions.create(0U).handle.has_value());
    const auto replacement =
        sessions.create(fermentation::kWebSessionIdleLimitMs + 1U);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::WebSessionStatus::Created),
        static_cast<int>(replacement.status));
    TEST_ASSERT_TRUE(replacement.handle.has_value());
}

void test_service_lease_status_is_read_only_and_uses_its_policy() {
    Random random;
    fermentation::WebSessionManager sessions(random, {100U, 250U});
    const auto created = sessions.create(0U);
    TEST_ASSERT_TRUE(created.handle.has_value());
    TEST_ASSERT_TRUE(sessions.grantServiceLease(*created.handle, 0U));
    const auto initial = sessions.serviceLeaseStatus(*created.handle, 40U);
    TEST_ASSERT_TRUE(initial.active);
    TEST_ASSERT_FALSE(initial.remainingMillis.has_value());
    const auto later = sessions.serviceLeaseStatus(*created.handle, 99U);
    TEST_ASSERT_TRUE(later.active);
    TEST_ASSERT_FALSE(later.remainingMillis.has_value());
    TEST_ASSERT_FALSE(
        sessions.serviceLeaseStatus(*created.handle, 100U).active);
}

void test_cookie_parser_rejects_duplicate_session_cookie() {
    Random random;
    fermentation::WebSessionManager sessions(random);
    const auto created = sessions.create(0U);
    TEST_ASSERT_TRUE(created.handle.has_value());
    const auto duplicate =
        sessions.find("FSSESSION=" + created.cookieValue +
                          "; FSSESSION=" + created.cookieValue,
                      1U);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::WebSessionStatus::Missing),
        static_cast<int>(duplicate.status));
}

void test_request_digest_preserves_canonical_bounded_identity() {
    CapturingDigest digest;
    device_platform::HttpRequest first{
        "POST", "/internal/ui/run", std::string("a\0b", 3U), {}};
    device_platform::HttpRequest second{
        "POST", "/internal/ui/run", std::string("a\0c", 3U), {}};
    fermentation::ReplayDigest firstDigest{};
    fermentation::ReplayDigest secondDigest{};
    TEST_ASSERT_TRUE(fermentation::mutationDigest(first, digest, firstDigest));
    const auto firstStream = digest.stream;
    TEST_ASSERT_TRUE(
        fermentation::mutationDigest(second, digest, secondDigest));
    TEST_ASSERT_TRUE(firstDigest != secondDigest);
    TEST_ASSERT_EQUAL_UINT32(fermentation::kReplayDigestBytes,
                             firstDigest.size());
    auto differentMethod = first;
    differentMethod.method = "PUT";
    fermentation::ReplayDigest differentMethodDigest{};
    TEST_ASSERT_TRUE(fermentation::mutationDigest(differentMethod, digest,
                                                  differentMethodDigest));
    TEST_ASSERT_TRUE(firstDigest != differentMethodDigest);
    auto differentPath = first;
    differentPath.path = "/internal/ui/other";
    fermentation::ReplayDigest differentPathDigest{};
    TEST_ASSERT_TRUE(fermentation::mutationDigest(differentPath, digest,
                                                  differentPathDigest));
    TEST_ASSERT_TRUE(firstDigest != differentPathDigest);

    device_platform::HttpRequest boundaryA{"A", "BC", {}, {}};
    device_platform::HttpRequest boundaryB{"AB", "C", {}, {}};
    fermentation::ReplayDigest boundaryDigest{};
    TEST_ASSERT_TRUE(
        fermentation::mutationDigest(boundaryA, digest, boundaryDigest));
    const auto boundaryStreamA = digest.stream;
    TEST_ASSERT_TRUE(
        fermentation::mutationDigest(boundaryB, digest, boundaryDigest));
    TEST_ASSERT_TRUE(boundaryStreamA != digest.stream);
    TEST_ASSERT_TRUE(firstStream != boundaryStreamA);

    device_platform::HttpRequest atBodyLimit{
        "POST",
        "/",
        std::string(fermentation::kMaximumWebRunMutationBodyBytes, 'x'),
        {}};
    TEST_ASSERT_TRUE(
        fermentation::mutationDigest(atBodyLimit, digest, boundaryDigest));
    atBodyLimit.body.push_back('x');
    TEST_ASSERT_FALSE(
        fermentation::mutationDigest(atBodyLimit, digest, boundaryDigest));
    atBodyLimit.body.clear();
    atBodyLimit.method.assign(fermentation::kMaximumMutationMethodBytes + 1U,
                              'M');
    TEST_ASSERT_FALSE(
        fermentation::mutationDigest(atBodyLimit, digest, boundaryDigest));
    atBodyLimit.method = "POST";
    atBodyLimit.path.assign(fermentation::kMaximumMutationPathBytes + 1U, 'P');
    TEST_ASSERT_FALSE(
        fermentation::mutationDigest(atBodyLimit, digest, boundaryDigest));
}

void test_replay_cache_uses_compact_outcome_codes_and_full_window() {
    Random random;
    fermentation::WebSessionManager sessions(random);
    const auto created = sessions.create(0U);
    TEST_ASSERT_TRUE(created.handle.has_value());
    TEST_ASSERT_EQUAL_UINT32(1152U,
                             fermentation::kMaximumWebSessionReplayDigestBytes);
    TEST_ASSERT_TRUE(sizeof(sessions) <=
                     fermentation::kMaximumWebSessionManagerBytes);

    const auto digest = testDigest("compact-outcome");
    const auto reserved =
        sessions.reserveMutation(*created.handle, 1U, 1U, digest);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::MutationReservationStatus::Reserved),
        static_cast<int>(reserved.status));

    const fermentation::WebMutationOutcome unsupported{200U, "application/json",
                                                       "ok"};
    TEST_ASSERT_FALSE(sessions.completeMutation(*created.handle, 1U, 1U, digest,
                                                unsupported));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::MutationSequenceState::InFlight),
        static_cast<int>(sessions.mutationSequence(*created.handle, 1U).state));
    TEST_ASSERT_TRUE(sessions.completeMutation(
        *created.handle, 1U, 1U, digest,
        fermentation::replayOutcome(fermentation::ReplayOutcomeCode::Applied)));

    for (std::uint8_t value = 1U; value <= 8U; ++value) {
        const auto code = static_cast<fermentation::ReplayOutcomeCode>(value);
        const auto mutationDigest =
            testDigest("outcome-" + std::to_string(value));
        const auto next = sessions.reserveMutation(*created.handle, 1U + value,
                                                   1U + value, mutationDigest);
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(fermentation::MutationReservationStatus::Reserved),
            static_cast<int>(next.status));
        TEST_ASSERT_TRUE(sessions.completeMutation(
            *created.handle, 1U + value, 1U + value, mutationDigest,
            fermentation::replayOutcome(code)));
    }

    const auto expired =
        sessions.reserveMutation(*created.handle, 10U, 1U, digest);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            fermentation::MutationReservationStatus::ReplayExpired),
        static_cast<int>(expired.status));
    for (std::uint8_t value = 1U; value <= 8U; ++value) {
        const auto code = static_cast<fermentation::ReplayOutcomeCode>(value);
        const auto mutationDigest =
            testDigest("outcome-" + std::to_string(value));
        const auto replay = sessions.reserveMutation(
            *created.handle, 10U, 1U + value, mutationDigest);
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(
                fermentation::MutationReservationStatus::ReplayOutcome),
            static_cast<int>(replay.status));
        TEST_ASSERT_TRUE(replay.outcome.has_value());
        const auto expected = fermentation::replayOutcome(code);
        TEST_ASSERT_EQUAL_UINT16(expected.statusCode,
                                 replay.outcome->statusCode);
        TEST_ASSERT_EQUAL_STRING(expected.contentType.c_str(),
                                 replay.outcome->contentType.c_str());
        TEST_ASSERT_EQUAL_STRING(expected.body.c_str(),
                                 replay.outcome->body.c_str());
    }
}

void test_sequence_gap_and_uint64_overflow_fail_closed() {
    Random random;
    fermentation::WebSessionManager sessions(random);
    const auto created = sessions.create(0U);
    TEST_ASSERT_TRUE(created.handle.has_value());
    const auto gap = sessions.reserveMutation(*created.handle, 1U, 2U,
                                              testDigest("gap-payload"));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::MutationReservationStatus::SequenceGap),
        static_cast<int>(gap.status));

    const auto parsedMaximum = fermentation::parseMutationSequence(
        std::to_string(std::numeric_limits<std::uint64_t>::max()));
    TEST_ASSERT_TRUE(parsedMaximum.has_value());
    TEST_ASSERT_EQUAL_UINT64(std::numeric_limits<std::uint64_t>::max(),
                             *parsedMaximum);
    TEST_ASSERT_FALSE(
        fermentation::parseMutationSequence("18446744073709551616")
            .has_value());
    TEST_ASSERT_FALSE(fermentation::parseMutationSequence("0").has_value());

    TEST_ASSERT_TRUE(fermentation::WebSessionManagerTestAccess::setHighWater(
        sessions, *created.handle,
        std::numeric_limits<std::uint64_t>::max() - 1U));
    TEST_ASSERT_EQUAL_UINT64(
        std::numeric_limits<std::uint64_t>::max(),
        sessions.mutationSequence(*created.handle, 1U).nextMutationSeq);
    const auto boundaryReservation = sessions.reserveMutation(
        *created.handle, 1U, std::numeric_limits<std::uint64_t>::max(),
        testDigest("overflow-boundary"));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::MutationReservationStatus::Reserved),
        static_cast<int>(boundaryReservation.status));
    const fermentation::WebMutationOutcome outcome =
        fermentation::replayOutcome(fermentation::ReplayOutcomeCode::Applied);
    TEST_ASSERT_TRUE(sessions.completeMutation(
        *created.handle, 1U, std::numeric_limits<std::uint64_t>::max(),
        testDigest("overflow-boundary"), outcome));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::MutationSequenceState::Exhausted),
        static_cast<int>(sessions.mutationSequence(*created.handle, 1U).state));

    const auto replay = sessions.reserveMutation(
        *created.handle, 1U, std::numeric_limits<std::uint64_t>::max(),
        testDigest("overflow-boundary"));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            fermentation::MutationReservationStatus::ReplayOutcome),
        static_cast<int>(replay.status));
    const auto reused = sessions.reserveMutation(
        *created.handle, 1U, std::numeric_limits<std::uint64_t>::max(),
        testDigest("different-after-exhaustion"));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            fermentation::MutationReservationStatus::SequenceReused),
        static_cast<int>(reused.status));
    TEST_ASSERT_TRUE(fermentation::WebSessionManagerTestAccess::setHighWater(
        sessions, *created.handle, std::numeric_limits<std::uint64_t>::max(),
        true));
    const auto exhausted = sessions.reserveMutation(*created.handle, 1U, 1U,
                                                    testDigest("new-payload"));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::MutationReservationStatus::Exhausted),
        static_cast<int>(exhausted.status));
}

}  // namespace

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_more_than_eight_mutations_do_not_exhaust_session);
    RUN_TEST(test_replay_and_reuse_are_not_second_mutations);
    RUN_TEST(test_inflight_and_old_retired_values_fail_closed);
    RUN_TEST(test_session_expiry_and_capacity);
    RUN_TEST(test_repeated_find_does_not_extend_idle_deadline);
    RUN_TEST(test_touch_before_idle_expiry_extends_idle_deadline);
    RUN_TEST(test_browser_reload_lookup_does_not_extend_idle_deadline);
    RUN_TEST(test_repeated_touch_cannot_extend_absolute_session_limit);
    RUN_TEST(test_create_retires_all_expired_slots_before_capacity);
    RUN_TEST(test_service_lease_status_is_read_only_and_uses_its_policy);
    RUN_TEST(test_cookie_parser_rejects_duplicate_session_cookie);
    RUN_TEST(test_request_digest_preserves_canonical_bounded_identity);
    RUN_TEST(test_two_tabs_share_csrf_sequence_and_replay_outcome);
    RUN_TEST(test_browser_reload_preserves_live_session_csrf_and_replay_state);
    RUN_TEST(test_replay_cache_uses_compact_outcome_codes_and_full_window);
    RUN_TEST(test_sequence_gap_and_uint64_overflow_fail_closed);
    return UNITY_END();
}
