#include <unity.h>

#include <cstdint>
#include <optional>

#include "control_sensor_evidence.hpp"
#include "sensor_identity.hpp"
#include "sensor_quality_config.hpp"
#include "temperature_source.hpp"
#include "virtual_time_source.hpp"

// R1 trial plan S3: quality configuration per role. TEST fixtures only.

namespace {

using namespace fermentation;

class FixedSource final : public device_platform::ITemperatureSource {
   public:
    explicit FixedSource(const device_platform::VirtualTimeSource& time)
        : time_(time) {}
    void set(double celsius) { celsius_ = celsius; }
    [[nodiscard]] device_platform::TemperatureReading read() const override {
        return device_platform::TemperatureReading::create(
                   device_platform::SensorIdentity::create(0x28A6ULL).identity,
                   time_.monotonicMillis(),
                   device_platform::TemperatureSampleStatus::Ok, celsius_)
            .reading.value();
    }

   private:
    const device_platform::VirtualTimeSource& time_;
    double celsius_{20.0};
};

device_platform::SensorQualityConfig config(double maxPlausible) {
    return device_platform::SensorQualityConfig::create(
               3U, 5.0, -20.0, maxPlausible, 100.0, 10'000U, 3U, 2U, 2'000U)
        .config.value();
}

struct Rig {
    device_platform::VirtualTimeSource time;
    FixedSource air{time};
    FixedSource cooling{time};
    FixedSource product{time};
};

std::optional<CrossRolePlausibilityContext> run(
    Rig& rig, ControlSensorEvidenceProducer& producer, int ticks = 6) {
    std::optional<CrossRolePlausibilityContext> last;
    for (int i = 0; i < ticks; ++i) {
        rig.time.advanceMonotonicMillis(1'000U);
        last = producer.sample(ProcessState::Standby);
    }
    return last;
}

void test_the_cooling_body_may_use_a_wider_range_than_the_air() {
    Rig rig;
    ControlSensorQualityConfigs configs;
    configs.air = config(60.0);
    configs.cooling = config(100.0);
    configs.product = config(60.0);
    ControlSensorEvidenceProducer producer(rig.air, rig.cooling, rig.product,
                                           rig.time, configs);
    rig.air.set(30.0);
    rig.cooling.set(90.0);  // plausible for the heat sink, not for the air
    rig.product.set(25.0);
    const auto evidence = run(rig, producer);
    TEST_ASSERT_TRUE(evidence.has_value());
    TEST_ASSERT_TRUE(evidence->cooling.quality ==
                     device_platform::SensorQuality::Valid);
    TEST_ASSERT_TRUE(evidence->air.quality ==
                     device_platform::SensorQuality::Valid);

    Rig other;
    ControlSensorEvidenceProducer same(other.air, other.cooling, other.product,
                                       other.time, std::optional{config(60.0)});
    other.air.set(30.0);
    other.cooling.set(90.0);  // implausible with the single (air) range
    other.product.set(25.0);
    const auto strict = run(other, same);
    TEST_ASSERT_TRUE(strict.has_value());
    TEST_ASSERT_TRUE(strict->cooling.quality !=
                     device_platform::SensorQuality::Valid);
}

void test_air_and_cooling_are_mandatory_for_any_evidence() {
    Rig rig;
    ControlSensorQualityConfigs onlyAir;
    onlyAir.air = config(60.0);
    ControlSensorEvidenceProducer noCooling(rig.air, rig.cooling, rig.product,
                                            rig.time, onlyAir);
    TEST_ASSERT_FALSE(noCooling.configured());
    TEST_ASSERT_FALSE(noCooling.sample(ProcessState::Standby).has_value());

    ControlSensorQualityConfigs onlyCooling;
    onlyCooling.cooling = config(100.0);
    ControlSensorEvidenceProducer noAir(rig.air, rig.cooling, rig.product,
                                        rig.time, onlyCooling);
    TEST_ASSERT_FALSE(noAir.configured());
    TEST_ASSERT_FALSE(noAir.sample(ProcessState::Standby).has_value());
}

void test_a_product_without_configuration_never_becomes_valid() {
    Rig rig;
    ControlSensorQualityConfigs configs;
    configs.air = config(60.0);
    configs.cooling = config(100.0);
    ControlSensorEvidenceProducer producer(rig.air, rig.cooling, rig.product,
                                           rig.time, configs);
    TEST_ASSERT_TRUE(producer.configured());
    const auto evidence = run(rig, producer);
    TEST_ASSERT_TRUE(evidence.has_value());
    TEST_ASSERT_TRUE(evidence->air.quality ==
                     device_platform::SensorQuality::Valid);
    TEST_ASSERT_TRUE(evidence->product.quality !=
                     device_platform::SensorQuality::Valid);
    TEST_ASSERT_FALSE(evidence->product.rawCelsius.has_value());
}

void test_the_single_config_constructor_is_unchanged() {
    Rig rig;
    ControlSensorEvidenceProducer unconfigured(
        rig.air, rig.cooling, rig.product, rig.time,
        std::optional<device_platform::SensorQualityConfig>{});
    TEST_ASSERT_FALSE(unconfigured.configured());
    TEST_ASSERT_FALSE(unconfigured.sample(ProcessState::Standby).has_value());
    // Raw readings work without any quality configuration.
    const auto raw = unconfigured.readRawSamples();
    TEST_ASSERT_TRUE(raw.air.has_value());
    TEST_ASSERT_TRUE(raw.cooling.has_value());
    TEST_ASSERT_TRUE(raw.product.has_value());
}

}  // namespace

void setUp() {}
void tearDown() {}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_the_cooling_body_may_use_a_wider_range_than_the_air);
    RUN_TEST(test_air_and_cooling_are_mandatory_for_any_evidence);
    RUN_TEST(test_a_product_without_configuration_never_becomes_valid);
    RUN_TEST(test_the_single_config_constructor_is_unchanged);
    return UNITY_END();
}
