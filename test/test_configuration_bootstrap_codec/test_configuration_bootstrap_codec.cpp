#include <unity.h>

#include <cstdint>
#include <optional>
#include <string>

#include "configuration_bootstrap.hpp"
#include "configuration_bootstrap_codec.hpp"
#include "configuration_limits.hpp"
#include "configuration_storage_contract.hpp"
#include "storage_envelope.hpp"

namespace {

fermentation::ConfigurationBootstrapRecord record(
    std::uint64_t epoch, std::uint64_t sequence,
    fermentation::ConfigurationBootstrapState state,
    std::uint32_t schema =
        fermentation::kConfigurationBootstrapSchemaVersion2) {
    return {fermentation::ConfigurationBootstrapSequence{sequence},
            fermentation::kConfigurationStorageFormatVersion1,
            device_platform::StorageEpoch{epoch}, state, schema};
}

void test_schema1_history_is_closed() {
    constexpr auto schema1 =
        fermentation::kConfigurationBootstrapSchemaVersion1;
    TEST_ASSERT_TRUE(fermentation::isPlausible(record(
        1U, 1U, fermentation::ConfigurationBootstrapState::Initializing,
        schema1)));
    TEST_ASSERT_TRUE(fermentation::isPlausible(record(
        1U, 2U, fermentation::ConfigurationBootstrapState::Initialized,
        schema1)));
    TEST_ASSERT_TRUE(fermentation::isPlausible(record(
        2U, 3U, fermentation::ConfigurationBootstrapState::Resetting,
        schema1)));
    TEST_ASSERT_TRUE(fermentation::isPlausible(record(
        2U, 4U, fermentation::ConfigurationBootstrapState::Initialized,
        schema1)));
    TEST_ASSERT_FALSE(fermentation::isPlausible(record(
        2U, 2U, fermentation::ConfigurationBootstrapState::Initialized,
        schema1)));
    TEST_ASSERT_FALSE(fermentation::isPlausible(record(
        1U, 3U, fermentation::ConfigurationBootstrapState::Resetting,
        schema1)));
}

void test_schema2_accepts_only_the_defined_schema1_migrations() {
    constexpr auto schema1 =
        fermentation::kConfigurationBootstrapSchemaVersion1;
    constexpr auto schema2 =
        fermentation::kConfigurationBootstrapSchemaVersion2;
    const auto schema1Initializing = record(
        1U, 1U, fermentation::ConfigurationBootstrapState::Initializing,
        schema1);
    const auto schema2Initialized = record(
        1U, 2U, fermentation::ConfigurationBootstrapState::Initialized,
        schema2);
    TEST_ASSERT_TRUE(fermentation::isAllowedBootstrapSuccessor(
        schema1Initializing, schema2Initialized));

    const auto schema1Initialized = record(
        3U, 6U, fermentation::ConfigurationBootstrapState::Initialized,
        schema1);
    const auto schema2Resetting = record(
        4U, 7U, fermentation::ConfigurationBootstrapState::Resetting,
        schema2);
    TEST_ASSERT_TRUE(fermentation::isAllowedBootstrapSuccessor(
        schema1Initialized, schema2Resetting));

    const auto schema1Resetting = record(
        3U, 5U, fermentation::ConfigurationBootstrapState::Resetting,
        schema1);
    const auto schema2Pending = fermentation::ConfigurationBootstrapRecord{
        fermentation::ConfigurationBootstrapSequence{6U},
        fermentation::kConfigurationStorageFormatVersion1,
        device_platform::StorageEpoch{3U},
        fermentation::ConfigurationBootstrapState::Initialized, schema2,
        fermentation::RunEpochHandoffState::Pending,
        device_platform::StorageEpoch{2U}, device_platform::StorageEpoch{3U}};
    TEST_ASSERT_TRUE(fermentation::isAllowedBootstrapSuccessor(
        schema1Resetting, schema2Pending));

    TEST_ASSERT_TRUE(fermentation::isAllowedBootstrapSuccessor(
        schema1Initializing,
        record(1U, 2U, fermentation::ConfigurationBootstrapState::Initialized,
               schema1)));
}

void test_schema2_roundtrip_uses_new_wire_size() {
    const auto expected =
        record(2U, 3U, fermentation::ConfigurationBootstrapState::Resetting);
    std::string bytes;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            fermentation::ConfigurationBootstrapCodecStatus::Success),
        static_cast<int>(
            fermentation::encodeConfigurationBootstrapRecord(expected, bytes)));
    TEST_ASSERT_EQUAL_UINT32(43U,
                             bytes.size());
    const auto decoded =
        fermentation::decodeConfigurationBootstrapRecord(bytes);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            fermentation::ConfigurationBootstrapCodecStatus::Success),
        static_cast<int>(decoded.status));
    TEST_ASSERT_TRUE(decoded.value.has_value());
    TEST_ASSERT_TRUE(*decoded.value == expected);
}

void test_crc_and_trailing_bytes_are_rejected() {
    std::string bytes;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            fermentation::ConfigurationBootstrapCodecStatus::Success),
        static_cast<int>(fermentation::encodeConfigurationBootstrapRecord(
            record(1U, 1U,
                   fermentation::ConfigurationBootstrapState::Initializing),
            bytes)));
    bytes.back() ^= 0x01;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            fermentation::ConfigurationBootstrapCodecStatus::InvalidEnvelope),
        static_cast<int>(
            fermentation::decodeConfigurationBootstrapRecord(bytes).status));
}

