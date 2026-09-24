#include <unity.h>

#include <cstdint>

#include "raw_touch_recovery_detector.hpp"

namespace {

using device_platform::RawTouchSample;
using device_platform::RawTouchSampleStatus;

constexpr std::uint16_t kMinimumStrength = 40U;
constexpr std::uint64_t kRequiredHoldMicros = 10'000'000ULL;  // 10 s

RawTouchSample heldSample(std::uint64_t monotonicTimeUs,
                          std::uint16_t strength = 100U) {
    RawTouchSample sample;
    sample.status = RawTouchSampleStatus::Contact;
    sample.contact = true;
    sample.strength = strength;
    sample.monotonicTimeUs = monotonicTimeUs;
    return sample;
}

RawTouchSample releasedSample(std::uint64_t monotonicTimeUs) {
    RawTouchSample sample;
    sample.status = RawTouchSampleStatus::NoContact;
    sample.contact = false;
    sample.strength = 0U;
    sample.monotonicTimeUs = monotonicTimeUs;
    return sample;
}

void test_triggers_exactly_at_required_hold_duration() {
    fermentation::RawTouchRecoveryDetector detector(kMinimumStrength,
                                                    kRequiredHoldMicros);
    TEST_ASSERT_FALSE(detector.observe(heldSample(0U)));
    TEST_ASSERT_TRUE(detector.holding());
    TEST_ASSERT_FALSE(detector.observe(heldSample(kRequiredHoldMicros - 1U)));
    TEST_ASSERT_TRUE(detector.observe(heldSample(kRequiredHoldMicros)));
}

void test_does_not_trigger_below_required_duration() {
    fermentation::RawTouchRecoveryDetector detector(kMinimumStrength,
                                                    kRequiredHoldMicros);
    TEST_ASSERT_FALSE(detector.observe(heldSample(0U)));
    TEST_ASSERT_FALSE(detector.observe(heldSample(kRequiredHoldMicros / 2U)));
}

void test_release_before_threshold_gives_no_partial_credit() {
    fermentation::RawTouchRecoveryDetector detector(kMinimumStrength,
                                                    kRequiredHoldMicros);
    TEST_ASSERT_FALSE(detector.observe(heldSample(0U)));
    TEST_ASSERT_FALSE(detector.observe(heldSample(kRequiredHoldMicros / 2U)));
    TEST_ASSERT_FALSE(
        detector.observe(releasedSample(kRequiredHoldMicros / 2U + 1U)));
    TEST_ASSERT_FALSE(detector.holding());
    // A fresh hold starting right after release needs the full duration
    // again, not just the remainder.
    TEST_ASSERT_FALSE(
        detector.observe(heldSample(kRequiredHoldMicros / 2U + 2U)));
    TEST_ASSERT_FALSE(detector.observe(
        heldSample(kRequiredHoldMicros + kRequiredHoldMicros / 2U)));
}

void test_below_minimum_strength_does_not_count_as_held() {
    fermentation::RawTouchRecoveryDetector detector(kMinimumStrength,
                                                    kRequiredHoldMicros);
    TEST_ASSERT_FALSE(detector.observe(heldSample(0U, kMinimumStrength - 1U)));
    TEST_ASSERT_FALSE(detector.holding());
}

void test_controller_error_does_not_count_as_held() {
    fermentation::RawTouchRecoveryDetector detector(kMinimumStrength,
                                                    kRequiredHoldMicros);
    RawTouchSample sample = heldSample(0U);
    sample.status = RawTouchSampleStatus::ControllerError;
    TEST_ASSERT_FALSE(detector.observe(sample));
    TEST_ASSERT_FALSE(detector.holding());
}

void test_no_retrigger_while_still_held_after_first_trigger() {
    fermentation::RawTouchRecoveryDetector detector(kMinimumStrength,
                                                    kRequiredHoldMicros);
    static_cast<void>(detector.observe(heldSample(0U)));
    TEST_ASSERT_TRUE(detector.observe(heldSample(kRequiredHoldMicros)));
    TEST_ASSERT_FALSE(
        detector.observe(heldSample(kRequiredHoldMicros + 1'000'000ULL)));
}

void test_reset_allows_a_new_detection_cycle() {
    fermentation::RawTouchRecoveryDetector detector(kMinimumStrength,
                                                    kRequiredHoldMicros);
    static_cast<void>(detector.observe(heldSample(0U)));
    TEST_ASSERT_TRUE(detector.observe(heldSample(kRequiredHoldMicros)));
    detector.reset();
    TEST_ASSERT_FALSE(detector.holding());
    TEST_ASSERT_FALSE(detector.observe(heldSample(0U)));
    TEST_ASSERT_TRUE(detector.observe(heldSample(kRequiredHoldMicros)));
}

void test_backwards_monotonic_time_restarts_hold_instead_of_faulting() {
    fermentation::RawTouchRecoveryDetector detector(kMinimumStrength,
                                                    kRequiredHoldMicros);
    TEST_ASSERT_FALSE(detector.observe(heldSample(1'000'000ULL)));
    // A regression in the monotonic source must not be interpreted as an
    // already-elapsed duration.
    TEST_ASSERT_FALSE(detector.observe(heldSample(500'000ULL)));
    TEST_ASSERT_FALSE(
        detector.observe(heldSample(500'000ULL + kRequiredHoldMicros - 1U)));
    TEST_ASSERT_TRUE(
        detector.observe(heldSample(500'000ULL + kRequiredHoldMicros)));
}

}  // namespace

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_triggers_exactly_at_required_hold_duration);
    RUN_TEST(test_does_not_trigger_below_required_duration);
    RUN_TEST(test_release_before_threshold_gives_no_partial_credit);
    RUN_TEST(test_below_minimum_strength_does_not_count_as_held);
    RUN_TEST(test_controller_error_does_not_count_as_held);
    RUN_TEST(test_no_retrigger_while_still_held_after_first_trigger);
    RUN_TEST(test_reset_allows_a_new_detection_cycle);
    RUN_TEST(test_backwards_monotonic_time_restarts_hold_instead_of_faulting);
    return UNITY_END();
}
