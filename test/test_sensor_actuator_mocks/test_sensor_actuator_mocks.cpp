#include <unity.h>

#include "mock_actuator_sink.hpp"
#include "mock_temperature_source.hpp"
#include "thermal_simulation_model.hpp"
#include "virtual_time_source.hpp"

namespace {

constexpr device_platform::ThermalSimulationConfig kTestConfig{
    /*ambientCelsius=*/20.0,
    /*heatingRateCelsiusPerSecond=*/0.1,
    /*coolingRateCelsiusPerSecond=*/0.2,
    /*idleDriftRateCelsiusPerSecond=*/0.05,
};

}  // namespace

void test_temperature_source_reports_configured_value() {
    device_platform::MockTemperatureSource sensor(4.0);

    const auto reading = sensor.read();

    TEST_ASSERT_TRUE(reading.available);
    TEST_ASSERT_EQUAL_DOUBLE(4.0, reading.celsius);
}

void test_temperature_source_can_change_value() {
    device_platform::MockTemperatureSource sensor(4.0);

    sensor.setCelsius(6.5);

    TEST_ASSERT_EQUAL_DOUBLE(6.5, sensor.read().celsius);
}

void test_temperature_source_fault_injection_marks_unavailable() {
    device_platform::MockTemperatureSource sensor(4.0);

    sensor.setAvailable(false);

    TEST_ASSERT_FALSE(sensor.read().available);
}

void test_actuator_sink_tracks_current_state() {
    device_platform::MockActuatorSink actuators;

    actuators.setHeating(true);
    actuators.setInsideFan(true);
    actuators.setBuzzer(true);

    TEST_ASSERT_TRUE(actuators.heating());
    TEST_ASSERT_FALSE(actuators.cooling());
    TEST_ASSERT_TRUE(actuators.insideFan());
    TEST_ASSERT_FALSE(actuators.outsideFan());
    TEST_ASSERT_TRUE(actuators.buzzer());
}

void test_actuator_sink_journals_every_command_in_order() {
    device_platform::MockActuatorSink actuators;

    actuators.setHeating(true);
    actuators.setOutsideFan(true);
    actuators.setHeating(false);

    const auto& journal = actuators.commandJournal();
    TEST_ASSERT_EQUAL_UINT32(3U, journal.size());
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(device_platform::ActuatorCommandKind::Heating),
        static_cast<int>(journal[0].kind));
    TEST_ASSERT_TRUE(journal[0].enabled);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(device_platform::ActuatorCommandKind::OutsideFan),
        static_cast<int>(journal[1].kind));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(device_platform::ActuatorCommandKind::Heating),
        static_cast<int>(journal[2].kind));
    TEST_ASSERT_FALSE(journal[2].enabled);
}

void test_actuator_sink_makes_simultaneous_directions_visible() {
    device_platform::MockActuatorSink actuators;

    TEST_ASSERT_FALSE(actuators.simultaneousDirectionsObserved());

    actuators.setHeating(true);
    TEST_ASSERT_FALSE(actuators.simultaneousDirectionsObserved());

    actuators.setCooling(true);
    TEST_ASSERT_TRUE(actuators.simultaneousDirectionsObserved());

    // Bleibt sichtbar, auch wenn danach nur noch eine Richtung aktiv ist.
    actuators.setHeating(false);
    TEST_ASSERT_TRUE(actuators.simultaneousDirectionsObserved());
}

void test_thermal_model_heats_deterministically() {
    device_platform::ThermalSimulationModel model(kTestConfig, 4.0);

    model.advance(10000, /*heating=*/true, /*cooling=*/false);

    TEST_ASSERT_EQUAL_DOUBLE(5.0, model.celsius());
}

void test_thermal_model_cools_deterministically() {
    device_platform::ThermalSimulationModel model(kTestConfig, 4.0);

    model.advance(10000, /*heating=*/false, /*cooling=*/true);

    TEST_ASSERT_EQUAL_DOUBLE(2.0, model.celsius());
}

