#include <unity.h>

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <new>
#include <optional>
#include <string>
#include <utility>

#include "configuration_document_codec.hpp"
#include "configuration_document_codec_internal.hpp"
#include "configuration_limits.hpp"
#include "configuration_migration.hpp"
#include "configuration_storage_contract.hpp"
#include "big_endian_codec.hpp"
#include "byte_buffer.hpp"
#include "mock_time_zone_resolver.hpp"
#include "standard_program_catalog.hpp"
#include "storage_envelope.hpp"

namespace {
std::size_t gLiveAllocBytes = 0U;
std::size_t gPeakAllocBytes = 0U;
constexpr std::size_t kAllocHeader = alignof(std::max_align_t);
}  // namespace

void* operator new(std::size_t size) {
    void* raw = std::malloc(size + kAllocHeader);
    if (raw == nullptr) {
        throw std::bad_alloc();
    }
    *static_cast<std::size_t*>(raw) = size;
    gLiveAllocBytes += size;
    if (gLiveAllocBytes > gPeakAllocBytes) {
        gPeakAllocBytes = gLiveAllocBytes;
    }
    return static_cast<char*>(raw) + kAllocHeader;
}

void operator delete(void* ptr) noexcept {
    if (ptr == nullptr) {
        return;
    }
    void* raw = static_cast<char*>(ptr) - kAllocHeader;
    gLiveAllocBytes -= *static_cast<std::size_t*>(raw);
    std::free(raw);
}

void operator delete(void* ptr, std::size_t) noexcept { operator delete(ptr); }

