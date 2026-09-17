#include <unity.h>

#include <cstdint>
#include <string>
#include <utility>

#include "configuration_document_codec.hpp"
#include "configuration_documents.hpp"
#include "mock_time_zone_resolver.hpp"
#include "network_mode.hpp"

namespace {

using device_platform::NetworkMode;
using device_platform_test_support::MockTimeZoneResolver;
using fermentation::ConfigurationCodecStatus;
using fermentation::HomeWifiCredentials;
using fermentation::UserConfiguration;
using fermentation::UserConfigurationSchema;

UserConfiguration validConfiguration() {
    return {"de", "Europe/Zurich", "Fermentationsschrank"};
}

void test_mode_is_explicit_and_credentials_are_optional() {
    MockTimeZoneResolver resolver;
    auto configuration = validConfiguration();
    configuration.networkMode = NetworkMode::AP_ONLY;
    TEST_ASSERT_TRUE(fermentation::validateUserConfiguration(configuration,
                                                              resolver)
                         .status == fermentation::UserConfigurationStatus::Success);
    TEST_ASSERT_FALSE(configuration.homeWifiCredentials.has_value());

    configuration.networkMode = NetworkMode::HOME_WIFI;
    configuration.homeWifiCredentials =
        HomeWifiCredentials{"Fermentation-WLAN", "correct-horse-battery"};
    TEST_ASSERT_TRUE(fermentation::validateUserConfiguration(configuration,
                                                              resolver)
                         .status == fermentation::UserConfigurationStatus::Success);
}

void test_startup_decision_never_infers_ap_only_from_missing_credentials() {
    using device_platform::NetworkStartupPath;
    TEST_ASSERT_TRUE(device_platform::decideNetworkStartup(
                         NetworkMode::AP_ONLY, false, false)
                         .path == NetworkStartupPath::AccessPointOnly);
    TEST_ASSERT_TRUE(device_platform::decideNetworkStartup(
                         NetworkMode::HOME_WIFI, false, false)
                         .path == NetworkStartupPath::HomeWifiSetup);
    TEST_ASSERT_TRUE(device_platform::decideNetworkStartup(
                         NetworkMode::HOME_WIFI, true, false)
                         .path == NetworkStartupPath::HomeWifi);
    TEST_ASSERT_TRUE(device_platform::decideNetworkStartup(
                         NetworkMode::HOME_WIFI, true, true)
                         .path == NetworkStartupPath::HomeWifiSetup);
}

void test_invalid_network_values_fail_closed() {
    MockTimeZoneResolver resolver;
    auto configuration = validConfiguration();
    configuration.networkMode = static_cast<NetworkMode>(0xFFU);
    TEST_ASSERT_TRUE(fermentation::validateUserConfiguration(configuration,
                                                              resolver)
                         .status == fermentation::UserConfigurationStatus::InvalidNetworkMode);

    configuration = validConfiguration();
    configuration.homeWifiCredentials =
        HomeWifiCredentials{"", "short"};
    TEST_ASSERT_TRUE(fermentation::validateUserConfiguration(configuration,
                                                              resolver)
                         .status == fermentation::UserConfigurationStatus::InvalidHomeWifiSsid);

    configuration.homeWifiCredentials =
        HomeWifiCredentials{"ssid", "short"};
    TEST_ASSERT_TRUE(fermentation::validateUserConfiguration(configuration,
                                                              resolver)
                         .status == fermentation::UserConfigurationStatus::InvalidHomeWifiPassword);
}

void test_v3_round_trip_preserves_mode_and_credentials() {
    MockTimeZoneResolver resolver;
    auto configuration = validConfiguration();
    configuration.networkMode = NetworkMode::HOME_WIFI;
    configuration.homeWifiCredentials =
        HomeWifiCredentials{"Fermentation-WLAN", "correct-horse-battery"};
    std::string payload;
    TEST_ASSERT_TRUE(fermentation::encodeUserConfigurationPayload(
                         configuration,
                         static_cast<std::uint32_t>(UserConfigurationSchema::Version3),
                         resolver, payload) == ConfigurationCodecStatus::Success);

    const auto decoded = fermentation::decodeUserConfigurationPayload(
        static_cast<std::uint32_t>(UserConfigurationSchema::Version3), payload,
        resolver);
    TEST_ASSERT_TRUE(decoded.document.has_value());
    TEST_ASSERT_TRUE(fermentation::configurationContentEquals(
        configuration, *decoded.document));
}

void test_v2_migrates_to_home_wifi_without_credentials() {
    MockTimeZoneResolver resolver;
    auto configuration = validConfiguration();
    configuration.networkMode = NetworkMode::AP_ONLY;
    configuration.homeWifiCredentials =
        HomeWifiCredentials{"old-ssid", "old-password"};
    std::string payload;
    TEST_ASSERT_TRUE(fermentation::encodeUserConfigurationPayload(
                         configuration,
                         static_cast<std::uint32_t>(UserConfigurationSchema::Version2),
                         resolver, payload) == ConfigurationCodecStatus::Success);

    const auto decoded = fermentation::decodeUserConfigurationPayload(
        static_cast<std::uint32_t>(UserConfigurationSchema::Version2), payload,
        resolver);
    TEST_ASSERT_TRUE(decoded.document.has_value());
    TEST_ASSERT_TRUE(decoded.document->networkMode == NetworkMode::HOME_WIFI);
    TEST_ASSERT_FALSE(decoded.document->homeWifiCredentials.has_value());
}

}  // namespace

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_mode_is_explicit_and_credentials_are_optional);
    RUN_TEST(test_startup_decision_never_infers_ap_only_from_missing_credentials);
    RUN_TEST(test_invalid_network_values_fail_closed);
    RUN_TEST(test_v3_round_trip_preserves_mode_and_credentials);
    RUN_TEST(test_v2_migrates_to_home_wifi_without_credentials);
    return UNITY_END();
}
