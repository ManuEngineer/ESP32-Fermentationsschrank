#include <unity.h>

#include "mock_bidirectional_actuator_sink.hpp"
#include "mock_binary_output_sink.hpp"
#include "mock_temperature_source.hpp"
#include "thermal_simulation_model.hpp"
#include "virtual_time_source.hpp"

namespace {

constexpr device_platform_test_support::ThermalSimulationConfig kTestConfig{
    /*ambientCelsius=*/20.0,
    /*heatingRateCelsiusPerSecond=*/0.1,
    /*coolingRateCelsiusPerSecond=*/0.2,
    /*idleDriftRateCelsiusPerSecond=*/0.05,
};

}  // namespace

void test_temperature_source_reports_configured_value() {
    device_platform_test_support::MockTemperatureSource sensor(
        /*identity=*/std::nullopt, /*monotonicTimestampMs=*/0U,
        /*celsius=*/4.0);

    const auto reading = sensor.read();

    TEST_ASSERT_TRUE(reading.status() ==
                      device_platform::TemperatureSampleStatus::Ok);
    TEST_ASSERT_TRUE(reading.celsius().has_value());
    TEST_ASSERT_EQUAL_DOUBLE(4.0, reading.celsius().value());
}

void test_temperature_source_can_change_value() {
    device_platform_test_support::MockTemperatureSource sensor(
        /*identity=*/std::nullopt, /*monotonicTimestampMs=*/0U,
        /*celsius=*/4.0);

    sensor.setReading(/*identity=*/std::nullopt,
                       /*monotonicTimestampMs=*/1000U, /*celsius=*/6.5);

    TEST_ASSERT_EQUAL_DOUBLE(6.5, sensor.read().celsius().value());
}

void test_temperature_source_fault_injection_marks_unavailable() {
    device_platform_test_support::MockTemperatureSource sensor(
        /*identity=*/std::nullopt, /*monotonicTimestampMs=*/0U,
        /*celsius=*/4.0);

    sensor.setFault(/*identity=*/std::nullopt, /*monotonicTimestampMs=*/1000U,
                     device_platform::TemperatureSampleStatus::BusFault);

    const auto reading = sensor.read();
    TEST_ASSERT_FALSE(reading.status() ==
                       device_platform::TemperatureSampleStatus::Ok);
    TEST_ASSERT_FALSE(reading.celsius().has_value());
}

void test_bidirectional_actuator_sink_tracks_current_state() {
    device_platform_test_support::MockBidirectionalActuatorSink actuator;

    actuator.setForward(true);

    TEST_ASSERT_TRUE(actuator.forward());
    TEST_ASSERT_FALSE(actuator.reverse());
}

void test_bidirectional_actuator_sink_journals_every_command_in_order() {
    device_platform_test_support::MockBidirectionalActuatorSink actuator;

    actuator.setForward(true);
    actuator.setForward(false);
    actuator.setReverse(true);

    const auto& journal = actuator.commandJournal();
    TEST_ASSERT_EQUAL_UINT32(3U, journal.size());
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(device_platform_test_support::
                             BidirectionalActuatorCommandKind::Forward),
        static_cast<int>(journal[0].kind));
    TEST_ASSERT_TRUE(journal[0].enabled);
    TEST_ASSERT_FALSE(journal[1].enabled);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(device_platform_test_support::
                             BidirectionalActuatorCommandKind::Reverse),
        static_cast<int>(journal[2].kind));
    TEST_ASSERT_TRUE(journal[2].enabled);
}

void test_bidirectional_actuator_sink_makes_simultaneous_activation_visible() {
    device_platform_test_support::MockBidirectionalActuatorSink actuator;

    TEST_ASSERT_FALSE(actuator.simultaneousActivationObserved());

    actuator.setForward(true);
    TEST_ASSERT_FALSE(actuator.simultaneousActivationObserved());

    actuator.setReverse(true);
    TEST_ASSERT_TRUE(actuator.simultaneousActivationObserved());

    // Bleibt sichtbar, auch wenn danach nur noch eine Richtung aktiv ist.
    actuator.setForward(false);
    TEST_ASSERT_TRUE(actuator.simultaneousActivationObserved());
}

void test_binary_output_sink_tracks_current_state_and_journal() {
    device_platform_test_support::MockBinaryOutputSink output;

    TEST_ASSERT_FALSE(output.enabled());

    output.setEnabled(true);
    output.setEnabled(false);

    TEST_ASSERT_FALSE(output.enabled());
    const auto& journal = output.commandJournal();
    TEST_ASSERT_EQUAL_UINT32(2U, journal.size());
    TEST_ASSERT_TRUE(journal[0].enabled);
    TEST_ASSERT_FALSE(journal[1].enabled);
}

void test_thermal_model_heats_deterministically() {
    device_platform_test_support::ThermalSimulationModel model(kTestConfig,
                                                               4.0);

    model.advance(10000, /*heating=*/true, /*cooling=*/false);

    TEST_ASSERT_EQUAL_DOUBLE(5.0, model.celsius());
}

