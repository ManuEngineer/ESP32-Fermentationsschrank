#include <cstddef>
#include <vector>

#include <unity.h>

#include "shared_enable_bridge_sink.hpp"

namespace {

using device_platform::SharedEnableBridgeSink;

enum class Out : unsigned char { Forward, Reverse, Enable };

struct Event {
    Out out;
    bool value;
    bool accepted;
};

struct Log {
    std::vector<Event> events;
};

// Recording output with an injectable per-sink acceptance switch. A rejecting
// sink models a not-yet-initialised or faulted output (setEnabled -> false).
class RecordingOutput final : public device_platform::IBinaryOutputSink {
   public:
    RecordingOutput(Out out, Log& log) : out_(out), log_(log) {}

    [[nodiscard]] bool setEnabled(bool enabled) override {
        log_.events.push_back({out_, enabled, accepting});
        return accepting;
    }

    bool accepting{true};

   private:
    Out out_;
    Log& log_;
};

struct Fixture {
    Log log;
    RecordingOutput forward{Out::Forward, log};
    RecordingOutput reverse{Out::Reverse, log};
    RecordingOutput enable{Out::Enable, log};
    SharedEnableBridgeSink bridge{forward, reverse, enable};
};

void expectEvent(const Log& log, std::size_t index, Out out, bool value) {
    TEST_ASSERT_TRUE(index < log.events.size());
    TEST_ASSERT_TRUE(log.events[index].out == out);
    TEST_ASSERT_TRUE(log.events[index].value == value);
}

// Replays the accepted commands and proves the hard bridge invariants over the
// whole command sequence: both legs are never on together, a leg is only
// switched on while the shared enable is off, and a leg changes direction
// only through the all-off state.
void assertBridgeInvariants(const Log& log) {
    bool forward = false;
    bool reverse = false;
    bool enable = false;
    for (const auto& event : log.events) {
        if (!event.accepted) continue;
        switch (event.out) {
            case Out::Forward:
                if (event.value) TEST_ASSERT_FALSE(enable || reverse);
                forward = event.value;
                break;
            case Out::Reverse:
                if (event.value) TEST_ASSERT_FALSE(enable || forward);
                reverse = event.value;
                break;
            case Out::Enable:
                if (event.value) TEST_ASSERT_TRUE(forward != reverse);
                enable = event.value;
                break;
        }
        TEST_ASSERT_FALSE(forward && reverse);
    }
}

bool anyEnableOrLegOn(const Log& log, std::size_t from) {
    for (std::size_t i = from; i < log.events.size(); ++i) {
        if (log.events[i].value) return true;
    }
    return false;
}

void test_begin_ready_applies_all_off_in_order() {
    Fixture f;
    TEST_ASSERT_TRUE(f.bridge.begin());
    TEST_ASSERT_EQUAL_UINT(3U, f.log.events.size());
    expectEvent(f.log, 0, Out::Enable, false);
    expectEvent(f.log, 1, Out::Forward, false);
    expectEvent(f.log, 2, Out::Reverse, false);

    // A second begin() changes nothing.
    TEST_ASSERT_TRUE(f.bridge.begin());
    TEST_ASSERT_EQUAL_UINT(3U, f.log.events.size());
}

// An uninitialised output answers setEnabled(false) with false and must never
// count as a successful all-off initialisation (review blocker P1).
void begin_with_rejecting_output(Out rejecting) {
    Fixture f;
    (rejecting == Out::Forward   ? f.forward
     : rejecting == Out::Reverse ? f.reverse
                                 : f.enable)
        .accepting = false;
    TEST_ASSERT_FALSE(f.bridge.begin());
    // Initial all-off attempt plus the bounded shutdown attempt: every output
    // is tried exactly once per round, in enable/forward/reverse order.
    TEST_ASSERT_EQUAL_UINT(6U, f.log.events.size());
    for (std::size_t round = 0; round < 2; ++round) {
        expectEvent(f.log, round * 3 + 0, Out::Enable, false);
        expectEvent(f.log, round * 3 + 1, Out::Forward, false);
        expectEvent(f.log, round * 3 + 2, Out::Reverse, false);
    }
    // Latched: no later command may switch anything on.
    const auto before = f.log.events.size();
    f.bridge.setForward(true);
    f.bridge.setReverse(true);
    TEST_ASSERT_FALSE(anyEnableOrLegOn(f.log, before));
    TEST_ASSERT_FALSE(f.bridge.begin());
    assertBridgeInvariants(f.log);
}

void test_begin_with_uninitialised_enable_is_not_ready() {
    begin_with_rejecting_output(Out::Enable);
}

void test_begin_with_uninitialised_forward_is_not_ready() {
    begin_with_rejecting_output(Out::Forward);
}

void test_begin_with_uninitialised_reverse_is_not_ready() {
    begin_with_rejecting_output(Out::Reverse);
}

void test_calls_before_begin_are_discarded() {
    Fixture f;
    f.bridge.setForward(true);
    f.bridge.setReverse(true);
    f.bridge.setForward(false);
    f.bridge.setReverse(false);
    TEST_ASSERT_EQUAL_UINT(0U, f.log.events.size());
    // begin() still works afterwards.
    TEST_ASSERT_TRUE(f.bridge.begin());
}

void test_forward_enable_sequence_leg_first_enable_last() {
    Fixture f;
    TEST_ASSERT_TRUE(f.bridge.begin());
    f.log.events.clear();

    f.bridge.setForward(true);
    TEST_ASSERT_EQUAL_UINT(2U, f.log.events.size());
    expectEvent(f.log, 0, Out::Forward, true);
    expectEvent(f.log, 1, Out::Enable, true);

    f.bridge.setForward(true);  // idempotent
    TEST_ASSERT_EQUAL_UINT(2U, f.log.events.size());

    f.bridge.setForward(false);  // master gate first
    TEST_ASSERT_EQUAL_UINT(4U, f.log.events.size());
    expectEvent(f.log, 2, Out::Enable, false);
    expectEvent(f.log, 3, Out::Forward, false);

    f.bridge.setForward(false);  // already off: nothing
    TEST_ASSERT_EQUAL_UINT(4U, f.log.events.size());
    assertBridgeInvariants(f.log);
}

void test_reverse_enable_sequence_is_symmetric() {
    Fixture f;
    TEST_ASSERT_TRUE(f.bridge.begin());
    f.log.events.clear();

    f.bridge.setReverse(true);
    expectEvent(f.log, 0, Out::Reverse, true);
    expectEvent(f.log, 1, Out::Enable, true);
    f.bridge.setReverse(false);
    expectEvent(f.log, 2, Out::Enable, false);
    expectEvent(f.log, 3, Out::Reverse, false);
    TEST_ASSERT_EQUAL_UINT(4U, f.log.events.size());
    assertBridgeInvariants(f.log);
}

void test_contradictory_command_shuts_down_and_latches() {
    Fixture f;
    TEST_ASSERT_TRUE(f.bridge.begin());
    f.bridge.setForward(true);
    f.log.events.clear();

    f.bridge.setReverse(true);  // forward still on: conflict
    TEST_ASSERT_EQUAL_UINT(3U, f.log.events.size());
    expectEvent(f.log, 0, Out::Enable, false);
    expectEvent(f.log, 1, Out::Forward, false);
    expectEvent(f.log, 2, Out::Reverse, false);

    f.bridge.setForward(true);  // latched
    f.bridge.setReverse(true);
    TEST_ASSERT_EQUAL_UINT(3U, f.log.events.size());
    assertBridgeInvariants(f.log);
}

void test_contradictory_command_in_the_other_order_also_latches() {
    Fixture f;
    TEST_ASSERT_TRUE(f.bridge.begin());
    f.bridge.setReverse(true);
    f.log.events.clear();

    f.bridge.setForward(true);
    TEST_ASSERT_EQUAL_UINT(3U, f.log.events.size());
    f.bridge.setReverse(true);
    TEST_ASSERT_EQUAL_UINT(3U, f.log.events.size());
}

void test_direction_change_runs_only_through_all_off() {
    Fixture f;
    TEST_ASSERT_TRUE(f.bridge.begin());
    f.log.events.clear();

    f.bridge.setForward(true);
    // Immediately following counter command in the same breath, as a driver
    // would issue it: off first, then on. The reverse leg only goes on after
    // the enable and forward leg were successfully switched off.
    f.bridge.setForward(false);
    f.bridge.setReverse(true);
    TEST_ASSERT_EQUAL_UINT(6U, f.log.events.size());
    expectEvent(f.log, 0, Out::Forward, true);
    expectEvent(f.log, 1, Out::Enable, true);
    expectEvent(f.log, 2, Out::Enable, false);
    expectEvent(f.log, 3, Out::Forward, false);
    expectEvent(f.log, 4, Out::Reverse, true);
    expectEvent(f.log, 5, Out::Enable, true);
    assertBridgeInvariants(f.log);
}

void test_failed_off_before_direction_change_blocks_the_new_direction() {
    Fixture f;
    TEST_ASSERT_TRUE(f.bridge.begin());
    f.bridge.setForward(true);
    f.log.events.clear();

    f.forward.accepting = false;  // forward OFF cannot be applied
    f.bridge.setForward(false);   // -> shutdown + Faulted
    f.forward.accepting = true;
    const auto afterOff = f.log.events.size();
    f.bridge.setReverse(true);
    TEST_ASSERT_FALSE(anyEnableOrLegOn(f.log, afterOff));
}

// A write failure in any single command of an ON request shuts down and
// latches; nothing is switched on afterwards.
void enable_failure(bool forwardDirection, Out failing) {
    Fixture f;
    TEST_ASSERT_TRUE(f.bridge.begin());
    f.log.events.clear();
    (failing == Out::Enable ? f.enable
     : forwardDirection     ? f.forward
                            : f.reverse)
        .accepting = false;

    if (forwardDirection) {
        f.bridge.setForward(true);
    } else {
        f.bridge.setReverse(true);
    }
    // The shutdown round (enable, forward, reverse) always follows the failure.
    const auto size = f.log.events.size();
    TEST_ASSERT_TRUE(size >= 4U);
    expectEvent(f.log, size - 3, Out::Enable, false);
    expectEvent(f.log, size - 2, Out::Forward, false);
    expectEvent(f.log, size - 1, Out::Reverse, false);

    // Latched: a new ON is discarded even after the output recovers.
    f.forward.accepting = true;
    f.reverse.accepting = true;
    f.enable.accepting = true;
    const auto before = f.log.events.size();
    f.bridge.setForward(true);
    f.bridge.setReverse(true);
    TEST_ASSERT_FALSE(anyEnableOrLegOn(f.log, before));
}

void test_forward_leg_write_failure_on_enable_latches() {
    enable_failure(true, Out::Forward);
}

void test_forward_enable_write_failure_on_enable_latches() {
    enable_failure(true, Out::Enable);
}

void test_reverse_leg_write_failure_on_enable_latches() {
    enable_failure(false, Out::Reverse);
}

void test_reverse_enable_write_failure_on_enable_latches() {
    enable_failure(false, Out::Enable);
}

void disable_failure(bool forwardDirection, Out failing) {
    Fixture f;
    TEST_ASSERT_TRUE(f.bridge.begin());
    if (forwardDirection) {
        f.bridge.setForward(true);
    } else {
        f.bridge.setReverse(true);
    }
    f.log.events.clear();
    (failing == Out::Enable ? f.enable
     : forwardDirection     ? f.forward
                            : f.reverse)
        .accepting = false;

    if (forwardDirection) {
        f.bridge.setForward(false);
    } else {
        f.bridge.setReverse(false);
    }
    // Both OFF commands are attempted, then one bounded shutdown round.
    TEST_ASSERT_EQUAL_UINT(5U, f.log.events.size());
    expectEvent(f.log, 0, Out::Enable, false);
    expectEvent(f.log, 1, forwardDirection ? Out::Forward : Out::Reverse,
                false);
    expectEvent(f.log, 2, Out::Enable, false);
    expectEvent(f.log, 3, Out::Forward, false);
    expectEvent(f.log, 4, Out::Reverse, false);

    f.forward.accepting = true;
    f.reverse.accepting = true;
    f.enable.accepting = true;
    const auto before = f.log.events.size();
    f.bridge.setForward(true);
    f.bridge.setReverse(true);
    TEST_ASSERT_FALSE(anyEnableOrLegOn(f.log, before));
}

void test_forward_off_failure_of_the_leg_latches() {
    disable_failure(true, Out::Forward);
}

void test_forward_off_failure_of_the_enable_latches() {
    disable_failure(true, Out::Enable);
}

void test_reverse_off_failure_of_the_leg_latches() {
    disable_failure(false, Out::Reverse);
}

void test_reverse_off_failure_of_the_enable_latches() {
    disable_failure(false, Out::Enable);
}

void test_off_in_faulted_state_is_best_effort_without_unlatching() {
    Fixture f;
    TEST_ASSERT_TRUE(f.bridge.begin());
    f.bridge.setForward(true);
    f.bridge.setReverse(true);  // conflict -> Faulted
    f.log.events.clear();

    f.bridge.setForward(false);
    TEST_ASSERT_EQUAL_UINT(3U, f.log.events.size());
    expectEvent(f.log, 0, Out::Enable, false);
    expectEvent(f.log, 1, Out::Forward, false);
    expectEvent(f.log, 2, Out::Reverse, false);
    TEST_ASSERT_FALSE(anyEnableOrLegOn(f.log, 0));

    const auto before = f.log.events.size();
    f.bridge.setForward(true);
    f.bridge.setReverse(true);
    TEST_ASSERT_FALSE(anyEnableOrLegOn(f.log, before));
    TEST_ASSERT_FALSE(f.bridge.begin());
}

void test_shutdown_attempts_every_output_even_when_all_reject() {
    Fixture f;
    TEST_ASSERT_TRUE(f.bridge.begin());
    f.bridge.setForward(true);
    f.log.events.clear();

    f.forward.accepting = false;
    f.reverse.accepting = false;
    f.enable.accepting = false;
    f.bridge.setReverse(true);  // conflict -> shutdown with rejecting outputs
    TEST_ASSERT_EQUAL_UINT(3U, f.log.events.size());
    expectEvent(f.log, 0, Out::Enable, false);
    expectEvent(f.log, 1, Out::Forward, false);
    expectEvent(f.log, 2, Out::Reverse, false);
}

void test_whole_session_keeps_the_bridge_invariants() {
    Fixture f;
    TEST_ASSERT_TRUE(f.bridge.begin());
    f.bridge.setForward(true);
    f.bridge.setForward(false);
    f.bridge.setReverse(true);
    f.bridge.setReverse(false);
    f.bridge.setForward(true);
    f.bridge.setForward(false);
    assertBridgeInvariants(f.log);
}

}  // namespace