namespace {

using device_platform::EnvelopeEncodeStatus;
using device_platform::StorageEnvelope;
using device_platform::StorageEpoch;
using device_platform_test_support::MockTimeZoneResolver;
using fermentation::ConfigurationCodecStatus;
using fermentation::ProgramCatalog;
using fermentation::UserConfiguration;

UserConfiguration validUserConfiguration() {
    return {"de", "Europe/Zurich", "Fermentationsschrank"};
}

fermentation::ProgramDocument userProgram(std::size_t index,
                                          std::size_t noteBytes = 0U) {
    auto copy = fermentation::FactoryProgramCatalog::makeUserCopy(
        "water-kefir", "user-" + std::to_string(index), "Benutzerprogramm");
    TEST_ASSERT_TRUE(copy.has_value());
    copy->program.notes.clear();
    for (std::size_t byte = 0U; byte + 1U < noteBytes; byte += 2U) {
        copy->program.notes += "\xC3\xA4";
    }
    if ((noteBytes % 2U) != 0U) {
        copy->program.notes.push_back(static_cast<char>('a' + index % 26U));
    }
    return std::move(*copy);
}

ProgramCatalog largeCatalog() {
    auto catalog = fermentation::makeFactoryProgramCatalog();
    for (std::size_t index = 0U; index < 12U; ++index) {
        catalog.programs.push_back(userProgram(index, 1024U));
    }
    return catalog;
}

std::string repeatedUmlaut(std::size_t scalarCount) {
    std::string value;
    value.reserve(scalarCount * 2U);
    for (std::size_t index = 0U; index < scalarCount; ++index) {
        value += "\xC3\xA4";
    }
    return value;
}

void maximizeProgramPayload(fermentation::ProgramDocument& document) {
    auto& program = document.program;
    program.name = repeatedUmlaut(48U);
    program.notes = repeatedUmlaut(512U);
    program.preheat = true;
    program.sensorPreference = fermentation::SensorPreference::AirOnly;
    program.productSensorFailure.policy =
        fermentation::ProductSensorFailurePolicy::FallbackToAirAfterTimeout;
    program.productSensorFailure.fallbackDelaySeconds = 0U;
    program.fermentationStages.front().targetTemperatureCelsius = 20.0;
    program.fermentationStages.front().durationMinutes = 60U;
    program.targetQualification.bandCelsius = 0.5;
    program.targetQualification.durationMinutes = 10U;
    program.maximumTargetReachMinutes = 60U;
    program.maximumProductWaitMinutes = 60U;
    program.completion.mode =
        fermentation::CompletionMode::CoolAndHoldForDuration;
    program.completion.coolingTargetCelsius = 4.0;
    program.completion.holdDurationMinutes = 60U;
}

ProgramCatalog maximumValidCatalog() {
    auto catalog = fermentation::makeFactoryProgramCatalog();
    for (auto& document : catalog.programs) {
        maximizeProgramPayload(document);
    }
    for (std::size_t index = 0U; index < 12U; ++index) {
        const auto suffix = std::to_string(index);
        const std::size_t padding = 48U - 5U - suffix.size();
        auto document = userProgram(index);
        document.program.id = "user-" + std::string(padding, 'a') + suffix;
        maximizeProgramPayload(document);
        catalog.programs.push_back(std::move(document));
    }
    return catalog;
}

std::string bytesFromHex(const std::string& hex) {
    std::string bytes;
    bytes.reserve(hex.size() / 2U);
    for (std::size_t index = 0U; index < hex.size(); index += 2U) {
        bytes.push_back(
            static_cast<char>(std::stoul(hex.substr(index, 2U), nullptr, 16)));
    }
    return bytes;
}

bool skipOptional(device_platform::ByteReader& reader, std::size_t valueBytes) {
    bool present = false;
    if (!device_platform::big_endian::readOptionalTag(reader, present)) {
        return false;
    }
    if (!present) {
        return true;
    }
    std::string ignored(valueBytes, '\0');
    return reader.readBytes(ignored.data(), ignored.size());
}

std::string schemaFourCatalogFixture(const std::string& current) {
    device_platform::ByteReader reader(current);
    std::uint8_t count = 0U;
    TEST_ASSERT_TRUE(device_platform::big_endian::readUint8(reader, count));
    std::vector<std::size_t> schemas;
    std::vector<std::size_t> masks;
    std::vector<std::size_t> productWaitTags;
    for (std::uint8_t program = 0U; program < count; ++program) {
        schemas.push_back(reader.position());
        std::uint32_t schema = 0U;
        TEST_ASSERT_TRUE(
            device_platform::big_endian::readUint32(reader, schema));
        masks.push_back(reader.position());
        std::uint64_t mask = 0U;
        TEST_ASSERT_TRUE(device_platform::big_endian::readUint64(reader, mask));
        for (std::size_t stringIndex = 0U; stringIndex < 3U; ++stringIndex) {
            std::uint16_t length = 0U;
            TEST_ASSERT_TRUE(
                device_platform::big_endian::readUint16(reader, length));
            std::string ignored(length, '\0');
            TEST_ASSERT_TRUE(reader.readBytes(ignored.data(), ignored.size()));
        }
        std::uint8_t ignoredByte = 0U;
        for (std::size_t byte = 0U; byte < 9U; ++byte) {
            TEST_ASSERT_TRUE(
                device_platform::big_endian::readUint8(reader, ignoredByte));
        }
        TEST_ASSERT_TRUE(skipOptional(reader, 4U));
        std::uint8_t stages = 0U;
        TEST_ASSERT_TRUE(
            device_platform::big_endian::readUint8(reader, stages));
        for (std::uint8_t stage = 0U; stage < stages; ++stage) {
            TEST_ASSERT_TRUE(skipOptional(reader, 8U));
            TEST_ASSERT_TRUE(skipOptional(reader, 4U));
        }
        TEST_ASSERT_TRUE(skipOptional(reader, 8U));
        TEST_ASSERT_TRUE(skipOptional(reader, 4U));
        TEST_ASSERT_TRUE(skipOptional(reader, 4U));
        productWaitTags.push_back(reader.position());
        TEST_ASSERT_TRUE(skipOptional(reader, 4U));
        TEST_ASSERT_TRUE(
            device_platform::big_endian::readUint8(reader, ignoredByte));
        TEST_ASSERT_TRUE(skipOptional(reader, 8U));
        TEST_ASSERT_TRUE(skipOptional(reader, 4U));
    }
    TEST_ASSERT_EQUAL_UINT32(0U, reader.remaining());

    std::string legacy = current;
    for (const auto offset : schemas) {
        legacy[offset + 3U] =
            static_cast<char>(fermentation::kMigratableProgramSchemaVersion);
    }
    for (const auto offset : masks) {
        const auto mask = fermentation::kSchema4RequiredProgramFields;
        for (std::size_t byte = 0U; byte < 8U; ++byte) {
            legacy[offset + byte] = static_cast<char>(
                mask >> static_cast<unsigned>((7U - byte) * 8U));
        }
    }
    for (auto iterator = productWaitTags.rbegin();
         iterator != productWaitTags.rend(); ++iterator) {
        TEST_ASSERT_EQUAL_UINT8(0U,
                                static_cast<std::uint8_t>(legacy[*iterator]));
        legacy.erase(*iterator, 1U);
    }
    return legacy;
}

void test_sensor_preference_wire_ids_are_explicit_in_both_directions() {
    using fermentation::SensorPreference;
    using namespace fermentation::configuration_codec_internal;
    const std::pair<SensorPreference, std::uint8_t> cases[] = {
        {SensorPreference::ProductIfAvailableElseAir, 1U},
        {SensorPreference::AirProductOptional, 2U},
        {SensorPreference::ProductRequired, 3U},
        {SensorPreference::AirOnly, 4U},
    };
    for (const auto& [value, expectedWireId] : cases) {
        std::uint8_t wireId = 0U;
        TEST_ASSERT_TRUE(sensorPreferenceToWireId(value, wireId));
        TEST_ASSERT_EQUAL_UINT8(expectedWireId, wireId);
        SensorPreference decoded = SensorPreference::AirOnly;
        TEST_ASSERT_TRUE(sensorPreferenceFromWireId(expectedWireId, decoded));
        TEST_ASSERT_TRUE(decoded == value);
    }
    SensorPreference unchanged = SensorPreference::ProductRequired;
    TEST_ASSERT_FALSE(sensorPreferenceFromWireId(0U, unchanged));
    TEST_ASSERT_TRUE(unchanged == SensorPreference::ProductRequired);
    TEST_ASSERT_FALSE(sensorPreferenceFromWireId(5U, unchanged));
    TEST_ASSERT_TRUE(unchanged == SensorPreference::ProductRequired);
}

void test_failure_policy_wire_ids_are_explicit_in_both_directions() {
    using fermentation::ProductSensorFailurePolicy;
    using namespace fermentation::configuration_codec_internal;
    const std::pair<ProductSensorFailurePolicy, std::uint8_t> cases[] = {
        {ProductSensorFailurePolicy::FallbackToAirAfterTimeout, 1U},
        {ProductSensorFailurePolicy::WaitForUser, 2U},
        {ProductSensorFailurePolicy::StopToSafeState, 3U},
    };
    for (const auto& [value, expectedWireId] : cases) {
        std::uint8_t wireId = 0U;
        TEST_ASSERT_TRUE(productSensorFailurePolicyToWireId(value, wireId));
        TEST_ASSERT_EQUAL_UINT8(expectedWireId, wireId);
        ProductSensorFailurePolicy decoded =
            ProductSensorFailurePolicy::StopToSafeState;
        TEST_ASSERT_TRUE(
            productSensorFailurePolicyFromWireId(expectedWireId, decoded));
        TEST_ASSERT_TRUE(decoded == value);
    }
    ProductSensorFailurePolicy unchanged =
        ProductSensorFailurePolicy::WaitForUser;
    TEST_ASSERT_FALSE(productSensorFailurePolicyFromWireId(0U, unchanged));
    TEST_ASSERT_TRUE(unchanged == ProductSensorFailurePolicy::WaitForUser);
    TEST_ASSERT_FALSE(productSensorFailurePolicyFromWireId(4U, unchanged));
    TEST_ASSERT_TRUE(unchanged == ProductSensorFailurePolicy::WaitForUser);
}

void test_completion_mode_wire_ids_are_explicit_in_both_directions() {
    using fermentation::CompletionMode;
    using namespace fermentation::configuration_codec_internal;
    const std::pair<CompletionMode, std::uint8_t> cases[] = {
        {CompletionMode::FinishWithoutCooling, 1U},
        {CompletionMode::CoolThenFinish, 2U},
        {CompletionMode::CoolAndHoldForDuration, 3U},
        {CompletionMode::CoolAndHoldUntilManualStop, 4U},
    };
    for (const auto& [value, expectedWireId] : cases) {
        std::uint8_t wireId = 0U;
        TEST_ASSERT_TRUE(completionModeToWireId(value, wireId));
        TEST_ASSERT_EQUAL_UINT8(expectedWireId, wireId);
        CompletionMode decoded = CompletionMode::FinishWithoutCooling;
        TEST_ASSERT_TRUE(completionModeFromWireId(expectedWireId, decoded));
        TEST_ASSERT_TRUE(decoded == value);
    }
    CompletionMode unchanged = CompletionMode::CoolThenFinish;
    TEST_ASSERT_FALSE(completionModeFromWireId(0U, unchanged));
    TEST_ASSERT_TRUE(unchanged == CompletionMode::CoolThenFinish);
    TEST_ASSERT_FALSE(completionModeFromWireId(5U, unchanged));
    TEST_ASSERT_TRUE(unchanged == CompletionMode::CoolThenFinish);
}

void test_user_configuration_payload_golden_bytes_and_round_trip() {
    MockTimeZoneResolver resolver;
    std::string encoded;
    TEST_ASSERT_TRUE(fermentation::encodeUserConfigurationPayload(
                         validUserConfiguration(), resolver, encoded) ==
                     ConfigurationCodecStatus::Success);
    const auto expected = bytesFromHex(
        "00026465000d4575726f70652f5a757269636800144665726d656e746174696f6e7373"
        "636872616e6b");
    TEST_ASSERT_EQUAL_UINT32(expected.size(), encoded.size());
    TEST_ASSERT_EQUAL_MEMORY(expected.data(), encoded.data(), expected.size());

    const auto decoded =
        fermentation::decodeUserConfigurationPayload(1U, expected, resolver);
    TEST_ASSERT_TRUE(decoded.status == ConfigurationCodecStatus::Success);
    TEST_ASSERT_TRUE(decoded.document.has_value());
    std::string reencoded;
    TEST_ASSERT_TRUE(fermentation::encodeUserConfigurationPayload(
                         *decoded.document, resolver, reencoded) ==
                     ConfigurationCodecStatus::Success);
    TEST_ASSERT_EQUAL_MEMORY(expected.data(), reencoded.data(),
                             expected.size());
}

void test_user_configuration_full_envelope_golden_bytes_has_no_old_fields() {
    MockTimeZoneResolver resolver;
    std::string payload;
    TEST_ASSERT_TRUE(fermentation::encodeUserConfigurationPayload(
                         validUserConfiguration(), resolver, payload) ==
                     ConfigurationCodecStatus::Success);
    StorageEnvelope envelope;
    envelope.recordTypeId = fermentation::configuration_storage_contract::
        kUserConfigurationRecordType;
    envelope.schemaVersion = 1U;
    envelope.storageEpoch = StorageEpoch(1U);
    envelope.versionValue = 1U;
    envelope.payload = payload;
    std::string encoded;
    TEST_ASSERT_TRUE(device_platform::encodeEnvelope(envelope, encoded, 512U) ==
                     EnvelopeEncodeStatus::Success);
    const auto expected = bytesFromHex(
        "445052460001000100000001000000000000000100000000000000010000002900383b"
        "bfb100026465000d4575726f70652f5a757269636800144665726d656e746174696f6e"
        "7373636872616e6b");
    TEST_ASSERT_EQUAL_UINT32(78U, encoded.size());
    TEST_ASSERT_EQUAL_MEMORY(expected.data(), encoded.data(), expected.size());
    TEST_ASSERT_EQUAL_UINT32(37U, encoded.size() - payload.size());
}

void test_user_codec_rejects_missing_extra_and_oversized_payloads() {
    MockTimeZoneResolver resolver;
    std::string encoded;
    TEST_ASSERT_TRUE(fermentation::encodeUserConfigurationPayload(
                         validUserConfiguration(), resolver, encoded) ==
                     ConfigurationCodecStatus::Success);
    auto truncated = encoded.substr(0U, encoded.size() - 1U);
    TEST_ASSERT_TRUE(
        fermentation::decodeUserConfigurationPayload(1U, truncated, resolver)
            .status == ConfigurationCodecStatus::Truncated);
    auto extended = encoded + "x";
    TEST_ASSERT_TRUE(
        fermentation::decodeUserConfigurationPayload(1U, extended, resolver)
            .status == ConfigurationCodecStatus::TrailingBytes);
    TEST_ASSERT_TRUE(fermentation::decodeUserConfigurationPayload(
                         1U, std::string(257U, 'x'), resolver)
                         .status == ConfigurationCodecStatus::CapacityExceeded);
    TEST_ASSERT_TRUE(
        fermentation::decodeUserConfigurationPayload(2U, encoded, resolver)
            .status == ConfigurationCodecStatus::UnsupportedSchema);
}

void test_payload_capacity_boundaries_are_independent_from_field_validation() {
    MockTimeZoneResolver resolver;
    for (const std::size_t size : {255U, 256U}) {
        TEST_ASSERT_FALSE(fermentation::decodeUserConfigurationPayload(
                              1U, std::string(size, 'x'), resolver)
                              .status ==
                          ConfigurationCodecStatus::CapacityExceeded);
    }
    TEST_ASSERT_TRUE(fermentation::decodeUserConfigurationPayload(
                         1U, std::string(257U, 'x'), resolver)
                         .status == ConfigurationCodecStatus::CapacityExceeded);
    for (const std::size_t size : {32767U, 32768U}) {
        TEST_ASSERT_FALSE(fermentation::decodeProgramCatalogPayload(
                              1U, std::string(size, 'x'))
                              .status ==
                          ConfigurationCodecStatus::CapacityExceeded);
    }
    TEST_ASSERT_TRUE(
        fermentation::decodeProgramCatalogPayload(1U, std::string(32769U, 'x'))
            .status == ConfigurationCodecStatus::CapacityExceeded);
}

void test_invalid_user_encode_leaves_output_unchanged() {
    MockTimeZoneResolver resolver;
    auto invalid = validUserConfiguration();
    invalid.deviceName = " bad";
    std::string output = "old";
    TEST_ASSERT_TRUE(fermentation::encodeUserConfigurationPayload(
                         invalid, resolver, output) ==
                     ConfigurationCodecStatus::InvalidDocument);
    TEST_ASSERT_EQUAL_STRING("old", output.c_str());
}

void test_service_configuration_is_exactly_empty() {
    std::string output = "old";
    TEST_ASSERT_TRUE(fermentation::encodeServiceConfigurationPayload(
                         fermentation::ServiceConfiguration{}, output) ==
                     ConfigurationCodecStatus::Success);
    TEST_ASSERT_TRUE(output.empty());
    TEST_ASSERT_TRUE(
        fermentation::decodeServiceConfigurationPayload(1U, "").status ==
        ConfigurationCodecStatus::Success);
    TEST_ASSERT_TRUE(
        fermentation::decodeServiceConfigurationPayload(1U, "x").status ==
        ConfigurationCodecStatus::TrailingBytes);
    TEST_ASSERT_TRUE(
        fermentation::decodeServiceConfigurationPayload(0U, "").status ==
        ConfigurationCodecStatus::UnsupportedSchema);
}

void test_program_catalog_round_trip_is_deterministic_and_preserves_notes() {
    auto catalog = fermentation::makeFactoryProgramCatalog();
    auto user = userProgram(1U);
    user.program.notes = "Zeile 1\nZeile 2";
    user.program.installed = false;
    user.program.enabled = false;
    catalog.programs.push_back(user);
    std::string encoded;
    TEST_ASSERT_TRUE(
        fermentation::encodeProgramCatalogPayload(catalog, encoded) ==
        ConfigurationCodecStatus::Success);
    const auto decoded = fermentation::decodeProgramCatalogPayload(1U, encoded);
    TEST_ASSERT_TRUE(decoded.status == ConfigurationCodecStatus::Success);
    TEST_ASSERT_EQUAL_STRING(
        "Zeile 1\nZeile 2",
        decoded.document->programs[4].program.notes.c_str());
    TEST_ASSERT_FALSE(decoded.document->programs[4].program.installed);
    TEST_ASSERT_FALSE(decoded.document->programs[4].program.enabled);
    std::string reencoded;
    TEST_ASSERT_TRUE(fermentation::encodeProgramCatalogPayload(
                         *decoded.document, reencoded) ==
                     ConfigurationCodecStatus::Success);
    TEST_ASSERT_EQUAL_UINT32(encoded.size(), reencoded.size());
    TEST_ASSERT_EQUAL_MEMORY(encoded.data(), reencoded.data(), encoded.size());
}

void test_program_catalog_factory_payload_has_fixed_golden_bytes() {
    const auto catalog = fermentation::makeFactoryProgramCatalog();
    std::string encoded;
    TEST_ASSERT_TRUE(
        fermentation::encodeProgramCatalogPayload(catalog, encoded) ==
        ConfigurationCodecStatus::Success);
    const auto expected = bytesFromHex(
        "0400000005000000000000ffff000b796f677572742d6d696c64000c4a6f6768757274"
        "206d696c64000001010101010101010100010000000000000400000000000500000000"
        "0000ffff000b796f677572742d6669726d00114a6f6768757274207374696368666573"
        "74"
        "0000010101010101010101000100000000000004000000000005000000000000ffff00"
        "0a6d696c6b2d6b65666972000a4d696c63686b65666972000001010101010100020100"
        "0100000000000004000000000005000000000000ffff000b77617465722d6b65666972"
        "000b5761737365726b656669720000010101010101000201000100000000000001000"
        "0");
    TEST_ASSERT_EQUAL_UINT32(expected.size(), encoded.size());
    TEST_ASSERT_EQUAL_MEMORY(expected.data(), encoded.data(), expected.size());
}

void test_program_catalog_rejects_truncated_trailing_and_capacity_payloads() {
    auto catalog = fermentation::makeFactoryProgramCatalog();
    std::string encoded;
    TEST_ASSERT_TRUE(
        fermentation::encodeProgramCatalogPayload(catalog, encoded) ==
        ConfigurationCodecStatus::Success);
    TEST_ASSERT_TRUE(fermentation::decodeProgramCatalogPayload(
                         1U, encoded.substr(0U, encoded.size() - 1U))
                         .status == ConfigurationCodecStatus::Truncated);
    TEST_ASSERT_TRUE(
        fermentation::decodeProgramCatalogPayload(1U, encoded + "x").status ==
        ConfigurationCodecStatus::TrailingBytes);
    TEST_ASSERT_TRUE(
        fermentation::decodeProgramCatalogPayload(1U, std::string(32769U, 'x'))
            .status == ConfigurationCodecStatus::CapacityExceeded);
}

void test_program_catalog_rejects_unknown_schema_mask_and_enum() {
    auto catalog = fermentation::makeFactoryProgramCatalog();
    std::string encoded;
    TEST_ASSERT_TRUE(
        fermentation::encodeProgramCatalogPayload(catalog, encoded) ==
        ConfigurationCodecStatus::Success);
    auto futureSchema = encoded;
    futureSchema[1] = 0;
    futureSchema[2] = 0;
    futureSchema[3] = 0;
    futureSchema[4] = 6;
    TEST_ASSERT_TRUE(
        fermentation::decodeProgramCatalogPayload(1U, futureSchema).status ==
        ConfigurationCodecStatus::UnsupportedSchema);

    auto unknownMask = encoded;
    unknownMask[5] = static_cast<char>(0x80U);
    TEST_ASSERT_TRUE(
        fermentation::decodeProgramCatalogPayload(1U, unknownMask).status ==
        ConfigurationCodecStatus::InvalidWireValue);

    auto missingRequiredBit = encoded;
    missingRequiredBit[12] = static_cast<char>(
        static_cast<unsigned char>(missingRequiredBit[12]) & 0xFEU);
    TEST_ASSERT_TRUE(
        fermentation::decodeProgramCatalogPayload(1U, missingRequiredBit)
            .status == ConfigurationCodecStatus::InvalidWireValue);

    auto invalidEnum = encoded;
    invalidEnum[49] = static_cast<char>(0xFFU);
    TEST_ASSERT_TRUE(
        fermentation::decodeProgramCatalogPayload(1U, invalidEnum).status ==
        ConfigurationCodecStatus::InvalidWireValue);

    auto invalidBool = encoded;
    invalidBool[42] = static_cast<char>(0x02U);
    TEST_ASSERT_TRUE(
        fermentation::decodeProgramCatalogPayload(1U, invalidBool).status ==
        ConfigurationCodecStatus::InvalidWireValue);

    auto invalidOptional = encoded;
    invalidOptional[51] = static_cast<char>(0x02U);
    TEST_ASSERT_TRUE(
        fermentation::decodeProgramCatalogPayload(1U, invalidOptional).status ==
        ConfigurationCodecStatus::InvalidWireValue);
}

void test_program_count_boundaries_are_checked_before_allocation() {
    TEST_ASSERT_TRUE(fermentation::decodeProgramCatalogPayload(
                         1U, std::string(1U, static_cast<char>(3U)))
                         .status == ConfigurationCodecStatus::InvalidWireValue);
    TEST_ASSERT_TRUE(fermentation::decodeProgramCatalogPayload(
                         1U, std::string(1U, static_cast<char>(17U)))
                         .status == ConfigurationCodecStatus::InvalidWireValue);
}

void test_schema_four_programs_migrate_to_schema_five_deterministically() {
    const auto catalog = fermentation::makeFactoryProgramCatalog();
    std::string current;
    TEST_ASSERT_TRUE(
        fermentation::encodeProgramCatalogPayload(catalog, current) ==
        ConfigurationCodecStatus::Success);
    const auto legacy = schemaFourCatalogFixture(current);
    TEST_ASSERT_TRUE(legacy.size() + 4U == current.size());
    const auto decoded = fermentation::decodeProgramCatalogPayload(1U, legacy);
    TEST_ASSERT_TRUE(decoded.status == ConfigurationCodecStatus::Success);
    for (const auto& document : decoded.document->programs) {
        TEST_ASSERT_EQUAL_UINT32(fermentation::kCurrentProgramSchemaVersion,
                                 document.schema.version);
    }
    std::string reencoded;
    TEST_ASSERT_TRUE(fermentation::encodeProgramCatalogPayload(
                         *decoded.document, reencoded) ==
                     ConfigurationCodecStatus::Success);
    TEST_ASSERT_EQUAL_UINT32(current.size(), reencoded.size());
    TEST_ASSERT_EQUAL_MEMORY(current.data(), reencoded.data(), current.size());
}

void test_invalid_catalog_encode_leaves_output_unchanged() {
    auto invalid = fermentation::makeFactoryProgramCatalog();
    invalid.programs.pop_back();
    std::string output = "old";
    TEST_ASSERT_TRUE(
        fermentation::encodeProgramCatalogPayload(invalid, output) ==
        ConfigurationCodecStatus::InvalidDocument);
    TEST_ASSERT_EQUAL_STRING("old", output.c_str());
}

void test_factory_catalog_writer_allocation_follows_exact_payload_size() {
    // Native Regression fuer relative Host-Allokationen. Sie ist keine reale
    // ESP32-Heapgarantie und zaehlt das bereits vorhandene Katalogmodell nicht
    // als neuen Encode-Puffer.
    const auto catalog = fermentation::makeFactoryProgramCatalog();
    const auto calculated = fermentation::configuration_codec_internal::
        calculateProgramCatalogPayloadSize(catalog);
    TEST_ASSERT_TRUE(calculated.status == ConfigurationCodecStatus::Success);
    const std::size_t baseline = gLiveAllocBytes;
    gPeakAllocBytes = baseline;
    std::string encoded;
    TEST_ASSERT_TRUE(
        fermentation::encodeProgramCatalogPayload(catalog, encoded) ==
        ConfigurationCodecStatus::Success);
    TEST_ASSERT_EQUAL_UINT32(calculated.payloadSize, encoded.size());
    const std::size_t peakDelta = gPeakAllocBytes - baseline;
    TEST_ASSERT_TRUE(peakDelta < 8192U);
    TEST_ASSERT_TRUE(
        peakDelta <
        fermentation::configuration_limits::kMaximumProgramCatalogPayloadBytes /
            4U);
}

void test_large_catalog_writer_allocation_follows_exact_payload_size() {
    const auto catalog = largeCatalog();
    const auto calculated = fermentation::configuration_codec_internal::
        calculateProgramCatalogPayloadSize(catalog);
    TEST_ASSERT_TRUE(calculated.status == ConfigurationCodecStatus::Success);
    const std::size_t baseline = gLiveAllocBytes;
    gPeakAllocBytes = baseline;
    std::string encoded;
    TEST_ASSERT_TRUE(
        fermentation::encodeProgramCatalogPayload(catalog, encoded) ==
        ConfigurationCodecStatus::Success);
    const std::size_t peakDelta = gPeakAllocBytes - baseline;
    TEST_ASSERT_TRUE(encoded.size() > 12000U);
    TEST_ASSERT_EQUAL_UINT32(calculated.payloadSize, encoded.size());
    TEST_ASSERT_TRUE(peakDelta < encoded.size() + 8192U);
}

void test_maximum_valid_catalog_has_exact_canonical_payload_size() {
    const auto catalog = maximumValidCatalog();
    TEST_ASSERT_TRUE(fermentation::validateProgramCatalog(catalog) ==
                     fermentation::ProgramCatalogStatus::Success);
    const auto calculated = fermentation::configuration_codec_internal::
        calculateProgramCatalogPayloadSize(catalog);
    TEST_ASSERT_TRUE(calculated.status == ConfigurationCodecStatus::Success);
    TEST_ASSERT_EQUAL_UINT32(19916U, calculated.payloadSize);
    std::string encoded;
    TEST_ASSERT_TRUE(
        fermentation::encodeProgramCatalogPayload(catalog, encoded) ==
        ConfigurationCodecStatus::Success);
    TEST_ASSERT_EQUAL_UINT32(19916U, encoded.size());
}

void test_global_payload_boundary_uses_production_size_calculation() {
    auto exact = maximumValidCatalog();
    const auto validSize = fermentation::configuration_codec_internal::
        calculateProgramCatalogPayloadSize(exact);
    TEST_ASSERT_EQUAL_UINT32(19916U, validSize.payloadSize);
    exact.programs.back().program.notes.append(
        fermentation::configuration_limits::kMaximumProgramCatalogPayloadBytes -
            validSize.payloadSize,
        'x');
    const auto exactSize = fermentation::configuration_codec_internal::
        calculateProgramCatalogPayloadSize(exact);
    TEST_ASSERT_TRUE(exactSize.status == ConfigurationCodecStatus::Success);
    TEST_ASSERT_EQUAL_UINT32(
        fermentation::configuration_limits::kMaximumProgramCatalogPayloadBytes,
        exactSize.payloadSize);
    const std::string oldOutput("old\0payload", 11U);
    std::string output = oldOutput;
    TEST_ASSERT_TRUE(fermentation::encodeProgramCatalogPayload(exact, output) ==
                     ConfigurationCodecStatus::InvalidDocument);
    TEST_ASSERT_EQUAL_UINT32(oldOutput.size(), output.size());
    TEST_ASSERT_EQUAL_MEMORY(oldOutput.data(), output.data(), oldOutput.size());

    exact.programs.back().program.notes.push_back('x');
    const auto overSize = fermentation::configuration_codec_internal::
        calculateProgramCatalogPayloadSize(exact);
    TEST_ASSERT_TRUE(overSize.status ==
                     ConfigurationCodecStatus::CapacityExceeded);
    const std::size_t baseline = gLiveAllocBytes;
    gPeakAllocBytes = baseline;
    TEST_ASSERT_TRUE(fermentation::encodeProgramCatalogPayload(exact, output) ==
                     ConfigurationCodecStatus::CapacityExceeded);
    TEST_ASSERT_EQUAL_UINT32(baseline, gPeakAllocBytes);
    TEST_ASSERT_EQUAL_UINT32(oldOutput.size(), output.size());
    TEST_ASSERT_EQUAL_MEMORY(oldOutput.data(), output.data(), oldOutput.size());
}

void test_successful_encode_can_hold_catalog_old_output_and_new_payload() {
    const auto catalog = fermentation::makeFactoryProgramCatalog();
    const auto calculated = fermentation::configuration_codec_internal::
        calculateProgramCatalogPayloadSize(catalog);
    std::string output(4096U, 'o');
    const std::size_t baseline = gLiveAllocBytes;
    gPeakAllocBytes = baseline;
    TEST_ASSERT_TRUE(fermentation::encodeProgramCatalogPayload(
                         catalog, output) == ConfigurationCodecStatus::Success);
    TEST_ASSERT_EQUAL_UINT32(calculated.payloadSize, output.size());
    const std::size_t peakDelta = gPeakAllocBytes - baseline;
    TEST_ASSERT_TRUE(peakDelta >= calculated.payloadSize);
    TEST_ASSERT_TRUE(peakDelta < calculated.payloadSize + 8192U);
}

void test_large_catalog_decode_does_not_materialize_a_second_full_catalog() {
    const auto catalog = largeCatalog();
    std::string encoded;
    TEST_ASSERT_TRUE(
        fermentation::encodeProgramCatalogPayload(catalog, encoded) ==
        ConfigurationCodecStatus::Success);
    const std::size_t baseline = gLiveAllocBytes;
    gPeakAllocBytes = baseline;
    const auto decoded = fermentation::decodeProgramCatalogPayload(1U, encoded);
    TEST_ASSERT_TRUE(decoded.status == ConfigurationCodecStatus::Success);
    const std::size_t peakDelta = gPeakAllocBytes - baseline;
    TEST_ASSERT_TRUE(peakDelta < encoded.size() * 2U);
}

void test_large_catalog_copy_migration_keeps_only_source_and_candidate() {
    auto source = largeCatalog();
    for (auto& document : source.programs) {
        document.schema.version = fermentation::kMigratableProgramSchemaVersion;
        document.schema.presentFields =
            fermentation::kSchema4RequiredProgramFields;
        document.program.maximumProductWaitMinutes.reset();
    }
    const std::size_t baseline = gLiveAllocBytes;
    gPeakAllocBytes = baseline;
    const auto migrated =
        fermentation::migrateProgramCatalogDocumentsToCurrentSchema(source);
    TEST_ASSERT_TRUE(migrated.status ==
                     fermentation::CopyMigrationStatus::Migrated);
    TEST_ASSERT_TRUE(migrated.document.has_value());
    const std::size_t peakDelta = gPeakAllocBytes - baseline;
    TEST_ASSERT_TRUE(peakDelta < 30000U);
    for (const auto& document : migrated.document->programs) {
        TEST_ASSERT_EQUAL_UINT32(fermentation::kCurrentProgramSchemaVersion,
                                 document.schema.version);
    }
}

}  // namespace

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_sensor_preference_wire_ids_are_explicit_in_both_directions);
    RUN_TEST(test_failure_policy_wire_ids_are_explicit_in_both_directions);
    RUN_TEST(test_completion_mode_wire_ids_are_explicit_in_both_directions);
    RUN_TEST(test_user_configuration_payload_golden_bytes_and_round_trip);
    RUN_TEST(
        test_user_configuration_full_envelope_golden_bytes_has_no_old_fields);
    RUN_TEST(test_user_codec_rejects_missing_extra_and_oversized_payloads);
    RUN_TEST(
        test_payload_capacity_boundaries_are_independent_from_field_validation);
    RUN_TEST(test_invalid_user_encode_leaves_output_unchanged);
    RUN_TEST(test_service_configuration_is_exactly_empty);
    RUN_TEST(
        test_program_catalog_round_trip_is_deterministic_and_preserves_notes);
    RUN_TEST(test_program_catalog_factory_payload_has_fixed_golden_bytes);
    RUN_TEST(
        test_program_catalog_rejects_truncated_trailing_and_capacity_payloads);
    RUN_TEST(test_program_catalog_rejects_unknown_schema_mask_and_enum);
    RUN_TEST(
        test_schema_four_programs_migrate_to_schema_five_deterministically);
    RUN_TEST(test_program_count_boundaries_are_checked_before_allocation);
    RUN_TEST(test_invalid_catalog_encode_leaves_output_unchanged);
    RUN_TEST(test_factory_catalog_writer_allocation_follows_exact_payload_size);
    RUN_TEST(test_large_catalog_writer_allocation_follows_exact_payload_size);
    RUN_TEST(test_maximum_valid_catalog_has_exact_canonical_payload_size);
    RUN_TEST(test_global_payload_boundary_uses_production_size_calculation);
    RUN_TEST(
        test_successful_encode_can_hold_catalog_old_output_and_new_payload);
    RUN_TEST(
        test_large_catalog_decode_does_not_materialize_a_second_full_catalog);
    RUN_TEST(test_large_catalog_copy_migration_keeps_only_source_and_candidate);
    return UNITY_END();
}
