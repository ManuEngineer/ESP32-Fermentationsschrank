#include <unity.h>

#include <cstdint>
#include <optional>

#include "control_chain.hpp"
#include "control_sensor_evidence.hpp"
#include "device_platform.hpp"
#include "fermentation_application.hpp"
#include "fermentation_ui_text.hpp"
#include "mock_bidirectional_actuator_sink.hpp"
#include "mock_binary_output_sink.hpp"
#include "mock_time_zone_resolver.hpp"
#include "raw_temperature_samples.hpp"
#include "sensor_quality_config.hpp"
#include "simulated_persistent_state_store.hpp"
#include "temperature_source.hpp"
#include "virtual_time_source.hpp"
#include <string>

#include "web_json_codec.hpp"

// R1 trial plan S1: real technical DS18B20 readings on the local display
// without claiming `SensorQuality::Valid`. TEST fixtures only.

namespace {

using namespace fermentation;

class ScriptedSource final : public device_platform::ITemperatureSource {
   public:
    explicit ScriptedSource(const device_platform::VirtualTimeSource& time)
        : time_(time) {}
    void set(double celsius) {
        celsius_ = celsius;
        status_ = device_platform::TemperatureSampleStatus::Ok;
        stamp_ = time_.monotonicMillis();
    }
    void fail(device_platform::TemperatureSampleStatus status) {
        status_ = status;
        stamp_ = time_.monotonicMillis();
    }
    [[nodiscard]] device_platform::TemperatureReading read() const override {
        const bool ok = status_ == device_platform::TemperatureSampleStatus::Ok;
        return device_platform::TemperatureReading::create(
                   device_platform::SensorIdentity::create(rom_).identity, stamp_,
                   status_, ok ? std::optional<double>{celsius_} : std::nullopt)
            .reading.value();
    }
    std::uint64_t rom_{0x28A60000006C7248ULL};

   private:
    const device_platform::VirtualTimeSource& time_;
    double celsius_{20.0};
    device_platform::TemperatureSampleStatus status_{
        device_platform::TemperatureSampleStatus::MissingSample};
    std::uint64_t stamp_{0U};
};

struct Rig {
    device_platform::DevicePlatform platform;
    device_platform_test_support::SimulatedPersistentStateStore store;
    device_platform_test_support::MockTimeZoneResolver timeZoneResolver;
    device_platform::VirtualTimeSource time;
    ScriptedSource air{time};
    ScriptedSource cooling{time};
    ScriptedSource product{time};
    ControlSensorEvidenceProducer producer{
        air, cooling, product, time,
        std::optional<device_platform::SensorQualityConfig>{}};
    FermentationApplication application;

