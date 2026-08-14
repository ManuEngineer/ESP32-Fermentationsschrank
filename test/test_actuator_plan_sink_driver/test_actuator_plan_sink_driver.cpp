#include <cstdint>
#include <vector>

#include <unity.h>

#include "actuator_plan_sink_driver.hpp"
#include "mock_bidirectional_actuator_sink.hpp"
#include "mock_binary_output_sink.hpp"

namespace {

using namespace fermentation;
using device_platform_test_support::MockBidirectionalActuatorSink;
using device_platform_test_support::MockBinaryOutputSink;

struct SharedActuatorCallTrace {
    enum class Sink : std::uint8_t { Peltier, OuterFan, InnerFan };
    struct Entry {
        Sink sink;
        std::uint8_t call;
        bool value;
    };
    std::vector<Entry> entries;
};

class TracingBidirectionalSink final
    : public device_platform::IBidirectionalActuatorSink {
   public:
    TracingBidirectionalSink(MockBidirectionalActuatorSink& inner,
                             SharedActuatorCallTrace& trace)
        : inner_(inner), trace_(trace) {}

    void setForward(bool enabled) override {
        trace_.entries.push_back(
            {SharedActuatorCallTrace::Sink::Peltier, 0U, enabled});
        inner_.setForward(enabled);
    }

    void setReverse(bool enabled) override {
        trace_.entries.push_back(
            {SharedActuatorCallTrace::Sink::Peltier, 1U, enabled});
        inner_.setReverse(enabled);
    }

   private:
    MockBidirectionalActuatorSink& inner_;
    SharedActuatorCallTrace& trace_;
};

class TracingBinarySink final : public device_platform::IBinaryOutputSink {
   public:
    TracingBinarySink(MockBinaryOutputSink& inner,
                      SharedActuatorCallTrace::Sink sink,
                      SharedActuatorCallTrace& trace)
        : inner_(inner), sink_(sink), trace_(trace) {}

    void setEnabled(bool enabled) override {
        trace_.entries.push_back({sink_, 0U, enabled});
        inner_.setEnabled(enabled);
    }