void test_newer_bootstrap_schema_is_fail_closed_without_partial_value() {
    std::string bytes;
    TEST_ASSERT_TRUE(device_platform::encodeEnvelope(
                         {fermentation::configuration_storage_contract::
                              kConfigurationBootstrapRecordType,
                          3U, device_platform::StorageEpoch{1U}, 1U,
                          std::nullopt, std::string(6U, '\0')},
                         bytes, 64U) ==
                     device_platform::EnvelopeEncodeStatus::Success);
    const auto decoded =
        fermentation::decodeConfigurationBootstrapRecord(bytes);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::ConfigurationBootstrapCodecStatus::
                             UnsupportedNewerSchema),
        static_cast<int>(decoded.status));
    TEST_ASSERT_FALSE(decoded.value.has_value());
}

void test_schema2_bound_handoff_roundtrips_without_utc() {
    const auto expected = fermentation::ConfigurationBootstrapRecord{
        fermentation::ConfigurationBootstrapSequence{4U},
        fermentation::kConfigurationStorageFormatVersion1,
        device_platform::StorageEpoch{2U},
        fermentation::ConfigurationBootstrapState::Initialized,
        fermentation::kConfigurationBootstrapSchemaVersion2,
        fermentation::RunEpochHandoffState::Pending,
        device_platform::StorageEpoch{1U}, device_platform::StorageEpoch{2U}};
    std::string bytes;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::ConfigurationBootstrapCodecStatus::Success),
        static_cast<int>(fermentation::encodeConfigurationBootstrapRecord(
            expected, bytes)));
    TEST_ASSERT_EQUAL_UINT32(fermentation::configuration_limits::
                                 kMaximumConfigurationBootstrapEnvelopeBytes,
                             bytes.size());
    const auto decoded =
        fermentation::decodeConfigurationBootstrapRecord(bytes);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::ConfigurationBootstrapCodecStatus::Success),
        static_cast<int>(decoded.status));
    TEST_ASSERT_TRUE(decoded.value.has_value());
    TEST_ASSERT_TRUE(*decoded.value == expected);
}

void test_test_local_epoch_record_does_not_reinterpret_bootstrap_schema1() {
    std::string bytes;
    TEST_ASSERT_TRUE(
        device_platform::encodeEnvelope(
            {device_platform::RecordTypeId{65000U}, 1U,
             device_platform::StorageEpoch{7U}, 1U, std::nullopt, "test-only"},
            bytes, 128U) == device_platform::EnvelopeEncodeStatus::Success);
    const auto generic = device_platform::decodeEnvelope(bytes);
    TEST_ASSERT_TRUE(generic.status ==
                     device_platform::EnvelopeDecodeStatus::Success);
    TEST_ASSERT_EQUAL_UINT32(7U, generic.envelope->storageEpoch.value());
    const auto bootstrap =
        fermentation::decodeConfigurationBootstrapRecord(bytes);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::ConfigurationBootstrapCodecStatus::
                             RecordIdentityMismatch),
        static_cast<int>(bootstrap.status));
    TEST_ASSERT_FALSE(bootstrap.value.has_value());
}

void test_schema1_rejects_invalid_payload_format_state_and_utc() {
    struct Case {
        std::uint32_t format;
        std::uint8_t state;
        std::size_t payloadSize;
        bool utc;
        fermentation::ConfigurationBootstrapCodecStatus expected;
    };
    const Case cases[]{
        {0U, 1U, 5U, false,
         fermentation::ConfigurationBootstrapCodecStatus::InvalidModel},
        {2U, 1U, 5U, false,
         fermentation::ConfigurationBootstrapCodecStatus::
             UnsupportedNewerSchema},
        {1U, 0U, 5U, false,
         fermentation::ConfigurationBootstrapCodecStatus::InvalidModel},
        {1U, 1U, 4U, false,
         fermentation::ConfigurationBootstrapCodecStatus::InvalidModel},
        {1U, 1U, 6U, false,
         fermentation::ConfigurationBootstrapCodecStatus::InvalidModel},
        {1U, 1U, 5U, true,
         fermentation::ConfigurationBootstrapCodecStatus::InvalidModel}};
    for (const auto& item : cases) {
        std::string payload(item.payloadSize, '\0');
        if (item.payloadSize >= 4U) {
            payload[0] = static_cast<char>((item.format >> 24U) & 0xFFU);
            payload[1] = static_cast<char>((item.format >> 16U) & 0xFFU);
            payload[2] = static_cast<char>((item.format >> 8U) & 0xFFU);
            payload[3] = static_cast<char>(item.format & 0xFFU);
        }
        if (item.payloadSize >= 5U) {
            payload[4] = static_cast<char>(item.state);
        }
        std::string bytes;
        TEST_ASSERT_TRUE(
            device_platform::encodeEnvelope(
                {fermentation::configuration_storage_contract::
                     kConfigurationBootstrapRecordType,
                 1U, device_platform::StorageEpoch{1U}, 1U,
                 item.utc ? std::optional<std::int64_t>{0} : std::nullopt,
                 payload},
                bytes, 64U) == device_platform::EnvelopeEncodeStatus::Success);
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(item.expected),
            static_cast<int>(
                fermentation::decodeConfigurationBootstrapRecord(bytes)
                    .status));
    }
}

}  // namespace

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_schema1_history_is_closed);
    RUN_TEST(test_schema2_accepts_only_the_defined_schema1_migrations);
    RUN_TEST(test_schema2_roundtrip_uses_new_wire_size);
    RUN_TEST(test_crc_and_trailing_bytes_are_rejected);
    RUN_TEST(test_newer_bootstrap_schema_is_fail_closed_without_partial_value);
    RUN_TEST(test_schema2_bound_handoff_roundtrips_without_utc);
    RUN_TEST(
        test_test_local_epoch_record_does_not_reinterpret_bootstrap_schema1);
    RUN_TEST(test_schema1_rejects_invalid_payload_format_state_and_utc);
    return UNITY_END();
}
