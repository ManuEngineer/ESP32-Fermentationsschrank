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

void test_driver_orders_heating_enable_before_hbridge_direction() {
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
    TEST_ASSERT_TRUE(trace.entries[0].sink ==
                     SharedActuatorCallTrace::Sink::Peltier);
    TEST_ASSERT_EQUAL_UINT8(1U, trace.entries[0].call);
    TEST_ASSERT_FALSE(trace.entries[0].value);
    TEST_ASSERT_TRUE(trace.entries[1].sink ==
                     SharedActuatorCallTrace::Sink::OuterFan);
    TEST_ASSERT_TRUE(trace.entries[1].value);
    TEST_ASSERT_TRUE(trace.entries[2].sink ==
                     SharedActuatorCallTrace::Sink::Peltier);
    TEST_ASSERT_EQUAL_UINT8(0U, trace.entries[2].call);
    TEST_ASSERT_TRUE(trace.entries[2].value);
    TEST_ASSERT_TRUE(inner.enabled());
    TEST_ASSERT_FALSE(peltier.simultaneousActivationObserved());
}

void test_driver_orders_cooling_and_forces_off_between_directions() {
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

    driver.apply(result(AbstractControlDirection::Cooling, true, true));
    TEST_ASSERT_EQUAL_UINT8(0U, trace.entries[0].call);
    TEST_ASSERT_FALSE(trace.entries[0].value);
    TEST_ASSERT_TRUE(trace.entries[1].sink ==
                     SharedActuatorCallTrace::Sink::OuterFan);
    TEST_ASSERT_TRUE(trace.entries[2].sink ==
                     SharedActuatorCallTrace::Sink::Peltier);
    TEST_ASSERT_EQUAL_UINT8(1U, trace.entries[2].call);
    TEST_ASSERT_TRUE(trace.entries[2].value);

    driver.apply(result(AbstractControlDirection::Idle, false, false));
    driver.apply(result(AbstractControlDirection::Heating, true, true));
    TEST_ASSERT_FALSE(peltier.simultaneousActivationObserved());
    TEST_ASSERT_TRUE(peltier.forward());
    TEST_ASSERT_FALSE(peltier.reverse());
    TEST_ASSERT_TRUE(outer.enabled());
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
    RUN_TEST(test_driver_orders_heating_enable_before_hbridge_direction);
    RUN_TEST(test_driver_orders_cooling_and_forces_off_between_directions);
    RUN_TEST(test_driver_fails_closed_for_unknown_direction);
    RUN_TEST(test_driver_fails_closed_for_corrupted_direction_after_heating);
    RUN_TEST(test_driver_fails_closed_for_corrupted_direction_after_cooling);
    RUN_TEST(
        test_driver_forces_outer_fan_for_heating_and_cooling_regardless_of_result);
    return UNITY_END();
}
