#include <unity.h>

#include <cstdint>
#include <string>

#include "configuration_bootstrap.hpp"
#include "configuration_bootstrap_codec.hpp"
#include "configuration_limits.hpp"
#include "storage_envelope.hpp"

namespace {

fermentation::ConfigurationBootstrapRecord record(
    std::uint64_t epoch, std::uint64_t sequence,
    fermentation::ConfigurationBootstrapState state) {
    return {fermentation::ConfigurationBootstrapSequence{sequence},
            fermentation::kConfigurationStorageFormatVersion1,
            device_platform::StorageEpoch{epoch}, state};
}

void test_schema1_history_is_closed() {
    TEST_ASSERT_TRUE(fermentation::isPlausible(record(
        1U, 1U, fermentation::ConfigurationBootstrapState::Initializing)));
    TEST_ASSERT_TRUE(fermentation::isPlausible(record(
        1U, 2U, fermentation::ConfigurationBootstrapState::Initialized)));
    TEST_ASSERT_TRUE(fermentation::isPlausible(
        record(2U, 3U, fermentation::ConfigurationBootstrapState::Resetting)));
    TEST_ASSERT_TRUE(fermentation::isPlausible(record(
        2U, 4U, fermentation::ConfigurationBootstrapState::Initialized)));
    TEST_ASSERT_FALSE(fermentation::isPlausible(record(
        2U, 2U, fermentation::ConfigurationBootstrapState::Initialized)));
    TEST_ASSERT_FALSE(fermentation::isPlausible(
        record(1U, 3U, fermentation::ConfigurationBootstrapState::Resetting)));
}

void test_roundtrip_is_exactly_42_bytes() {
    const auto expected =
        record(2U, 3U, fermentation::ConfigurationBootstrapState::Resetting);
    std::string bytes;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            fermentation::ConfigurationBootstrapCodecStatus::Success),
        static_cast<int>(
            fermentation::encodeConfigurationBootstrapRecord(expected, bytes)));
    TEST_ASSERT_EQUAL_UINT32(fermentation::configuration_limits::
                                 kMaximumConfigurationBootstrapEnvelopeBytes,
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

}  // namespace

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_schema1_history_is_closed);
    RUN_TEST(test_roundtrip_is_exactly_42_bytes);
    RUN_TEST(test_crc_and_trailing_bytes_are_rejected);
    return UNITY_END();
}
