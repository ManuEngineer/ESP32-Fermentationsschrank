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

void test_web_run_command_decoder_keeps_intents_and_revisions_typed() {
    fermentation::WebUiRunCommand command;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::WebApiCodecStatus::Success),
        static_cast<int>(fermentation::decodeWebUiRunCommand(
            R"({"action":"start_manual_timed","confirmed":true,"expectedStateSequence":4,"expectedRunRevision":0,"targetTemperatureCelsius":28.5,"durationMinutes":90,"sensorMode":"PRODUCT","preheatEnabled":true,"qualificationBandCelsius":0.5,"qualificationDurationMinutes":10,"maximumTargetReachMinutes":180,"completionMode":"FINISH_WITHOUT_COOLING"})",
            command)));
    TEST_ASSERT_TRUE(command.confirmed);
    TEST_ASSERT_EQUAL_UINT32(4U, command.expected.expectedStateSequence);
    TEST_ASSERT_TRUE(command.expected.expectedRunRevision.has_value());
    TEST_ASSERT_TRUE(std::holds_alternative<
                     fermentation::FermentationUiStartManualTimedIntent>(
        command.payload));
    const auto& timed = std::get<fermentation::FermentationUiStartManualTimedIntent>(
        command.payload);
    TEST_ASSERT_EQUAL_UINT32(90U, timed.values.durationMinutes);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::RunSensorMode::Product),
        static_cast<int>(timed.values.sensorMode));
    TEST_ASSERT_TRUE(timed.values.preheatEnabled);
}

void test_web_run_command_decoder_rejects_missing_confirmation_or_bounds() {
    fermentation::WebUiRunCommand command;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::WebApiCodecStatus::MissingField),
        static_cast<int>(fermentation::decodeWebUiRunCommand(
            R"({"action":"stop","expectedStateSequence":1,"option":"back"})",
            command)));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::WebApiCodecStatus::WrongType),
        static_cast<int>(fermentation::decodeWebUiRunCommand(
            R"({"action":"stop","confirmed":false,"expectedStateSequence":1,"option":"bad"})",
            command)));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::WebApiCodecStatus::CapacityExceeded),
        static_cast<int>(fermentation::decodeWebUiRunCommand(
            std::string(4097U, 'x'), command)));
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
    RUN_TEST(test_web_run_command_decoder_keeps_intents_and_revisions_typed);
    RUN_TEST(test_web_run_command_decoder_rejects_missing_confirmation_or_bounds);
    return UNITY_END();
}
