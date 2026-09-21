#include <unity.h>

#include <cstdint>

#include "web_session.hpp"

namespace {

class Random final : public device_platform::ISecureRandomSource {
   public:
    bool fill(void* buffer, std::size_t length) override {
        auto* bytes = static_cast<std::uint8_t*>(buffer);
        for (std::size_t i = 0U; i < length; ++i) bytes[i] = next_++;
        return buffer != nullptr || length == 0U;
    }

   private:
    std::uint8_t next_{1U};
};

void test_more_than_eight_mutations_do_not_exhaust_session() {
    Random random;
    fermentation::WebSessionManager sessions(random);
    const auto created = sessions.create(0U);
    TEST_ASSERT_TRUE(created.handle.has_value());
    for (std::uint64_t sequence = 1U; sequence <= 20U; ++sequence) {
        const auto reserved = sessions.reserveMutation(
            *created.handle, sequence, sequence, "fingerprint-" + std::to_string(sequence));
        TEST_ASSERT_EQUAL_INT(static_cast<int>(fermentation::MutationReservationStatus::Reserved),
                              static_cast<int>(reserved.status));
        TEST_ASSERT_TRUE(sessions.completeMutation(
            *created.handle, sequence, sequence, "fingerprint-" + std::to_string(sequence),
            {200U, "application/json", "{\"ok\":true}"}));
    }
    const auto view = sessions.mutationSequence(*created.handle, 1U);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(fermentation::MutationSequenceState::Available),
                          static_cast<int>(view.state));
    TEST_ASSERT_EQUAL_UINT64(21U, view.nextMutationSeq);
}

void test_replay_and_reuse_are_not_second_mutations() {
    Random random;
    fermentation::WebSessionManager sessions(random);
    const auto created = sessions.create(0U);
    TEST_ASSERT_TRUE(created.handle.has_value());
    const auto first = sessions.reserveMutation(*created.handle, 1U, 1U, "same");
    TEST_ASSERT_EQUAL_INT(static_cast<int>(fermentation::MutationReservationStatus::Reserved),
                          static_cast<int>(first.status));
    TEST_ASSERT_TRUE(sessions.completeMutation(*created.handle, 1U, 1U, "same",
                                               {200U, "application/json", "ok"}));
    const auto replay = sessions.reserveMutation(*created.handle, 2U, 1U, "same");
    TEST_ASSERT_EQUAL_INT(static_cast<int>(fermentation::MutationReservationStatus::ReplayOutcome),
                          static_cast<int>(replay.status));
    const auto reused = sessions.reserveMutation(*created.handle, 2U, 1U, "other");
    TEST_ASSERT_EQUAL_INT(static_cast<int>(fermentation::MutationReservationStatus::SequenceReused),
                          static_cast<int>(reused.status));
}

void test_inflight_and_old_retired_values_fail_closed() {
    Random random;
    fermentation::WebSessionManager sessions(random);
    const auto created = sessions.create(0U);
    TEST_ASSERT_TRUE(created.handle.has_value());
    const auto inFlight = sessions.reserveMutation(*created.handle, 0U, 1U, "a");
    TEST_ASSERT_EQUAL_INT(static_cast<int>(fermentation::MutationReservationStatus::Reserved),
                          static_cast<int>(inFlight.status));
    const auto loser = sessions.reserveMutation(*created.handle, 0U, 2U, "b");
    TEST_ASSERT_EQUAL_INT(static_cast<int>(fermentation::MutationReservationStatus::SequenceConflict),
                          static_cast<int>(loser.status));
    TEST_ASSERT_TRUE(sessions.completeMutation(*created.handle, 0U, 1U, "a",
                                               {200U, "application/json", "ok"}));
    for (std::uint64_t sequence = 2U; sequence <= 10U; ++sequence) {
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(fermentation::MutationReservationStatus::Reserved),
            static_cast<int>(sessions.reserveMutation(*created.handle, 0U, sequence,
                                                       "x" + std::to_string(sequence)).status));
        TEST_ASSERT_TRUE(sessions.completeMutation(
            *created.handle, 0U, sequence, "x" + std::to_string(sequence),
            {200U, "application/json", "ok"}));
    }
    const auto expired = sessions.reserveMutation(*created.handle, 0U, 1U, "same");
    TEST_ASSERT_EQUAL_INT(static_cast<int>(fermentation::MutationReservationStatus::ReplayExpired),
                          static_cast<int>(expired.status));
}

void test_session_expiry_and_capacity() {
    Random random;
    fermentation::WebSessionManager sessions(random);
    auto first = sessions.create(0U);
    TEST_ASSERT_TRUE(first.handle.has_value());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(fermentation::WebSessionStatus::Found),
                          static_cast<int>(sessions.find("FSSESSION=" + first.cookieValue, 1U).status));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(fermentation::WebSessionStatus::Expired),
                          static_cast<int>(sessions.find("FSSESSION=" + first.cookieValue,
                                                          fermentation::kWebSessionIdleLimitMs + 1U)
                                                .status));
    for (std::size_t i = 0U; i < fermentation::kMaximumWebSessions; ++i)
        TEST_ASSERT_TRUE(sessions.create(1U + i).handle.has_value());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(fermentation::WebSessionStatus::Capacity),
                          static_cast<int>(sessions.create(99U).status));
}

void test_create_retires_all_expired_slots_before_capacity() {
    Random random;
    fermentation::WebSessionManager sessions(random);
    for (std::size_t i = 0U; i < fermentation::kMaximumWebSessions; ++i)
        TEST_ASSERT_TRUE(sessions.create(0U).handle.has_value());
    const auto replacement = sessions.create(
        fermentation::kWebSessionIdleLimitMs + 1U);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(fermentation::WebSessionStatus::Created),
                          static_cast<int>(replacement.status));
    TEST_ASSERT_TRUE(replacement.handle.has_value());
}

}  // namespace

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_more_than_eight_mutations_do_not_exhaust_session);
    RUN_TEST(test_replay_and_reuse_are_not_second_mutations);
    RUN_TEST(test_inflight_and_old_retired_values_fail_closed);
    RUN_TEST(test_session_expiry_and_capacity);
    RUN_TEST(test_create_retires_all_expired_slots_before_capacity);
    return UNITY_END();
}
