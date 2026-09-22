#include <array>
#include <cstdint>
#include <string>

#include <unity.h>

#include "authentication_records.hpp"
#include "device_platform.hpp"
#include "fermentation_application.hpp"
#include "mock_time_zone_resolver.hpp"
#include "simulated_persistent_state_store.hpp"
#include "virtual_time_source.hpp"
#include "web_application_routes.hpp"

namespace {

class Random final : public device_platform::ISecureRandomSource {
   public:
    bool fill(void* buffer, std::size_t length) override {
        if (buffer == nullptr && length != 0U) return false;
        auto* bytes = static_cast<std::uint8_t*>(buffer);
        for (std::size_t i = 0U; i < length; ++i) bytes[i] = next_++;
        return true;
    }

   private:
    std::uint8_t next_{1U};
};

class Kdf final : public fermentation::IAuthenticationKdf {
   public:
    bool derive(
        const std::string&, const fermentation::AuthVerifier& verifier,
        std::array<std::uint8_t, fermentation::kAuthenticationVerifierBytes>&
            out) override {
        if (!verifier.valid()) return false;
        out.fill(0U);
        return true;
    }
};

fermentation::CrossRolePlausibilityContext owningEvidence() {
    fermentation::CrossRolePlausibilityContext evidence;
    evidence.air.quality = device_platform::SensorQuality::Valid;
    evidence.cooling.quality = device_platform::SensorQuality::Valid;
    evidence.product.quality = device_platform::SensorQuality::Valid;
    return evidence;
}

device_platform::HttpRequestMetadata browserMetadata(
    const fermentation::WebSessionResult& session, std::uint64_t sequence) {
    device_platform::HttpRequestMetadata metadata;
    metadata.host = "fermentation.local";
    metadata.contentType = "application/json";
    metadata.cookie = "FSSESSION=" + session.cookieValue;
    metadata.csrfToken = session.csrfToken;
    metadata.mutationSeq = std::to_string(sequence);
    metadata.origin = "http://fermentation.local";
    metadata.secFetchSite = "same-origin";
    return metadata;
}

void test_internal_run_route_preserves_application_owner_and_replay_boundary() {
    device_platform::DevicePlatform platform;
    device_platform_test_support::SimulatedPersistentStateStore store;
    device_platform_test_support::MockTimeZoneResolver resolver;
    device_platform::VirtualTimeSource time;
    fermentation::FermentationApplication application;
    TEST_ASSERT_TRUE(platform.begin({true}));
    TEST_ASSERT_TRUE(application.begin(platform, store, resolver, time));
    application.publishOwningRuntimeEvidence(owningEvidence());

    Random random;
    Kdf kdf;
    fermentation::AuthenticationRecordStore records(store);
    fermentation::AuthenticationDomain authentication(records, kdf, random);
    fermentation::WebSessionManager sessions(random);
    const auto session = sessions.create(0U);
    TEST_ASSERT_TRUE(session.handle.has_value());
    fermentation::WebApplicationRoutes routes(application, authentication,
                                              sessions, time);

    const auto snapshot = application.uiSnapshot();
    const std::string body =
        "{\"action\":\"start_manual_timed\",\"confirmed\":true,"
        "\"expectedStateSequence\":" +
        std::to_string(snapshot.revisions.expectedStateSequence) +
        ",\"targetTemperatureCelsius\":30.0,\"durationMinutes\":60,"
        "\"sensorMode\":\"AIR\",\"preheatEnabled\":false,"
        "\"qualificationBandCelsius\":0.5,"
        "\"qualificationDurationMinutes\":10,"
        "\"maximumTargetReachMinutes\":180,"
        "\"completionMode\":\"FINISH_WITHOUT_COOLING\"}";
    device_platform::HttpResponse first;
    TEST_ASSERT_TRUE(routes.handle(
        {"POST", "/internal/ui/run", body, browserMetadata(session, 1U)},
        first));
    // The application-owned decision remains non-success; the Web route
    // cannot promote it to a mutation success.
    TEST_ASSERT_EQUAL_UINT16(422U, first.statusCode);
    TEST_ASSERT_TRUE(first.body.find("command_rejected") != std::string::npos);

    device_platform::HttpResponse retry;
    TEST_ASSERT_TRUE(routes.handle(
        {"POST", "/internal/ui/run", body, browserMetadata(session, 1U)},
        retry));
    TEST_ASSERT_EQUAL_UINT16(first.statusCode, retry.statusCode);
    TEST_ASSERT_EQUAL_STRING(first.body.c_str(), retry.body.c_str());

    const std::string changed = body.substr(0U, body.find("30.0")) + "31.0" +
                                body.substr(body.find("30.0") + 4U);
    device_platform::HttpResponse conflict;
    TEST_ASSERT_TRUE(routes.handle(
        {"POST", "/internal/ui/run", changed, browserMetadata(session, 1U)},
        conflict));
    TEST_ASSERT_EQUAL_UINT16(409U, conflict.statusCode);
}

}  // namespace

void setup() {}
void loop() {}

int main() {
    UNITY_BEGIN();
    RUN_TEST(
        test_internal_run_route_preserves_application_owner_and_replay_boundary);
    return UNITY_END();
}