    Rig() {
        time.setUnixTimeSeconds(1'700'000'000LL);
        time.advanceMonotonicMillis(10'000U);
        TEST_ASSERT_TRUE(platform.begin({true}));
        TEST_ASSERT_TRUE(
            application.begin(platform, store, timeZoneResolver, time));
    }
    void publish() {
        application.publishRawTemperatureSamples(producer.readRawSamples());
    }
    [[nodiscard]] const TemperatureView& view(
        const FermentationUiSnapshot& snapshot,
        FermentationTemperatureRole role) const {
        for (const auto& candidate : snapshot.temperatures) {
            if (candidate.role == role) return candidate;
        }
        TEST_FAIL_MESSAGE("role missing");
        return snapshot.temperatures.front();
    }
};

void test_without_publication_nothing_is_shown() {
    Rig rig;
    const auto snapshot = rig.application.uiSnapshot();
    for (const auto& view : snapshot.temperatures) {
        TEST_ASSERT_FALSE(view.valueCelsius.has_value());
        TEST_ASSERT_TRUE(view.basis == TemperatureValueBasis::Qualified);
    }
}

void test_fresh_raw_value_is_shown_unqualified_and_never_valid() {
    Rig rig;
    rig.air.set(44.75);
    rig.cooling.set(21.25);
    rig.product.set(21.5);
    rig.publish();
    const auto snapshot = rig.application.uiSnapshot();
    const auto& air = rig.view(snapshot, FermentationTemperatureRole::CabinetAir);
    const auto& cooling =
        rig.view(snapshot, FermentationTemperatureRole::Cooling);
    const auto& product = rig.view(snapshot, FermentationTemperatureRole::Product);
    TEST_ASSERT_TRUE(air.valueCelsius.has_value());
    TEST_ASSERT_EQUAL_DOUBLE(44.75, *air.valueCelsius);
    TEST_ASSERT_EQUAL_DOUBLE(21.25, *cooling.valueCelsius);
    TEST_ASSERT_EQUAL_DOUBLE(21.5, *product.valueCelsius);
    for (const auto* view : {&air, &cooling, &product}) {
        TEST_ASSERT_TRUE(view->basis == TemperatureValueBasis::RawUnqualified);
        TEST_ASSERT_TRUE(view->quality.quality !=
                         device_platform::SensorQuality::Valid);
        TEST_ASSERT_TRUE(view->quality.rawCelsius.has_value());
        TEST_ASSERT_TRUE(view->quality.identity.has_value());
        TEST_ASSERT_EQUAL_UINT64(0U, *view->quality.lastAcceptedSampleAgeMs);
        TEST_ASSERT_TRUE(temperatureQualityTextKey(*view) ==
                         fermentationTextKey("quality-unqualified"));
    }
}

void test_failed_or_missing_readings_are_not_shown() {
    Rig rig;
    rig.air.fail(device_platform::TemperatureSampleStatus::BusFault);
    rig.cooling.fail(device_platform::TemperatureSampleStatus::CrcFault);
    rig.product.fail(device_platform::TemperatureSampleStatus::MissingSample);
    rig.publish();
    const auto snapshot = rig.application.uiSnapshot();
    for (const auto& view : snapshot.temperatures) {
        TEST_ASSERT_FALSE(view.valueCelsius.has_value());
        TEST_ASSERT_TRUE(view.basis == TemperatureValueBasis::Qualified);
    }
}

void test_value_expires_without_new_samples() {
    Rig rig;
    rig.air.set(30.0);
    rig.cooling.set(30.0);
    rig.product.set(30.0);
    rig.publish();
    rig.time.advanceMonotonicMillis(kRawDisplayMaxAgeMs);
    auto snapshot = rig.application.uiSnapshot();
    TEST_ASSERT_TRUE(rig.view(snapshot, FermentationTemperatureRole::CabinetAir)
                         .valueCelsius.has_value());
    TEST_ASSERT_EQUAL_UINT64(
        kRawDisplayMaxAgeMs,
        *rig.view(snapshot, FermentationTemperatureRole::CabinetAir)
             .quality.lastAcceptedSampleAgeMs);
    rig.time.advanceMonotonicMillis(1U);
    snapshot = rig.application.uiSnapshot();
    for (const auto& view : snapshot.temperatures) {
        TEST_ASSERT_FALSE(view.valueCelsius.has_value());
    }
}

void test_recovery_after_a_new_sample() {
    Rig rig;
    rig.air.set(30.0);
    rig.publish();
    rig.time.advanceMonotonicMillis(kRawDisplayMaxAgeMs + 1U);
    rig.air.set(31.0);
    rig.publish();
    const auto snapshot = rig.application.uiSnapshot();
    const auto& air = rig.view(snapshot, FermentationTemperatureRole::CabinetAir);
    TEST_ASSERT_TRUE(air.valueCelsius.has_value());
    TEST_ASSERT_EQUAL_DOUBLE(31.0, *air.valueCelsius);
}

void test_raw_display_is_never_runtime_evidence_or_start_evidence() {
    Rig rig;
    rig.air.set(30.0);
    rig.cooling.set(30.0);
    rig.product.set(30.0);
    rig.publish();
    // The producer without quality configuration produces no evidence at all.
    TEST_ASSERT_FALSE(rig.producer.sample(ProcessState::Standby).has_value());
    FermentationUiCommandContext context;
    context.expected = rig.application.uiSnapshot().revisions;
    context.monotonicMillis = rig.time.monotonicMillis();
    FermentationUiStopRunIntent stop;
    stop.option = StopOption::AbortAndTurnOff;
    // Nothing a raw value does makes the start admission see a valid sensor:
    // a program start stays refused exactly as without any raw publication.
    FermentationUiStartProgramIntent start;
    start.candidate.programId = "no-such-program";
    const auto before = rig.application.prepareStartProgram(context, start);
    Rig other;
    FermentationUiCommandContext context2;
    context2.expected = other.application.uiSnapshot().revisions;
    context2.monotonicMillis = other.time.monotonicMillis();
    const auto reference = other.application.prepareStartProgram(context2, start);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(reference.status),
                          static_cast<int>(before.status));
    TEST_ASSERT_FALSE(before.request.has_value());
}

void test_web_json_keeps_the_qualified_contract() {
    Rig rig;
    rig.air.set(44.75);
    rig.publish();
    const auto snapshot = rig.application.uiSnapshot();
    const auto& air = rig.view(snapshot, FermentationTemperatureRole::CabinetAir);
    TEST_ASSERT_TRUE(air.basis == TemperatureValueBasis::RawUnqualified);
    std::string json;
    TEST_ASSERT_TRUE(encodeWebApiTemperatures(snapshot, json));
    // The raw value never reaches the web contract: quality stale, valid
    // false, value null for every role.
    TEST_ASSERT_TRUE(json.find("44.75") == std::string::npos);
    TEST_ASSERT_TRUE(json.find("\"valueCelsius\":null") != std::string::npos);
    TEST_ASSERT_TRUE(json.find("\"valid\":true") == std::string::npos);
}

}  // namespace

void setUp() {}
void tearDown() {}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_without_publication_nothing_is_shown);
    RUN_TEST(test_fresh_raw_value_is_shown_unqualified_and_never_valid);
    RUN_TEST(test_failed_or_missing_readings_are_not_shown);
    RUN_TEST(test_value_expires_without_new_samples);
    RUN_TEST(test_recovery_after_a_new_sample);
    RUN_TEST(test_raw_display_is_never_runtime_evidence_or_start_evidence);
    RUN_TEST(test_web_json_keeps_the_qualified_contract);
    return UNITY_END();
}