   private:
    MockBinaryOutputSink& inner_;
    SharedActuatorCallTrace::Sink sink_;
    SharedActuatorCallTrace& trace_;
};

ActuatorPlanTickResult result(AbstractControlDirection direction, bool outer,
                              bool inner) {
    ActuatorPlanTickResult value;
    value.appliedDirection = direction;
    value.outerFanEnabled = outer;
    value.innerFanEnabled = inner;
    return value;
}

void assertTraceEntry(const SharedActuatorCallTrace::Entry& entry,
                      SharedActuatorCallTrace::Sink sink, std::uint8_t call,
                      bool value) {
    TEST_ASSERT_TRUE(entry.sink == sink);
    TEST_ASSERT_EQUAL_UINT8(call, entry.call);
    TEST_ASSERT_TRUE(entry.value == value);
}

// Replay only the Peltier entries and prove that the shared trace contains a
// real all-off state between active directions. The underlying mock remains
// the source of truth for the no-simultaneous-activation assertion.
bool traceContainsPeltierOffBetweenActiveDirections(
    const SharedActuatorCallTrace& trace) {
    bool forward = false;
    bool reverse = false;
    bool hadHeating = false;
    bool hadCooling = false;
    bool offBetween = false;
    for (const auto& entry : trace.entries) {
        if (entry.sink != SharedActuatorCallTrace::Sink::Peltier) continue;
        if (entry.call == 0U) {
            forward = entry.value;
        } else {
            reverse = entry.value;
        }
        if (hadHeating && !forward && !reverse) offBetween = true;
        if (forward) hadHeating = true;
        if (reverse) hadCooling = true;
    }
    return hadHeating && hadCooling && offBetween;
}

void test_driver_cross_sink_trace_proves_global_safety_order() {
    MockBidirectionalActuatorSink peltier;
    MockBinaryOutputSink outer;
    MockBinaryOutputSink inner;
    SharedActuatorCallTrace trace;
    TracingBidirectionalSink tracedPeltier(peltier, trace);
    TracingBinarySink tracedOuter(
        outer, SharedActuatorCallTrace::Sink::OuterFan, trace);
    TracingBinarySink tracedInner(
        inner, SharedActuatorCallTrace::Sink::InnerFan, trace);
    ActuatorPlanSinkDriver driver(tracedPeltier, tracedOuter, tracedInner);

    driver.apply(result(AbstractControlDirection::Heating, true, true));

    TEST_ASSERT_EQUAL_UINT(4U, trace.entries.size());
    assertTraceEntry(trace.entries[0], SharedActuatorCallTrace::Sink::Peltier,
                     1U, false);
    assertTraceEntry(trace.entries[1], SharedActuatorCallTrace::Sink::OuterFan,
                     0U, true);
    assertTraceEntry(trace.entries[2], SharedActuatorCallTrace::Sink::Peltier,
                     0U, true);
    assertTraceEntry(trace.entries[3], SharedActuatorCallTrace::Sink::InnerFan,
                     0U, true);

    const auto coolingStart = trace.entries.size();
    driver.apply(result(AbstractControlDirection::Cooling, true, true));
    TEST_ASSERT_EQUAL_UINT(8U, trace.entries.size());
    assertTraceEntry(trace.entries[coolingStart + 0U],
                     SharedActuatorCallTrace::Sink::Peltier, 0U, false);
    assertTraceEntry(trace.entries[coolingStart + 1U],
                     SharedActuatorCallTrace::Sink::OuterFan, 0U, true);
    assertTraceEntry(trace.entries[coolingStart + 2U],
                     SharedActuatorCallTrace::Sink::Peltier, 1U, true);
    assertTraceEntry(trace.entries[coolingStart + 3U],
                     SharedActuatorCallTrace::Sink::InnerFan, 0U, true);
    TEST_ASSERT_TRUE(traceContainsPeltierOffBetweenActiveDirections(trace));
    driver.apply(result(AbstractControlDirection::Idle, false, false));
    TEST_ASSERT_EQUAL_UINT(12U, trace.entries.size());
    assertTraceEntry(trace.entries[8], SharedActuatorCallTrace::Sink::Peltier,
                     0U, false);
    assertTraceEntry(trace.entries[9], SharedActuatorCallTrace::Sink::Peltier,
                     1U, false);
    assertTraceEntry(trace.entries[10], SharedActuatorCallTrace::Sink::OuterFan,
                     0U, false);
    assertTraceEntry(trace.entries[11], SharedActuatorCallTrace::Sink::InnerFan,
                     0U, false);

    TEST_ASSERT_FALSE(peltier.simultaneousActivationObserved());
    TEST_ASSERT_FALSE(peltier.forward());
    TEST_ASSERT_FALSE(peltier.reverse());
    TEST_ASSERT_FALSE(outer.enabled());
    TEST_ASSERT_FALSE(inner.enabled());
}

void test_driver_fails_closed_for_unknown_direction() {
    MockBidirectionalActuatorSink peltier;
    MockBinaryOutputSink outer;
    MockBinaryOutputSink inner;
    ActuatorPlanSinkDriver driver(peltier, outer, inner);
    driver.apply(result(AbstractControlDirection::Unknown, true, true));
    TEST_ASSERT_FALSE(peltier.forward());
    TEST_ASSERT_FALSE(peltier.reverse());
    TEST_ASSERT_FALSE(outer.enabled());
    TEST_ASSERT_FALSE(inner.enabled());
}

// Owner-Review F2: a structurally corrupt appliedDirection value (e.g. an
// invalid cast bypassing the planner's own structural validation) has no
// matching switch case and must not leave a previously applied H-bridge
// state standing.
void test_driver_fails_closed_for_corrupted_direction_after_heating() {
    MockBidirectionalActuatorSink peltier;
    MockBinaryOutputSink outer;
    MockBinaryOutputSink inner;
    ActuatorPlanSinkDriver driver(peltier, outer, inner);

    driver.apply(result(AbstractControlDirection::Heating, true, true));
    TEST_ASSERT_TRUE(peltier.forward());

    ActuatorPlanTickResult corrupted;
    corrupted.appliedDirection = static_cast<AbstractControlDirection>(0xFF);
    corrupted.outerFanEnabled = true;
    corrupted.innerFanEnabled = true;
    driver.apply(corrupted);

    TEST_ASSERT_FALSE(peltier.forward());
    TEST_ASSERT_FALSE(peltier.reverse());
    TEST_ASSERT_FALSE(outer.enabled());
    TEST_ASSERT_FALSE(inner.enabled());
    TEST_ASSERT_FALSE(peltier.simultaneousActivationObserved());
}

void test_driver_fails_closed_for_corrupted_direction_after_cooling() {
    MockBidirectionalActuatorSink peltier;
    MockBinaryOutputSink outer;
    MockBinaryOutputSink inner;
    ActuatorPlanSinkDriver driver(peltier, outer, inner);

    driver.apply(result(AbstractControlDirection::Cooling, true, true));
    TEST_ASSERT_TRUE(peltier.reverse());

    ActuatorPlanTickResult corrupted;
    corrupted.appliedDirection = static_cast<AbstractControlDirection>(0xFF);
    corrupted.outerFanEnabled = true;
    corrupted.innerFanEnabled = true;
    driver.apply(corrupted);

    TEST_ASSERT_FALSE(peltier.forward());
    TEST_ASSERT_FALSE(peltier.reverse());
    TEST_ASSERT_FALSE(outer.enabled());
    TEST_ASSERT_FALSE(inner.enabled());
    TEST_ASSERT_FALSE(peltier.simultaneousActivationObserved());
}

// Owner-Review F3: Heating/Cooling must unconditionally enable the outer fan
// before releasing Peltier power, never trusting result.outerFanEnabled -
// the planner cannot itself produce this inconsistent combination
// (updateFanState() always forces outerFanActive true while
// lastAppliedDirection != Idle), so the driver is the only remaining
// safety net against a malformed result.
void test_driver_forces_outer_fan_for_heating_and_cooling_regardless_of_result() {
    MockBidirectionalActuatorSink peltier;
    MockBinaryOutputSink outer;
    MockBinaryOutputSink inner;
    ActuatorPlanSinkDriver driver(peltier, outer, inner);

    driver.apply(result(AbstractControlDirection::Heating, false, true));
    TEST_ASSERT_TRUE(outer.enabled());
    TEST_ASSERT_TRUE(peltier.forward());

    driver.apply(result(AbstractControlDirection::Idle, false, true));
    driver.apply(result(AbstractControlDirection::Cooling, false, true));
    TEST_ASSERT_TRUE(outer.enabled());
    TEST_ASSERT_TRUE(peltier.reverse());
    TEST_ASSERT_FALSE(peltier.simultaneousActivationObserved());
}

}  // namespace

int main(int argc, char** argv) {
    static_cast<void>(argc);
    static_cast<void>(argv);
    UNITY_BEGIN();
    RUN_TEST(test_driver_cross_sink_trace_proves_global_safety_order);
    RUN_TEST(test_driver_fails_closed_for_unknown_direction);
    RUN_TEST(test_driver_fails_closed_for_corrupted_direction_after_heating);
    RUN_TEST(test_driver_fails_closed_for_corrupted_direction_after_cooling);
    RUN_TEST(
        test_driver_forces_outer_fan_for_heating_and_cooling_regardless_of_result);
    return UNITY_END();
}
