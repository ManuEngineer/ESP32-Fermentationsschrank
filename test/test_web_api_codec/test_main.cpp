#include <optional>
#include <string>

#include <unity.h>

#include "web_api_codec.hpp"

namespace {

void test_snapshot_encoding_is_bounded_and_secret_free() {
    fermentation::FermentationUiSnapshot snapshot;
    snapshot.network.currentMode =
        device_platform::NetworkMode::HOME_WIFI;
    snapshot.network.selectionRequired = false;
    snapshot.revisions.expectedUserConfigurationRevision =
        fermentation::UserConfigurationRevision{7U};
    snapshot.refreshRevision = device_platform::UiRefreshRevision{3U};

    const fermentation::MutationSequenceView sequence{
        fermentation::MutationSequenceState::Available, 9U, std::nullopt};
    std::string encoded;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::WebApiCodecStatus::Success),
        static_cast<int>(fermentation::encodeUiSnapshot(
            snapshot, sequence, true, std::string{"0123456789abcdef0123456789abcdef"},
            encoded)));
    TEST_ASSERT_TRUE(encoded.find("\"webPasswordEnabled\":true") !=
                     std::string::npos);
    TEST_ASSERT_TRUE(encoded.find("\"nextMutationSeq\":9") !=
                     std::string::npos);
    TEST_ASSERT_TRUE(encoded.find("\"expectedUserConfigurationRevision\":7") !=
                     std::string::npos);
    TEST_ASSERT_TRUE(encoded.find("\"csrfToken\":\"0123456789abcdef0123456789abcdef\"") !=
                     std::string::npos);
    TEST_ASSERT_TRUE(encoded.find("credential") == std::string::npos);
}

void test_network_mode_decoder_keeps_only_user_modes_and_revision() {
    device_platform::NetworkMode mode =
        device_platform::NetworkMode::UNSELECTED;
    std::optional<fermentation::UserConfigurationRevision> revision;

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::WebApiCodecStatus::Success),
        static_cast<int>(fermentation::decodeNetworkMode(
            R"({"mode":"HOME_WIFI","expectedUserConfigurationRevision":7})",
            mode, revision)));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(device_platform::NetworkMode::HOME_WIFI),
        static_cast<int>(mode));
    TEST_ASSERT_TRUE(revision.has_value());
    TEST_ASSERT_EQUAL_UINT64(7U, revision->value());

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::WebApiCodecStatus::WrongType),
        static_cast<int>(fermentation::decodeNetworkMode(
            R"({"mode":"UNSELECTED"})", mode, revision)));
}

void test_boolean_and_credential_decoders_enforce_types_and_bounds() {
    bool enabled = false;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::WebApiCodecStatus::Success),
        static_cast<int>(fermentation::decodeBooleanField(
            R"({"enabled":true})", "enabled", enabled)));
    TEST_ASSERT_TRUE(enabled);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::WebApiCodecStatus::WrongType),
        static_cast<int>(fermentation::decodeBooleanField(
            R"({"enabled":"true"})", "enabled", enabled)));

    std::string credential;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::WebApiCodecStatus::Success),
        static_cast<int>(fermentation::decodeCredentialField(
            R"({"password":"secret"})", "password", 64U, credential)));
    TEST_ASSERT_EQUAL_STRING("secret", credential.c_str());
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::WebApiCodecStatus::CapacityExceeded),
        static_cast<int>(fermentation::decodeCredentialField(
            R"({"password":"secret"})", "password", 3U, credential)));
}

void test_json_negative_and_redaction_boundaries_are_bounded() {
    bool enabled = false;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::WebApiCodecStatus::InvalidJson),
        static_cast<int>(fermentation::decodeBooleanField(
            R"({"enabled":true)", "enabled", enabled)));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::WebApiCodecStatus::CapacityExceeded),
        static_cast<int>(fermentation::decodeBooleanField(
            std::string(4097U, 'x'), "enabled", enabled)));

    fermentation::FermentationUiSnapshot snapshot;
    snapshot.network.currentMode = device_platform::NetworkMode::AP_ONLY;
    std::string encoded;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::WebApiCodecStatus::Success),
        static_cast<int>(fermentation::encodeStatus(snapshot, true, encoded)));
    TEST_ASSERT_TRUE(encoded.find("password") == std::string::npos);
    TEST_ASSERT_TRUE(encoded.find("servicePin") == std::string::npos);
    TEST_ASSERT_TRUE(encoded.find("csrfToken") == std::string::npos);
}

}  // namespace

void setup() {}
void loop() {}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_snapshot_encoding_is_bounded_and_secret_free);
    RUN_TEST(test_network_mode_decoder_keeps_only_user_modes_and_revision);
    RUN_TEST(test_boolean_and_credential_decoders_enforce_types_and_bounds);
    RUN_TEST(test_json_negative_and_redaction_boundaries_are_bounded);
    return UNITY_END();
}
