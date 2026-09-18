#include <unity.h>

#include <cstdint>
#include <optional>
#include <string>
#include <utility>

#include "configuration_document_codec.hpp"
#include "configuration_documents.hpp"
#include "connectivity_credential_codec.hpp"
#include "connectivity_credentials.hpp"
#include "mock_time_zone_resolver.hpp"
#include "mock_network_lifecycle.hpp"
#include "network_configuration_service.hpp"
#include "network_setup_routes.hpp"
#include "network_mode.hpp"
#include "simulated_persistent_state_store.hpp"

namespace {

using device_platform::NetworkMode;
using device_platform_test_support::MockNetworkLifecycle;
using device_platform_test_support::MockTimeZoneResolver;
using device_platform_test_support::SimulatedPersistentStateStore;
using fermentation::ConfigurationCodecStatus;
using fermentation::ConnectivityCredential;
using fermentation::ConnectivityCredentialCodecStatus;
using fermentation::ConnectivityCredentialLoadStatus;
using fermentation::ConnectivityCredentialStore;
using fermentation::ConnectivityCredentialValidationStatus;
using fermentation::ConnectivityCredentialWriteStatus;
using fermentation::NetworkConfigurationService;
using fermentation::NetworkConfigurationStatus;
using fermentation::NetworkSetupRoutes;
using fermentation::NetworkStartupPath;
using fermentation::UserConfiguration;
using fermentation::UserConfigurationSchema;

UserConfiguration validConfiguration() {
    return {"de", "Europe/Zurich", "Fermentationsschrank"};
}

ConnectivityCredential validCredential() {
    return ConnectivityCredential{device_platform::NetworkCredentials{
        "Fermentation-WLAN", "correct-horse-battery"}};
}

void test_mode_selection_has_internal_unselected_state() {
    TEST_ASSERT_TRUE(
        device_platform::isValidNetworkMode(NetworkMode::UNSELECTED));
    TEST_ASSERT_FALSE(
        device_platform::isSelectableNetworkMode(NetworkMode::UNSELECTED));
    TEST_ASSERT_TRUE(
        device_platform::isSelectableNetworkMode(NetworkMode::AP_ONLY));
    TEST_ASSERT_TRUE(
        device_platform::isSelectableNetworkMode(NetworkMode::HOME_WIFI));

    MockTimeZoneResolver resolver;
    auto configuration = validConfiguration();
    TEST_ASSERT_TRUE(
        fermentation::validateUserConfiguration(configuration, resolver)
            .status == fermentation::UserConfigurationStatus::Success);
    configuration.networkMode = static_cast<NetworkMode>(0xFFU);
    TEST_ASSERT_TRUE(
        fermentation::validateUserConfiguration(configuration, resolver)
            .status ==
        fermentation::UserConfigurationStatus::InvalidNetworkMode);
}

void test_startup_decision_requires_selection_and_never_infers_mode() {
    TEST_ASSERT_TRUE(fermentation::decideNetworkStartup(NetworkMode::UNSELECTED,
                                                        false, false)
                         .path == NetworkStartupPath::SelectionRequired);
    TEST_ASSERT_TRUE(
        fermentation::decideNetworkStartup(NetworkMode::AP_ONLY, false, false)
            .path == NetworkStartupPath::AccessPointOnly);
    TEST_ASSERT_TRUE(
        fermentation::decideNetworkStartup(NetworkMode::HOME_WIFI, false, false)
            .path == NetworkStartupPath::HomeWifiSetup);
    TEST_ASSERT_TRUE(
        fermentation::decideNetworkStartup(NetworkMode::HOME_WIFI, true, false)
            .path == NetworkStartupPath::HomeWifi);
    TEST_ASSERT_TRUE(
        fermentation::decideNetworkStartup(NetworkMode::HOME_WIFI, true, true)
            .path == NetworkStartupPath::HomeWifiSetup);
}

void test_v1_v2_migrate_to_unselected_and_v3_has_no_credentials() {
    MockTimeZoneResolver resolver;
    auto configuration = validConfiguration();
    std::string v1;
    TEST_ASSERT_TRUE(
        fermentation::encodeUserConfigurationPayload(
            configuration,
            static_cast<std::uint32_t>(UserConfigurationSchema::Version1),
            resolver, v1) == ConfigurationCodecStatus::Success);
    const auto decodedV1 = fermentation::decodeUserConfigurationPayload(
        static_cast<std::uint32_t>(UserConfigurationSchema::Version1), v1,
        resolver);
    TEST_ASSERT_TRUE(decodedV1.document.has_value());
    TEST_ASSERT_TRUE(decodedV1.document->networkMode ==
                     NetworkMode::UNSELECTED);

    std::string v2;
    TEST_ASSERT_TRUE(
        fermentation::encodeUserConfigurationPayload(
            configuration,
            static_cast<std::uint32_t>(UserConfigurationSchema::Version2),
            resolver, v2) == ConfigurationCodecStatus::Success);
    const auto decodedV2 = fermentation::decodeUserConfigurationPayload(
        static_cast<std::uint32_t>(UserConfigurationSchema::Version2), v2,
        resolver);
    TEST_ASSERT_TRUE(decodedV2.document.has_value());
    TEST_ASSERT_TRUE(decodedV2.document->networkMode ==
                     NetworkMode::UNSELECTED);

    configuration.networkMode = NetworkMode::HOME_WIFI;
    std::string v3;
    TEST_ASSERT_TRUE(
        fermentation::encodeUserConfigurationPayload(
            configuration,
            static_cast<std::uint32_t>(UserConfigurationSchema::Version3),
            resolver, v3) == ConfigurationCodecStatus::Success);
    TEST_ASSERT_EQUAL_UINT32(v2.size() + 1U, v3.size());
    const auto decodedV3 = fermentation::decodeUserConfigurationPayload(
        static_cast<std::uint32_t>(UserConfigurationSchema::Version3), v3,
        resolver);
    TEST_ASSERT_TRUE(decodedV3.document.has_value());
    TEST_ASSERT_TRUE(decodedV3.document->networkMode == NetworkMode::HOME_WIFI);
}

void test_connectivity_credential_codec_keeps_pair_together() {
    const auto credential = validCredential();
    TEST_ASSERT_TRUE(fermentation::validateConnectivityCredential(credential) ==
                     ConnectivityCredentialValidationStatus::Success);
    std::string payload;
    TEST_ASSERT_TRUE(fermentation::encodeConnectivityCredentialPayload(
                         credential, payload) ==
                     ConnectivityCredentialCodecStatus::Success);
    const auto decoded =
        fermentation::decodeConnectivityCredentialPayload(1U, payload);
    TEST_ASSERT_TRUE(decoded.credential.has_value());
    TEST_ASSERT_TRUE(*decoded.credential == credential);

    const ConnectivityCredential empty;
    TEST_ASSERT_TRUE(
        fermentation::encodeConnectivityCredentialPayload(empty, payload) ==
        ConnectivityCredentialCodecStatus::Success);
    const auto decodedEmpty =
        fermentation::decodeConnectivityCredentialPayload(1U, payload);
    TEST_ASSERT_TRUE(decodedEmpty.credential.has_value());
    TEST_ASSERT_TRUE(decodedEmpty.credential->homeWifi == std::nullopt);
}

void test_connectivity_credential_store_uses_cc0_and_epoch_binding() {
    SimulatedPersistentStateStore store;
    ConnectivityCredentialStore credentials(store);
    const auto written = credentials.write(
        validCredential(), device_platform::StorageEpoch{1U}, 1U);
    TEST_ASSERT_TRUE(
        written.status ==
        fermentation::ConnectivityCredentialWriteStatus::Committed);
    const auto loaded = credentials.load(device_platform::StorageEpoch{1U});
    TEST_ASSERT_TRUE(loaded.status ==
                     ConnectivityCredentialLoadStatus::Available);
    TEST_ASSERT_TRUE(loaded.record.has_value());
    TEST_ASSERT_TRUE(loaded.record->credential == validCredential());
    TEST_ASSERT_EQUAL_UINT64(1U, loaded.record->recordSequence);
    TEST_ASSERT_EQUAL_STRING(
        "cc0", ConnectivityCredentialStore::key().bytes().c_str());

    const auto otherEpoch = credentials.load(device_platform::StorageEpoch{2U});
    TEST_ASSERT_TRUE(otherEpoch.status ==
                     ConnectivityCredentialLoadStatus::OtherEpoch);
}

void test_successful_write_with_readback_error_is_indeterminate() {
    SimulatedPersistentStateStore store;
    ConnectivityCredentialStore credentials(store);
    store.failNextReadAfterWrite();
    const auto result = credentials.write(
        validCredential(), device_platform::StorageEpoch{1U}, 1U);
    TEST_ASSERT_TRUE(result.status ==
                     ConnectivityCredentialWriteStatus::Indeterminate);
    store.injectReadFailure(ConnectivityCredentialStore::key(), false);
    const auto loaded = credentials.load(device_platform::StorageEpoch{1U});
    TEST_ASSERT_TRUE(loaded.status ==
                     ConnectivityCredentialLoadStatus::Available);
    TEST_ASSERT_TRUE(loaded.record->credential == validCredential());
}

void test_network_workflow_tests_before_commit_and_preserves_on_failure() {
    SimulatedPersistentStateStore store;
    ConnectivityCredentialStore credentials(store);
    MockNetworkLifecycle lifecycle;
    NetworkConfigurationService service(credentials, lifecycle);

    TEST_ASSERT_TRUE(
        service.start(NetworkMode::HOME_WIFI, device_platform::StorageEpoch{1U})
            .status == NetworkConfigurationStatus::Applied);
    TEST_ASSERT_TRUE(lifecycle.status().state ==
                     device_platform::NetworkLifecycleState::SetupAccessPoint);
    TEST_ASSERT_TRUE(
        service.beginCandidate("Fermentation-WLAN", "correct-horse-battery")
            .status == NetworkConfigurationStatus::Applied);
    lifecycle.setCandidateStatus(
        device_platform::NetworkOperationStatus::Failed);
    TEST_ASSERT_TRUE(service.testCandidate().status ==
                     NetworkConfigurationStatus::CandidateRejected);
    TEST_ASSERT_FALSE(
        credentials.load(device_platform::StorageEpoch{1U}).record.has_value());

    TEST_ASSERT_TRUE(
        service.beginCandidate("Fermentation-WLAN", "correct-horse-battery")
            .status == NetworkConfigurationStatus::Applied);
    lifecycle.setCandidateStatus(
        device_platform::NetworkOperationStatus::Applied);
    TEST_ASSERT_TRUE(service.testCandidate().status ==
                     NetworkConfigurationStatus::Applied);
    const auto committed = credentials.load(device_platform::StorageEpoch{1U});
    TEST_ASSERT_TRUE(committed.record.has_value());
    TEST_ASSERT_TRUE(committed.record->credential == validCredential());
}

void test_candidate_wrong_password_disconnect_and_timeout_never_commit() {
    for (const auto failure :
         {device_platform::NetworkOperationStatus::Failed,
          device_platform::NetworkOperationStatus::Busy,
          device_platform::NetworkOperationStatus::InvalidInput}) {
        SimulatedPersistentStateStore store;
        ConnectivityCredentialStore credentials(store);
        MockNetworkLifecycle lifecycle;
        NetworkConfigurationService service(credentials, lifecycle);
        TEST_ASSERT_TRUE(service
                             .start(NetworkMode::HOME_WIFI,
                                    device_platform::StorageEpoch{1U})
                             .status == NetworkConfigurationStatus::Applied);
        lifecycle.setCandidateStatus(failure);
        TEST_ASSERT_TRUE(
            service.beginCandidate("wrong", "wrong-pass-1").status ==
            NetworkConfigurationStatus::Applied);
        TEST_ASSERT_TRUE(service.testCandidate().status ==
                         NetworkConfigurationStatus::CandidateRejected);
        TEST_ASSERT_FALSE(credentials.load(device_platform::StorageEpoch{1U})
                              .record.has_value());
    }
}

void test_indeterminate_commit_stops_without_restoring_old_runtime() {
    SimulatedPersistentStateStore store;
    ConnectivityCredentialStore credentials(store);
    MockNetworkLifecycle lifecycle;
    NetworkConfigurationService service(credentials, lifecycle);
    TEST_ASSERT_TRUE(
        service.start(NetworkMode::HOME_WIFI, device_platform::StorageEpoch{1U})
            .status == NetworkConfigurationStatus::Applied);
    TEST_ASSERT_TRUE(
        service.beginCandidate("new-wlan", "new-password-1").status ==
        NetworkConfigurationStatus::Applied);
    store.failNextReadAfterWrite();
    TEST_ASSERT_TRUE(service.testCandidate().status ==
                     NetworkConfigurationStatus::CommitIndeterminate);
    TEST_ASSERT_TRUE(service.recoveryRequired());
    TEST_ASSERT_EQUAL_UINT(1U, lifecycle.stopCallCount());
    TEST_ASSERT_TRUE(
        credentials.load(device_platform::StorageEpoch{1U}).record.has_value());

    // Recovery is explicit and restartable: the next start reloads the
    // durable winner instead of restoring the pre-write runtime credential.
    lifecycle.setStartStatus(device_platform::NetworkOperationStatus::Applied);
    TEST_ASSERT_TRUE(
        service.start(NetworkMode::HOME_WIFI, device_platform::StorageEpoch{1U})
            .status == NetworkConfigurationStatus::Applied);
    TEST_ASSERT_FALSE(service.recoveryRequired());
    TEST_ASSERT_TRUE(lifecycle.lastStartedCredentials().has_value());
    const device_platform::NetworkCredentials recovered{"new-wlan",
                                                        "new-password-1"};
    TEST_ASSERT_TRUE(*lifecycle.lastStartedCredentials() == recovered);
}

void test_normal_home_mode_gates_setup_mutations_until_reconfiguration() {
    SimulatedPersistentStateStore store;
    ConnectivityCredentialStore credentials(store);
    TEST_ASSERT_TRUE(
        credentials
            .write(validCredential(), device_platform::StorageEpoch{1U}, 1U)
            .status == ConnectivityCredentialWriteStatus::Committed);
    MockNetworkLifecycle lifecycle;
    NetworkConfigurationService service(credentials, lifecycle);
    TEST_ASSERT_TRUE(
        service.start(NetworkMode::HOME_WIFI, device_platform::StorageEpoch{1U})
            .status == NetworkConfigurationStatus::Applied);
    TEST_ASSERT_FALSE(service.setupFlowActive());
    TEST_ASSERT_TRUE(service.scan().status ==
                     NetworkConfigurationStatus::SetupNotAvailable);
    TEST_ASSERT_TRUE(service.beginCandidate("ssid", "password-1").status ==
                     NetworkConfigurationStatus::SetupNotAvailable);

    TEST_ASSERT_TRUE(service.beginHomeWifiReconfiguration().status ==
                     NetworkConfigurationStatus::Applied);
    TEST_ASSERT_TRUE(service.setupFlowActive());
    // Reconfiguration starts the AP setup path without deleting the stored
    // credential. A failed candidate restores that still-active credential.
    lifecycle.setCandidateStatus(
        device_platform::NetworkOperationStatus::Failed);
    TEST_ASSERT_TRUE(
        service.beginCandidate("new-wlan", "new-password-1").status ==
        NetworkConfigurationStatus::Applied);
    TEST_ASSERT_TRUE(service.testCandidate().status ==
                     NetworkConfigurationStatus::CandidateRejected);
    TEST_ASSERT_TRUE(lifecycle.lastStartedCredentials().has_value());
    TEST_ASSERT_TRUE(*lifecycle.lastStartedCredentials() ==
                     validCredential().homeWifi);
}

void test_ap_only_does_not_infer_home_wifi_from_credentials() {
    SimulatedPersistentStateStore store;
    ConnectivityCredentialStore credentials(store);
    TEST_ASSERT_TRUE(
        credentials
            .write(validCredential(), device_platform::StorageEpoch{1U}, 1U)
            .status == ConnectivityCredentialWriteStatus::Committed);
    MockNetworkLifecycle lifecycle;
    NetworkConfigurationService service(credentials, lifecycle);
    TEST_ASSERT_TRUE(
        service.start(NetworkMode::AP_ONLY, device_platform::StorageEpoch{1U})
            .status == NetworkConfigurationStatus::Applied);
    TEST_ASSERT_TRUE(lifecycle.status().state ==
                     device_platform::NetworkLifecycleState::AccessPointOnly);
    TEST_ASSERT_FALSE(lifecycle.lastStartedCredentials().has_value());
}

void test_transport_stop_start_is_restartable() {
    MockNetworkLifecycle lifecycle;
    TEST_ASSERT_TRUE(
        lifecycle.start(NetworkMode::AP_ONLY, std::nullopt).status ==
        device_platform::NetworkOperationStatus::Applied);
    TEST_ASSERT_TRUE(lifecycle.accessPointInfo().has_value());
    TEST_ASSERT_TRUE(lifecycle.stop().status ==
                     device_platform::NetworkOperationStatus::Applied);
    TEST_ASSERT_FALSE(lifecycle.accessPointInfo().has_value());
    TEST_ASSERT_TRUE(
        lifecycle.start(NetworkMode::AP_ONLY, std::nullopt).status ==
        device_platform::NetworkOperationStatus::Applied);
    TEST_ASSERT_EQUAL_UINT(2U, lifecycle.startCallCount());
    TEST_ASSERT_TRUE(lifecycle.accessPointInfo().has_value());
}

void test_setup_routes_share_one_surface_and_redact_passwords() {
    SimulatedPersistentStateStore store;
    ConnectivityCredentialStore credentials(store);
    MockNetworkLifecycle lifecycle;
    NetworkConfigurationService service(credentials, lifecycle);
    TEST_ASSERT_TRUE(
        service.start(NetworkMode::HOME_WIFI, device_platform::StorageEpoch{1U})
            .status == NetworkConfigurationStatus::Applied);
    const auto genericStatus = service.status();
    TEST_ASSERT_TRUE(genericStatus.state ==
                     device_platform::NetworkLifecycleState::SetupAccessPoint);
    const auto accessPoint = service.accessPointInfo();
    TEST_ASSERT_TRUE(accessPoint.has_value());
    TEST_ASSERT_EQUAL_STRING("mock-setup-ap", accessPoint->ssid.c_str());
    TEST_ASSERT_EQUAL_STRING("mock-ap-password", accessPoint->password.c_str());
    TEST_ASSERT_TRUE(accessPoint->ipv4Address.has_value());
    TEST_ASSERT_TRUE(*accessPoint->ipv4Address == 0x0104A8C0U);
    NetworkSetupRoutes routes(service);

    device_platform::HttpResponse response;
    TEST_ASSERT_TRUE(routes.handle({"GET", "/", {}}, response));
    TEST_ASSERT_EQUAL_UINT16(200U, response.statusCode);
    TEST_ASSERT_NOT_NULL(response.body.c_str());
    TEST_ASSERT_NOT_EQUAL(std::string::npos, response.body.find("name=ssid"));
    TEST_ASSERT_NOT_EQUAL(std::string::npos,
                          response.body.find("id=scan-ssid"));
    TEST_ASSERT_NOT_EQUAL(std::string::npos,
                          response.body.find("select.onchange"));
    TEST_ASSERT_NOT_EQUAL(std::string::npos,
                          response.body.find("/api/network/scan"));
    TEST_ASSERT_NOT_EQUAL(std::string::npos,
                          response.body.find("Test and commit"));
    TEST_ASSERT_EQUAL(std::string::npos,
                      response.body.find("mock-ap-password"));

    response = {};
    TEST_ASSERT_TRUE(
        routes.handle({"GET", "/api/network/status", {}}, response));
    TEST_ASSERT_EQUAL_UINT16(200U, response.statusCode);
    TEST_ASSERT_EQUAL(std::string::npos,
                      response.body.find("mock-ap-password"));

    response = {};
    TEST_ASSERT_TRUE(
        routes.handle({"POST", "/api/network/candidate",
                       "ssid=Fermentation-WLAN&password=correct-horse-battery"},
                      response));
    TEST_ASSERT_EQUAL_UINT16(200U, response.statusCode);
    TEST_ASSERT_NOT_EQUAL(std::string::npos,
                          response.body.find("candidate committed"));
    TEST_ASSERT_EQUAL(std::string::npos,
                      response.body.find("correct-horse-battery"));
}

std::pair<std::uint16_t, std::string> submitCandidateRoute(
    device_platform::NetworkOperationStatus candidateStatus,
    std::optional<SimulatedPersistentStateStore::WriteFault> writeFault,
    bool failRuntimeAdoption) {
    SimulatedPersistentStateStore store;
    ConnectivityCredentialStore credentials(store);
    MockNetworkLifecycle lifecycle;
    NetworkConfigurationService service(credentials, lifecycle);
    if (service.start(NetworkMode::HOME_WIFI, device_platform::StorageEpoch{1U})
            .status != NetworkConfigurationStatus::Applied) {
        return {0U, "setup failed"};
    }
    lifecycle.setCandidateStatus(candidateStatus);
    if (writeFault.has_value()) {
        store.setNextWriteFault(*writeFault);
        if (*writeFault == SimulatedPersistentStateStore::WriteFault::
                               PowerCutAfterCommitBeforeReturn) {
            store.failNextReadAfterWrite();
        }
    }
    if (failRuntimeAdoption) {
        lifecycle.setStartStatus(
            device_platform::NetworkOperationStatus::Failed);
    }
    NetworkSetupRoutes routes(service);
    device_platform::HttpResponse response;
    if (!routes.handle({"POST", "/api/network/candidate",
                        "ssid=another&password=another-password"},
                       response)) {
        return {0U, "route unavailable"};
    }
    return {response.statusCode, std::move(response.body)};
}

void test_candidate_route_distinguishes_persistence_and_recovery_outcomes() {
    const auto rejected = submitCandidateRoute(
        device_platform::NetworkOperationStatus::Failed, std::nullopt, false);
    TEST_ASSERT_TRUE(rejected.first == 422U);
    TEST_ASSERT_NOT_EQUAL(std::string::npos,
                          rejected.second.find("candidate rejected"));
    TEST_ASSERT_EQUAL(std::string::npos,
                      rejected.second.find("commit indeterminate"));

    const auto writeFailure = submitCandidateRoute(
        device_platform::NetworkOperationStatus::Applied,
        SimulatedPersistentStateStore::WriteFault::FailBeforeBegin, false);
    TEST_ASSERT_TRUE(writeFailure.first == 500U);
    TEST_ASSERT_NOT_EQUAL(std::string::npos,
                          writeFailure.second.find("write failed"));

    const auto indeterminate =
        submitCandidateRoute(device_platform::NetworkOperationStatus::Applied,
                             SimulatedPersistentStateStore::WriteFault::
                                 PowerCutAfterCommitBeforeReturn,
                             false);
    TEST_ASSERT_TRUE(indeterminate.first == 503U);
    TEST_ASSERT_NOT_EQUAL(std::string::npos,
                          indeterminate.second.find("commit indeterminate"));
    TEST_ASSERT_EQUAL(std::string::npos,
                      indeterminate.second.find("not committed"));

    const auto recoveryRequired = submitCandidateRoute(
        device_platform::NetworkOperationStatus::Applied, std::nullopt, true);
    TEST_ASSERT_TRUE(recoveryRequired.first == 503U);
    TEST_ASSERT_NOT_EQUAL(std::string::npos,
                          recoveryRequired.second.find("credential committed"));
    TEST_ASSERT_NOT_EQUAL(std::string::npos,
                          recoveryRequired.second.find("runtime recovery"));
    TEST_ASSERT_EQUAL(std::string::npos,
                      recoveryRequired.second.find("not committed"));
}

void test_normal_home_routes_do_not_render_or_accept_setup_mutations() {
    SimulatedPersistentStateStore store;
    ConnectivityCredentialStore credentials(store);
    TEST_ASSERT_TRUE(
        credentials
            .write(validCredential(), device_platform::StorageEpoch{1U}, 1U)
            .status == ConnectivityCredentialWriteStatus::Committed);
    MockNetworkLifecycle lifecycle;
    NetworkConfigurationService service(credentials, lifecycle);
    TEST_ASSERT_TRUE(
        service.start(NetworkMode::HOME_WIFI, device_platform::StorageEpoch{1U})
            .status == NetworkConfigurationStatus::Applied);
    NetworkSetupRoutes routes(service);

    device_platform::HttpResponse response;
    TEST_ASSERT_TRUE(routes.handle({"GET", "/", {}}, response));
    TEST_ASSERT_EQUAL_UINT16(200U, response.statusCode);
    TEST_ASSERT_EQUAL(std::string::npos, response.body.find("name=ssid"));
    response = {};
    TEST_ASSERT_TRUE(routes.handle({"GET", "/api/network/scan", {}}, response));
    TEST_ASSERT_EQUAL_UINT16(503U, response.statusCode);
    response = {};
    TEST_ASSERT_TRUE(routes.handle({"POST", "/api/network/candidate",
                                    "ssid=another&password=another-password"},
                                   response));
    TEST_ASSERT_EQUAL_UINT16(503U, response.statusCode);
}

}  // namespace

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_mode_selection_has_internal_unselected_state);
    RUN_TEST(test_startup_decision_requires_selection_and_never_infers_mode);
    RUN_TEST(test_v1_v2_migrate_to_unselected_and_v3_has_no_credentials);
    RUN_TEST(test_connectivity_credential_codec_keeps_pair_together);
    RUN_TEST(test_connectivity_credential_store_uses_cc0_and_epoch_binding);
    RUN_TEST(test_successful_write_with_readback_error_is_indeterminate);
    RUN_TEST(
        test_network_workflow_tests_before_commit_and_preserves_on_failure);
    RUN_TEST(test_candidate_wrong_password_disconnect_and_timeout_never_commit);
    RUN_TEST(test_indeterminate_commit_stops_without_restoring_old_runtime);
    RUN_TEST(test_normal_home_mode_gates_setup_mutations_until_reconfiguration);
    RUN_TEST(test_ap_only_does_not_infer_home_wifi_from_credentials);
    RUN_TEST(test_transport_stop_start_is_restartable);
    RUN_TEST(test_setup_routes_share_one_surface_and_redact_passwords);
    RUN_TEST(
        test_candidate_route_distinguishes_persistence_and_recovery_outcomes);
    RUN_TEST(test_normal_home_routes_do_not_render_or_accept_setup_mutations);
    return UNITY_END();
}