int main(int argc, char** argv) {
    static_cast<void>(argc);
    static_cast<void>(argv);
    UNITY_BEGIN();
    RUN_TEST(test_begin_ready_applies_all_off_in_order);
    RUN_TEST(test_begin_with_uninitialised_enable_is_not_ready);
    RUN_TEST(test_begin_with_uninitialised_forward_is_not_ready);
    RUN_TEST(test_begin_with_uninitialised_reverse_is_not_ready);
    RUN_TEST(test_calls_before_begin_are_discarded);
    RUN_TEST(test_forward_enable_sequence_leg_first_enable_last);
    RUN_TEST(test_reverse_enable_sequence_is_symmetric);
    RUN_TEST(test_contradictory_command_shuts_down_and_latches);
    RUN_TEST(test_contradictory_command_in_the_other_order_also_latches);
    RUN_TEST(test_direction_change_runs_only_through_all_off);
    RUN_TEST(test_failed_off_before_direction_change_blocks_the_new_direction);
    RUN_TEST(test_forward_leg_write_failure_on_enable_latches);
    RUN_TEST(test_forward_enable_write_failure_on_enable_latches);
    RUN_TEST(test_reverse_leg_write_failure_on_enable_latches);
    RUN_TEST(test_reverse_enable_write_failure_on_enable_latches);
    RUN_TEST(test_forward_off_failure_of_the_leg_latches);
    RUN_TEST(test_forward_off_failure_of_the_enable_latches);
    RUN_TEST(test_reverse_off_failure_of_the_leg_latches);
    RUN_TEST(test_reverse_off_failure_of_the_enable_latches);
    RUN_TEST(test_off_in_faulted_state_is_best_effort_without_unlatching);
    RUN_TEST(test_shutdown_attempts_every_output_even_when_all_reject);
    RUN_TEST(test_whole_session_keeps_the_bridge_invariants);
    return UNITY_END();
}