void test_thermal_model_drifts_toward_ambient_when_idle() {
    device_platform::ThermalSimulationModel model(kTestConfig, 4.0);

    model.advance(20000, /*heating=*/false, /*cooling=*/false);

    TEST_ASSERT_EQUAL_DOUBLE(5.0, model.celsius());
}

void test_thermal_model_does_not_overshoot_ambient_while_drifting() {
    device_platform::ThermalSimulationModel model(kTestConfig, 19.99);

    model.advance(60000, /*heating=*/false, /*cooling=*/false);

    TEST_ASSERT_EQUAL_DOUBLE(20.0, model.celsius());
}

void test_thermal_model_ignores_simultaneous_heating_and_cooling() {
    device_platform::ThermalSimulationModel model(kTestConfig, 4.0);

    model.advance(10000, /*heating=*/true, /*cooling=*/true);

    TEST_ASSERT_EQUAL_DOUBLE(4.0, model.celsius());
}

void test_deterministic_heat_cool_cycle_driven_by_virtual_time() {
    device_platform::VirtualTimeSource timeSource;
    device_platform::MockActuatorSink actuators;
    device_platform::ThermalSimulationModel model(kTestConfig, 4.0);

    uint64_t lastUpdateMillis = timeSource.monotonicMillis();

    actuators.setHeating(true);
    timeSource.advanceMonotonicMillis(10000);
    model.advance(timeSource.monotonicMillis() - lastUpdateMillis,
                  actuators.heating(), actuators.cooling());
    lastUpdateMillis = timeSource.monotonicMillis();
    TEST_ASSERT_EQUAL_DOUBLE(5.0, model.celsius());

    actuators.setHeating(false);
    actuators.setCooling(true);
    timeSource.advanceMonotonicMillis(5000);
    model.advance(timeSource.monotonicMillis() - lastUpdateMillis,
                  actuators.heating(), actuators.cooling());
    lastUpdateMillis = timeSource.monotonicMillis();
    TEST_ASSERT_EQUAL_DOUBLE(4.0, model.celsius());
}

void test_power_loss_and_restart_reset_to_safe_defaults() {
    device_platform::VirtualTimeSource beforeRestart;
    device_platform::MockActuatorSink actuatorsBeforeRestart;
    actuatorsBeforeRestart.setHeating(true);
    actuatorsBeforeRestart.setInsideFan(true);
    beforeRestart.advanceMonotonicMillis(60000);
    beforeRestart.setUnixTimeSeconds(1700000000);

    // Ein Stromausfall/Neustart wird durch frische Instanzen nachgebildet:
    // kein Adapter darf ueberlebenden Aktorzustand aus dem vorherigen Lauf
    // erben.
    const device_platform::VirtualTimeSource afterRestart;
    const device_platform::MockActuatorSink actuatorsAfterRestart;

    TEST_ASSERT_EQUAL_UINT64(0U, afterRestart.monotonicMillis());
    TEST_ASSERT_FALSE(afterRestart.unixTimeSeconds().has_value());
    TEST_ASSERT_FALSE(actuatorsAfterRestart.heating());
    TEST_ASSERT_FALSE(actuatorsAfterRestart.cooling());
    TEST_ASSERT_FALSE(actuatorsAfterRestart.insideFan());
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_temperature_source_reports_configured_value);
    RUN_TEST(test_temperature_source_can_change_value);
    RUN_TEST(test_temperature_source_fault_injection_marks_unavailable);
    RUN_TEST(test_actuator_sink_tracks_current_state);
    RUN_TEST(test_actuator_sink_journals_every_command_in_order);
    RUN_TEST(test_actuator_sink_makes_simultaneous_directions_visible);
    RUN_TEST(test_thermal_model_heats_deterministically);
    RUN_TEST(test_thermal_model_cools_deterministically);
    RUN_TEST(test_thermal_model_drifts_toward_ambient_when_idle);
    RUN_TEST(test_thermal_model_does_not_overshoot_ambient_while_drifting);
    RUN_TEST(test_thermal_model_ignores_simultaneous_heating_and_cooling);
    RUN_TEST(test_deterministic_heat_cool_cycle_driven_by_virtual_time);
    RUN_TEST(test_power_loss_and_restart_reset_to_safe_defaults);
    return UNITY_END();
}