void test_thermal_model_cools_deterministically() {
    device_platform_test_support::ThermalSimulationModel model(kTestConfig,
                                                               4.0);

    model.advance(10000, /*heating=*/false, /*cooling=*/true);

    TEST_ASSERT_EQUAL_DOUBLE(2.0, model.celsius());
}

void test_thermal_model_drifts_toward_ambient_when_idle() {
    device_platform_test_support::ThermalSimulationModel model(kTestConfig,
                                                               4.0);

    model.advance(20000, /*heating=*/false, /*cooling=*/false);

    TEST_ASSERT_EQUAL_DOUBLE(5.0, model.celsius());
}

void test_thermal_model_does_not_overshoot_ambient_while_drifting() {
    device_platform_test_support::ThermalSimulationModel model(kTestConfig,
                                                               19.99);

    model.advance(60000, /*heating=*/false, /*cooling=*/false);

    TEST_ASSERT_EQUAL_DOUBLE(20.0, model.celsius());
}

void test_thermal_model_ignores_simultaneous_heating_and_cooling() {
    device_platform_test_support::ThermalSimulationModel model(kTestConfig,
                                                               4.0);

    model.advance(10000, /*heating=*/true, /*cooling=*/true);

    TEST_ASSERT_EQUAL_DOUBLE(4.0, model.celsius());
}

void test_deterministic_heat_cool_cycle_driven_by_virtual_time() {
    device_platform::VirtualTimeSource timeSource;
    device_platform_test_support::MockBidirectionalActuatorSink actuator;
    device_platform_test_support::ThermalSimulationModel model(kTestConfig,
                                                               4.0);

    uint64_t lastUpdateMillis = timeSource.monotonicMillis();

    // "Forward" steht hier stellvertretend fuer Heizen; die Rollenzuordnung
    // ist Aufgabe der Anwendung, nicht der Plattform.
    actuator.setForward(true);
    timeSource.advanceMonotonicMillis(10000);
    model.advance(timeSource.monotonicMillis() - lastUpdateMillis,
                  actuator.forward(), actuator.reverse());
    lastUpdateMillis = timeSource.monotonicMillis();
    TEST_ASSERT_EQUAL_DOUBLE(5.0, model.celsius());

    actuator.setForward(false);
    actuator.setReverse(true);
    timeSource.advanceMonotonicMillis(5000);
    model.advance(timeSource.monotonicMillis() - lastUpdateMillis,
                  actuator.forward(), actuator.reverse());
    lastUpdateMillis = timeSource.monotonicMillis();
    TEST_ASSERT_EQUAL_DOUBLE(4.0, model.celsius());
}

void test_power_loss_and_restart_reset_to_safe_defaults() {
    device_platform::VirtualTimeSource beforeRestart;
    device_platform_test_support::MockBidirectionalActuatorSink
        actuatorBeforeRestart;
    device_platform_test_support::MockBinaryOutputSink outputBeforeRestart;
    actuatorBeforeRestart.setForward(true);
    outputBeforeRestart.setEnabled(true);
    beforeRestart.advanceMonotonicMillis(60000);
    beforeRestart.setUnixTimeSeconds(1700000000);

    // Ein Stromausfall/Neustart wird durch frische Instanzen nachgebildet:
    // kein Adapter darf ueberlebenden Aktorzustand aus dem vorherigen Lauf
    // erben.
    const device_platform::VirtualTimeSource afterRestart;
    const device_platform_test_support::MockBidirectionalActuatorSink
        actuatorAfterRestart;
    const device_platform_test_support::MockBinaryOutputSink outputAfterRestart;

    TEST_ASSERT_EQUAL_UINT64(0U, afterRestart.monotonicMillis());
    TEST_ASSERT_FALSE(afterRestart.unixTimeSeconds().has_value());
    TEST_ASSERT_FALSE(actuatorAfterRestart.forward());
    TEST_ASSERT_FALSE(actuatorAfterRestart.reverse());
    TEST_ASSERT_FALSE(outputAfterRestart.enabled());
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_temperature_source_reports_configured_value);
    RUN_TEST(test_temperature_source_can_change_value);
    RUN_TEST(test_temperature_source_fault_injection_marks_unavailable);
    RUN_TEST(test_bidirectional_actuator_sink_tracks_current_state);
    RUN_TEST(test_bidirectional_actuator_sink_journals_every_command_in_order);
    RUN_TEST(
        test_bidirectional_actuator_sink_makes_simultaneous_activation_visible);
    RUN_TEST(test_binary_output_sink_tracks_current_state_and_journal);
    RUN_TEST(test_thermal_model_heats_deterministically);
    RUN_TEST(test_thermal_model_cools_deterministically);
    RUN_TEST(test_thermal_model_drifts_toward_ambient_when_idle);
    RUN_TEST(test_thermal_model_does_not_overshoot_ambient_while_drifting);
    RUN_TEST(test_thermal_model_ignores_simultaneous_heating_and_cooling);
    RUN_TEST(test_deterministic_heat_cool_cycle_driven_by_virtual_time);
    RUN_TEST(test_power_loss_and_restart_reset_to_safe_defaults);
    return UNITY_END();
}
