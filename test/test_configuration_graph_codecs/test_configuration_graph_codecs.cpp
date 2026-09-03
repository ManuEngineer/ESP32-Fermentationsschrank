#include <unity.h>

#include <cstdint>
#include <limits>
#include <string>

#include "configuration_graph.hpp"
#include "configuration_graph_codec.hpp"
#include "configuration_limits.hpp"
#include "configuration_storage_contract.hpp"
#include "crc32.hpp"
#include "storage_envelope.hpp"

namespace {

using device_platform::RecordTypeId;
using device_platform::SlotId;
using device_platform::StorageEpoch;
using fermentation::ConfigurationGraphCodecStatus;

fermentation::ConfigurationManifest validManifest(std::uint8_t origin = 2U,
                                                  std::uint8_t operation = 1U) {
    const StorageEpoch epoch{7U};
    return {
        fermentation::decodeChangeOrigin(origin),
        fermentation::decodeChangeOperation(operation),
        {fermentation::configuration_storage_contract::
             kUserConfigurationRecordType,
         SlotId{1U}, fermentation::UserConfigurationRevision{11U}, 1U, 42U,
         0x10203040U, epoch},
        {fermentation::configuration_storage_contract::
             kServiceConfigurationRecordType,
         SlotId{2U}, fermentation::ServiceConfigurationRevision{12U}, 1U, 0U,
         0U, epoch},
        {fermentation::configuration_storage_contract::
             kProgramCatalogRecordType,
         SlotId{3U}, fermentation::ProgramCatalogRevision{13U}, 1U, 512U,
         0xA0B0C0D0U, epoch},
    };
}

fermentation::ConfigurationManifestReference manifestReference(
    std::uint32_t slot, std::uint64_t generation) {
    return {fermentation::configuration_storage_contract::
                kConfigurationManifestRecordType,
            SlotId{slot},
            fermentation::ConfigurationManifestGeneration{generation},
            1U,
            static_cast<std::uint32_t>(fermentation::configuration_limits::
                                           kConfigurationManifestPayloadBytes),
            0x55667788U,
            StorageEpoch{7U}};
}

void test_manifest_payload_is_canonical_and_round_trips() {
    const auto manifest = validManifest();
    std::string bytes = "unchanged";
    TEST_ASSERT_TRUE(
        fermentation::encodeConfigurationManifestPayload(manifest, bytes) ==
        ConfigurationGraphCodecStatus::Success);
    TEST_ASSERT_EQUAL_UINT32(
        fermentation::configuration_limits::kConfigurationManifestPayloadBytes,
        bytes.size());
    TEST_ASSERT_EQUAL_HEX8(2U, static_cast<std::uint8_t>(bytes[0]));
    TEST_ASSERT_EQUAL_HEX8(1U, static_cast<std::uint8_t>(bytes[1]));
    TEST_ASSERT_EQUAL_HEX8(0U, static_cast<std::uint8_t>(bytes[2]));
    TEST_ASSERT_EQUAL_HEX8(1U, static_cast<std::uint8_t>(bytes[3]));
    TEST_ASSERT_EQUAL_HEX8(0U, static_cast<std::uint8_t>(bytes[4]));
    TEST_ASSERT_EQUAL_HEX8(0U, static_cast<std::uint8_t>(bytes[5]));
    TEST_ASSERT_EQUAL_HEX8(0U, static_cast<std::uint8_t>(bytes[6]));
    TEST_ASSERT_EQUAL_HEX8(1U, static_cast<std::uint8_t>(bytes[7]));

    const auto decoded =
        fermentation::decodeConfigurationManifestPayload(bytes);
    TEST_ASSERT_TRUE(decoded.status == ConfigurationGraphCodecStatus::Success);
    TEST_ASSERT_TRUE(decoded.value.has_value());
    TEST_ASSERT_TRUE(*decoded.value == manifest);
}

void test_unknown_change_metadata_is_preserved() {
    const auto manifest = validManifest(0xE1U, 0xF2U);
    std::string bytes;
    TEST_ASSERT_TRUE(
        fermentation::encodeConfigurationManifestPayload(manifest, bytes) ==
        ConfigurationGraphCodecStatus::Success);
    const auto decoded =
        fermentation::decodeConfigurationManifestPayload(bytes);
    TEST_ASSERT_TRUE(decoded.value.has_value());
    TEST_ASSERT_TRUE(decoded.value->origin.kind ==
                     fermentation::ChangeOriginKind::Unknown);
    TEST_ASSERT_EQUAL_HEX8(0xE1U, decoded.value->origin.wireValue);
    TEST_ASSERT_TRUE(decoded.value->operation.kind ==
                     fermentation::ChangeOperationKind::Unknown);
    TEST_ASSERT_EQUAL_HEX8(0xF2U, decoded.value->operation.wireValue);
}

void test_manifest_rejects_truncation_trailing_and_invalid_references() {
    std::string bytes;
    auto manifest = validManifest();
    TEST_ASSERT_TRUE(
        fermentation::encodeConfigurationManifestPayload(manifest, bytes) ==
        ConfigurationGraphCodecStatus::Success);
    auto truncated = bytes.substr(0U, bytes.size() - 1U);
    TEST_ASSERT_TRUE(
        fermentation::decodeConfigurationManifestPayload(truncated).status ==
        ConfigurationGraphCodecStatus::Truncated);
    auto trailing = bytes + "x";
    TEST_ASSERT_TRUE(
        fermentation::decodeConfigurationManifestPayload(trailing).status ==
        ConfigurationGraphCodecStatus::TrailingBytes);

    manifest.programCatalog.slot = SlotId{4U};
    std::string unchanged = "sentinel";
    TEST_ASSERT_TRUE(
        fermentation::encodeConfigurationManifestPayload(manifest, unchanged) ==
        ConfigurationGraphCodecStatus::InvalidModel);
    TEST_ASSERT_EQUAL_STRING_LEN("sentinel", unchanged.data(),
                                 unchanged.size());
}

void test_manifest_rejects_inconsistent_wire_metadata_and_reference_contracts() {
    auto manifest = validManifest();
    manifest.origin.kind = fermentation::ChangeOriginKind::InternalSystem;
    std::string output = "sentinel";
    TEST_ASSERT_TRUE(
        fermentation::encodeConfigurationManifestPayload(manifest, output) ==
        ConfigurationGraphCodecStatus::InvalidModel);

    manifest = validManifest();
    manifest.operation.kind =
        fermentation::ChangeOperationKind::FactoryInitialization;
    TEST_ASSERT_TRUE(
        fermentation::encodeConfigurationManifestPayload(manifest, output) ==
        ConfigurationGraphCodecStatus::InvalidModel);

    manifest = validManifest();
    manifest.userConfiguration.schemaVersion = 3U;
    TEST_ASSERT_TRUE(
        fermentation::encodeConfigurationManifestPayload(manifest, output) ==
        ConfigurationGraphCodecStatus::InvalidModel);

    manifest = validManifest();
    manifest.serviceConfiguration.payloadLength = 1U;
    TEST_ASSERT_TRUE(
        fermentation::encodeConfigurationManifestPayload(manifest, output) ==
        ConfigurationGraphCodecStatus::InvalidModel);

    manifest = validManifest();
    manifest.programCatalog.payloadLength =
        fermentation::configuration_limits::kMaximumProgramCatalogPayloadBytes +
        1U;
    TEST_ASSERT_TRUE(
        fermentation::encodeConfigurationManifestPayload(manifest, output) ==
        ConfigurationGraphCodecStatus::InvalidModel);
    TEST_ASSERT_EQUAL_STRING_LEN("sentinel", output.data(), output.size());
}

void test_root_payload_without_and_with_fallback_round_trips() {
    fermentation::ConfigurationRootRecord root{manifestReference(1U, 9U),
                                               std::nullopt};
    std::string bytes;
    TEST_ASSERT_TRUE(
        fermentation::encodeConfigurationRootPayload(root, bytes) ==
        ConfigurationGraphCodecStatus::Success);
    TEST_ASSERT_EQUAL_UINT32(fermentation::configuration_limits::
                                 kConfigurationRootPayloadWithoutFallbackBytes,
                             bytes.size());
    TEST_ASSERT_EQUAL_HEX8(0U, static_cast<std::uint8_t>(bytes.back()));
    auto decoded = fermentation::decodeConfigurationRootPayload(bytes);
    TEST_ASSERT_TRUE(decoded.value.has_value());
    TEST_ASSERT_TRUE(*decoded.value == root);

    root.fallback = manifestReference(0U, 8U);
    TEST_ASSERT_TRUE(
        fermentation::encodeConfigurationRootPayload(root, bytes) ==
        ConfigurationGraphCodecStatus::Success);
    TEST_ASSERT_EQUAL_UINT32(fermentation::configuration_limits::
                                 kConfigurationRootPayloadWithFallbackBytes,
                             bytes.size());
    decoded = fermentation::decodeConfigurationRootPayload(bytes);
    TEST_ASSERT_TRUE(decoded.value.has_value());
    TEST_ASSERT_TRUE(*decoded.value == root);
}

void test_root_rejects_invalid_optional_tag_trailing_and_equal_branches() {
    fermentation::ConfigurationRootRecord root{manifestReference(1U, 9U),
                                               std::nullopt};
    std::string bytes;
    TEST_ASSERT_TRUE(
        fermentation::encodeConfigurationRootPayload(root, bytes) ==
        ConfigurationGraphCodecStatus::Success);
    bytes.back() = static_cast<char>(2U);
    TEST_ASSERT_TRUE(
        fermentation::decodeConfigurationRootPayload(bytes).status ==
        ConfigurationGraphCodecStatus::InvalidOptionalTag);

    TEST_ASSERT_TRUE(
        fermentation::encodeConfigurationRootPayload(root, bytes) ==
        ConfigurationGraphCodecStatus::Success);
    bytes.push_back('x');
    TEST_ASSERT_TRUE(
        fermentation::decodeConfigurationRootPayload(bytes).status ==
        ConfigurationGraphCodecStatus::TrailingBytes);

    root.fallback = root.active;
    std::string unchanged = "sentinel";
    TEST_ASSERT_TRUE(
        fermentation::encodeConfigurationRootPayload(root, unchanged) ==
        ConfigurationGraphCodecStatus::InvalidModel);
    TEST_ASSERT_EQUAL_STRING_LEN("sentinel", unchanged.data(),
                                 unchanged.size());
}

void test_root_rejects_invalid_manifest_reference_and_same_generation_fallback() {
    auto active = manifestReference(1U, 9U);
    active.schemaVersion = 2U;
    fermentation::ConfigurationRootRecord root{active, std::nullopt};
    std::string output = "sentinel";
    TEST_ASSERT_TRUE(
        fermentation::encodeConfigurationRootPayload(root, output) ==
        ConfigurationGraphCodecStatus::InvalidModel);

    active = manifestReference(1U, 9U);
    active.payloadLength--;
    root = {active, std::nullopt};
    TEST_ASSERT_TRUE(
        fermentation::encodeConfigurationRootPayload(root, output) ==
        ConfigurationGraphCodecStatus::InvalidModel);

    root = {manifestReference(1U, 9U), manifestReference(0U, 9U)};
    TEST_ASSERT_TRUE(
        fermentation::encodeConfigurationRootPayload(root, output) ==
        ConfigurationGraphCodecStatus::InvalidModel);
    TEST_ASSERT_EQUAL_STRING_LEN("sentinel", output.data(), output.size());
}

void test_full_records_use_exact_identity_and_envelope_limits() {
    const auto manifest = validManifest();
    std::string manifestBytes;
    TEST_ASSERT_TRUE(fermentation::encodeConfigurationManifestRecord(
                         manifest,
                         fermentation::ConfigurationManifestGeneration{21U},
                         StorageEpoch{7U}, 1234, manifestBytes) ==
                     ConfigurationGraphCodecStatus::Success);
    TEST_ASSERT_EQUAL_UINT32(fermentation::configuration_limits::
                                 kMaximumConfigurationManifestEnvelopeBytes,
                             manifestBytes.size());
    const auto manifestEnvelope =
        device_platform::decodeEnvelope(manifestBytes);
    TEST_ASSERT_TRUE(manifestEnvelope.envelope.has_value());
    TEST_ASSERT_TRUE(manifestEnvelope.envelope->recordTypeId ==
                     fermentation::configuration_storage_contract::
                         kConfigurationManifestRecordType);
    TEST_ASSERT_EQUAL_UINT64(21U, manifestEnvelope.envelope->versionValue);

    const fermentation::ConfigurationRootRecord root{
        manifestReference(1U, 21U), manifestReference(0U, 20U)};
    std::string rootBytes;
    TEST_ASSERT_TRUE(fermentation::encodeConfigurationRootRecord(
                         root, fermentation::ConfigurationRootSequence{31U},
                         StorageEpoch{7U}, 1234,
                         rootBytes) == ConfigurationGraphCodecStatus::Success);
    TEST_ASSERT_EQUAL_UINT32(fermentation::configuration_limits::
                                 kMaximumConfigurationRootEnvelopeBytes,
                             rootBytes.size());
}

void test_reserved_zero_values_leave_output_unchanged() {
    auto manifest = validManifest();
    manifest.userConfiguration.version =
        fermentation::UserConfigurationRevision{0U};
    std::string output = "old";
    TEST_ASSERT_TRUE(
        fermentation::encodeConfigurationManifestPayload(manifest, output) ==
        ConfigurationGraphCodecStatus::InvalidModel);
    TEST_ASSERT_EQUAL_STRING_LEN("old", output.data(), output.size());

    const fermentation::ConfigurationRootRecord root{manifestReference(0U, 1U),
                                                     std::nullopt};
    TEST_ASSERT_TRUE(fermentation::encodeConfigurationRootRecord(
                         root, fermentation::ConfigurationRootSequence{0U},
                         StorageEpoch{7U}, std::nullopt, output) ==
                     ConfigurationGraphCodecStatus::InvalidModel);
    TEST_ASSERT_EQUAL_STRING_LEN("old", output.data(), output.size());
}

}  // namespace

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_manifest_payload_is_canonical_and_round_trips);
    RUN_TEST(test_unknown_change_metadata_is_preserved);
    RUN_TEST(test_manifest_rejects_truncation_trailing_and_invalid_references);
    RUN_TEST(
        test_manifest_rejects_inconsistent_wire_metadata_and_reference_contracts);
    RUN_TEST(test_root_payload_without_and_with_fallback_round_trips);
    RUN_TEST(
        test_root_rejects_invalid_optional_tag_trailing_and_equal_branches);
    RUN_TEST(
        test_root_rejects_invalid_manifest_reference_and_same_generation_fallback);
    RUN_TEST(test_full_records_use_exact_identity_and_envelope_limits);
    RUN_TEST(test_reserved_zero_values_leave_output_unchanged);
    return UNITY_END();
}
