#include "configuration_document_codec.hpp"

#include <algorithm>
#include <array>
#include <limits>
#include <set>
#include <utility>

#include "big_endian_codec.hpp"
#include "binary64_codec.hpp"
#include "byte_buffer.hpp"
#include "checked_size.hpp"
#include "configuration_document_codec_internal.hpp"
#include "configuration_limits.hpp"
#include "configuration_text.hpp"
#include "standard_program_catalog.hpp"

namespace fermentation {
namespace configuration_codec_internal {

bool sensorPreferenceToWireId(SensorPreference value, std::uint8_t& out) {
    switch (value) {
        case SensorPreference::ProductIfAvailableElseAir:
            out = 1U;
            return true;
        case SensorPreference::AirProductOptional:
            out = 2U;
            return true;
        case SensorPreference::ProductRequired:
            out = 3U;
            return true;
        case SensorPreference::AirOnly:
            out = 4U;
            return true;
    }
    return false;
}

bool sensorPreferenceFromWireId(std::uint8_t wireId, SensorPreference& out) {
    switch (wireId) {
        case 1U:
            out = SensorPreference::ProductIfAvailableElseAir;
            return true;
        case 2U:
            out = SensorPreference::AirProductOptional;
            return true;
        case 3U:
            out = SensorPreference::ProductRequired;
            return true;
        case 4U:
            out = SensorPreference::AirOnly;
            return true;
        default:
            return false;
    }
}

bool productSensorFailurePolicyToWireId(ProductSensorFailurePolicy value,
                                        std::uint8_t& out) {
    switch (value) {
        case ProductSensorFailurePolicy::FallbackToAirAfterTimeout:
            out = 1U;
            return true;
        case ProductSensorFailurePolicy::WaitForUser:
            out = 2U;
            return true;
        case ProductSensorFailurePolicy::StopToSafeState:
            out = 3U;
            return true;
    }
    return false;
}

bool productSensorFailurePolicyFromWireId(std::uint8_t wireId,
                                          ProductSensorFailurePolicy& out) {
    switch (wireId) {
        case 1U:
            out = ProductSensorFailurePolicy::FallbackToAirAfterTimeout;
            return true;
        case 2U:
            out = ProductSensorFailurePolicy::WaitForUser;
            return true;
        case 3U:
            out = ProductSensorFailurePolicy::StopToSafeState;
            return true;
        default:
            return false;
    }
}

bool returnStrategyToWireId(ReturnStrategy value, std::uint8_t& out) {
    switch (value) {
        case ReturnStrategy::RemainOnAirUntilEnd:
            out = 1U;
            return true;
        case ReturnStrategy::ManualReturnToProduct:
            out = 2U;
            return true;
        case ReturnStrategy::AutomaticValidatedReturnToProduct:
            out = 3U;
            return true;
    }
    return false;
}

bool returnStrategyFromWireId(std::uint8_t wireId, ReturnStrategy& out) {
    switch (wireId) {
        case 1U:
            out = ReturnStrategy::RemainOnAirUntilEnd;
            return true;
        case 2U:
            out = ReturnStrategy::ManualReturnToProduct;
            return true;
        case 3U:
            out = ReturnStrategy::AutomaticValidatedReturnToProduct;
            return true;
        default:
            return false;
    }
}

bool completionModeToWireId(CompletionMode value, std::uint8_t& out) {
    switch (value) {
        case CompletionMode::FinishWithoutCooling:
            out = 1U;
            return true;
        case CompletionMode::CoolThenFinish:
            out = 2U;
            return true;
        case CompletionMode::CoolAndHoldForDuration:
            out = 3U;
            return true;
        case CompletionMode::CoolAndHoldUntilManualStop:
            out = 4U;
            return true;
    }
    return false;
}

bool completionModeFromWireId(std::uint8_t wireId, CompletionMode& out) {
    switch (wireId) {
        case 1U:
            out = CompletionMode::FinishWithoutCooling;
            return true;
        case 2U:
            out = CompletionMode::CoolThenFinish;
            return true;
        case 3U:
            out = CompletionMode::CoolAndHoldForDuration;
            return true;
        case 4U:
            out = CompletionMode::CoolAndHoldUntilManualStop;
            return true;
        default:
            return false;
    }
}

}  // namespace configuration_codec_internal

namespace {

using device_platform::ByteReader;
using device_platform::ByteWriter;
namespace big_endian = device_platform::big_endian;

bool writeString(ByteWriter& writer, const std::string& value) {
    if (value.size() > std::numeric_limits<std::uint16_t>::max()) {
        return false;
    }
    return big_endian::writeUint16(writer,
                                   static_cast<std::uint16_t>(value.size())) &&
           writer.writeBytes(value.data(), value.size());
}

bool readString(ByteReader& reader, std::size_t maximumBytes,
                std::string& out) {
    std::uint16_t length = 0U;
    if (!big_endian::readUint16(reader, length) || length > maximumBytes ||
        length > reader.remaining()) {
        return false;
    }
    std::string candidate(length, '\0');
    if (!reader.readBytes(candidate.data(), length)) {
        return false;
    }
    out = std::move(candidate);
    return true;
}

bool writeOptionalUint32(ByteWriter& writer,
                         const std::optional<std::uint32_t>& value) {
    return big_endian::writeOptionalTag(writer, value.has_value()) &&
           (!value.has_value() || big_endian::writeUint32(writer, *value));
}

bool readOptionalUint32(ByteReader& reader, std::optional<std::uint32_t>& out) {
    bool present = false;
    if (!big_endian::readOptionalTag(reader, present)) {
        return false;
    }
    if (!present) {
        out.reset();
        return true;
    }
    std::uint32_t value = 0U;
    if (!big_endian::readUint32(reader, value)) {
        return false;
    }
    out = value;
    return true;
}

bool writeOptionalDouble(ByteWriter& writer,
                         const std::optional<double>& value) {
    return big_endian::writeOptionalTag(writer, value.has_value()) &&
           (!value.has_value() ||
            device_platform::binary64::encode(*value, writer));
}

bool readOptionalDouble(ByteReader& reader, std::optional<double>& out) {
    bool present = false;
    if (!big_endian::readOptionalTag(reader, present)) {
        return false;
    }
    if (!present) {
        out.reset();
        return true;
    }
    double value = 0.0;
    if (!device_platform::binary64::decode(reader, value)) {
        return false;
    }
    out = value;
    return true;
}

bool readSensor(ByteReader& reader, SensorPreference& out) {
    std::uint8_t raw = 0U;
    return big_endian::readUint8(reader, raw) &&
           configuration_codec_internal::sensorPreferenceFromWireId(raw, out);
}

bool readFailure(ByteReader& reader, ProductSensorFailurePolicy& out) {
    std::uint8_t raw = 0U;
    return big_endian::readUint8(reader, raw) &&
           configuration_codec_internal::productSensorFailurePolicyFromWireId(
               raw, out);
}

bool readCompletion(ByteReader& reader, CompletionMode& out) {
    std::uint8_t raw = 0U;
    return big_endian::readUint8(reader, raw) &&
           configuration_codec_internal::completionModeFromWireId(raw, out);
}

bool readReturnStrategy(ByteReader& reader, ReturnStrategy& out) {
    std::uint8_t raw = 0U;
    return big_endian::readUint8(reader, raw) &&
           configuration_codec_internal::returnStrategyFromWireId(raw, out);
}

bool writeProgramIdentityAndFlags(ByteWriter& writer,
                                  const ProgramDocument& document,
                                  std::uint8_t sensor, std::uint8_t failure,
                                  std::uint8_t returnStrategy) {
    const auto& program = document.program;
    bool ok = big_endian::writeUint32(writer, document.schema.version);
    ok = ok && big_endian::writeUint64(writer, document.schema.presentFields);
    ok = ok && writeString(writer, program.id);
    ok = ok && writeString(writer, program.name);
    // `notes` besitzt bewusst kein ProgramFieldMask-Bit. Der Katalogcodec
    // kodiert es unabhaengig von der Feldmaske immer direkt nach dem Namen.
    ok = ok && writeString(writer, program.notes);
    ok = ok && big_endian::writeBool(writer, program.builtIn);
    ok = ok && big_endian::writeBool(writer, program.factoryCatalogEntry);
    ok = ok && big_endian::writeBool(writer, program.resettable);
    ok = ok && big_endian::writeBool(writer, program.userDeletable);
    ok = ok && big_endian::writeBool(writer, program.installed);
    ok = ok && big_endian::writeBool(writer, program.enabled);
    ok = ok && big_endian::writeBool(writer, program.preheat);
    ok = ok && big_endian::writeUint8(writer, sensor);
    ok = ok && big_endian::writeUint8(writer, failure);
    ok = ok && writeOptionalUint32(
                   writer, program.productSensorFailure.fallbackDelaySeconds);
    if (document.schema.version >= kReturnStrategyFieldIntroducedInSchema) {
        ok = ok && big_endian::writeUint8(writer, returnStrategy);
    }
    return ok;
}

bool writeProgramStagesAndLimits(ByteWriter& writer,
                                 const ProgramDocument& document) {
    const auto& program = document.program;
    bool ok = big_endian::writeUint8(
        writer, static_cast<std::uint8_t>(program.fermentationStages.size()));
    for (const auto& stage : program.fermentationStages) {
        ok = ok && writeOptionalDouble(writer, stage.targetTemperatureCelsius);
        ok = ok && writeOptionalUint32(writer, stage.durationMinutes);
    }
    ok = ok &&
         writeOptionalDouble(writer, program.targetQualification.bandCelsius);
    ok = ok && writeOptionalUint32(writer,
                                   program.targetQualification.durationMinutes);
    ok = ok && writeOptionalUint32(writer, program.maximumTargetReachMinutes);
    if (document.schema.version >= kProductWaitFieldIntroducedInSchema) {
        ok = ok &&
             writeOptionalUint32(writer, program.maximumProductWaitMinutes);
    }
    return ok;
}

bool writeProgram(ByteWriter& writer, const ProgramDocument& document) {
    const auto& program = document.program;
    std::uint8_t sensor = 0U;
    std::uint8_t failure = 0U;
    std::uint8_t completion = 0U;
    std::uint8_t returnStrategy = 0U;
    if (!configuration_codec_internal::sensorPreferenceToWireId(
            program.sensorPreference, sensor) ||
        !configuration_codec_internal::productSensorFailurePolicyToWireId(
            program.productSensorFailure.policy, failure) ||
        !configuration_codec_internal::completionModeToWireId(
            program.completion.mode, completion) ||
        !configuration_codec_internal::returnStrategyToWireId(
            program.productSensorFailure.returnStrategy, returnStrategy) ||
        program.fermentationStages.size() >
            std::numeric_limits<std::uint8_t>::max()) {
        return false;
    }
    bool ok = writeProgramIdentityAndFlags(writer, document, sensor, failure,
                                           returnStrategy) &&
              writeProgramStagesAndLimits(writer, document);
    ok = ok && big_endian::writeUint8(writer, completion);
    ok = ok &&
         writeOptionalDouble(writer, program.completion.coolingTargetCelsius);
    ok = ok &&
         writeOptionalUint32(writer, program.completion.holdDurationMinutes);
    return ok;
}

class ProgramCatalogPayloadSizeAccumulator {
   public:
    [[nodiscard]] bool add(std::size_t bytes) {
        std::size_t next = 0U;
        if (!device_platform::checkedAddSize(
                size_, bytes,
                configuration_limits::kMaximumProgramCatalogPayloadBytes,
                next)) {
            return false;
        }
        size_ = next;
        return true;
    }

    [[nodiscard]] std::size_t size() const { return size_; }

   private:
    std::size_t size_{0U};
};

ConfigurationCodecStatus addStringPayloadSize(
    ProgramCatalogPayloadSizeAccumulator& size, const std::string& value) {
    if (value.size() > std::numeric_limits<std::uint16_t>::max()) {
        return ConfigurationCodecStatus::InvalidDocument;
    }
    if (!size.add(sizeof(std::uint16_t)) || !size.add(value.size())) {
        return ConfigurationCodecStatus::CapacityExceeded;
    }
    return ConfigurationCodecStatus::Success;
}

ConfigurationCodecStatus addOptionalPayloadSize(
    ProgramCatalogPayloadSizeAccumulator& size, bool present,
    std::size_t valueBytes) {
    if (!size.add(sizeof(std::uint8_t)) || (present && !size.add(valueBytes))) {
        return ConfigurationCodecStatus::CapacityExceeded;
    }
    return ConfigurationCodecStatus::Success;
}

ConfigurationCodecStatus addProgramIdentityAndFlagsPayloadSize(
    ProgramCatalogPayloadSizeAccumulator& size,
    const ProgramDocument& document) {
    const auto& program = document.program;
    if (!size.add(sizeof(std::uint32_t)) ||
        !size.add(sizeof(ProgramFieldMask))) {
        return ConfigurationCodecStatus::CapacityExceeded;
    }
    for (const auto* value : {&program.id, &program.name, &program.notes}) {
        const auto status = addStringPayloadSize(size, *value);
        if (status != ConfigurationCodecStatus::Success) {
            return status;
        }
    }
    // Sieben Boolwerte sowie Sensor- und Failure-Wire-ID.
    if (!size.add(7U) || !size.add(2U)) {
        return ConfigurationCodecStatus::CapacityExceeded;
    }
    const auto delayStatus = addOptionalPayloadSize(
        size, program.productSensorFailure.fallbackDelaySeconds.has_value(),
        sizeof(std::uint32_t));
    if (delayStatus != ConfigurationCodecStatus::Success) {
        return delayStatus;
    }
    if (document.schema.version >= kReturnStrategyFieldIntroducedInSchema &&
        !size.add(sizeof(std::uint8_t))) {
        return ConfigurationCodecStatus::CapacityExceeded;
    }
    return ConfigurationCodecStatus::Success;
}

ConfigurationCodecStatus addProgramStagesAndLimitsPayloadSize(
    ProgramCatalogPayloadSizeAccumulator& size,
    const ProgramDocument& document) {
    const auto& program = document.program;
    if (program.fermentationStages.size() >
            std::numeric_limits<std::uint8_t>::max() ||
        !size.add(sizeof(std::uint8_t))) {
        return program.fermentationStages.size() >
                       std::numeric_limits<std::uint8_t>::max()
                   ? ConfigurationCodecStatus::InvalidDocument
                   : ConfigurationCodecStatus::CapacityExceeded;
    }
    for (const auto& stage : program.fermentationStages) {
        if (addOptionalPayloadSize(
                size, stage.targetTemperatureCelsius.has_value(),
                sizeof(std::uint64_t)) != ConfigurationCodecStatus::Success ||
            addOptionalPayloadSize(size, stage.durationMinutes.has_value(),
                                   sizeof(std::uint32_t)) !=
                ConfigurationCodecStatus::Success) {
            return ConfigurationCodecStatus::CapacityExceeded;
        }
    }
    const bool optionalSizesFit =
        addOptionalPayloadSize(
            size, program.targetQualification.bandCelsius.has_value(),
            sizeof(std::uint64_t)) == ConfigurationCodecStatus::Success &&
        addOptionalPayloadSize(
            size, program.targetQualification.durationMinutes.has_value(),
            sizeof(std::uint32_t)) == ConfigurationCodecStatus::Success &&
        addOptionalPayloadSize(
            size, program.maximumTargetReachMinutes.has_value(),
            sizeof(std::uint32_t)) == ConfigurationCodecStatus::Success;
    if (!optionalSizesFit) {
        return ConfigurationCodecStatus::CapacityExceeded;
    }
    if (document.schema.version >= kProductWaitFieldIntroducedInSchema) {
        return addOptionalPayloadSize(
            size, program.maximumProductWaitMinutes.has_value(),
            sizeof(std::uint32_t));
    }
    return ConfigurationCodecStatus::Success;
}

ConfigurationCodecStatus addProgramPayloadSize(
    ProgramCatalogPayloadSizeAccumulator& size,
    const ProgramDocument& document) {
    std::uint8_t ignoredWireId = 0U;
    if (!configuration_codec_internal::sensorPreferenceToWireId(
            document.program.sensorPreference, ignoredWireId) ||
        !configuration_codec_internal::productSensorFailurePolicyToWireId(
            document.program.productSensorFailure.policy, ignoredWireId) ||
        !configuration_codec_internal::completionModeToWireId(
            document.program.completion.mode, ignoredWireId) ||
        !configuration_codec_internal::returnStrategyToWireId(
            document.program.productSensorFailure.returnStrategy,
            ignoredWireId)) {
        return ConfigurationCodecStatus::InvalidDocument;
    }
    auto status = addProgramIdentityAndFlagsPayloadSize(size, document);
    if (status == ConfigurationCodecStatus::Success) {
        status = addProgramStagesAndLimitsPayloadSize(size, document);
    }
    if (status != ConfigurationCodecStatus::Success) {
        return status;
    }
    if (!size.add(sizeof(std::uint8_t))) {
        return ConfigurationCodecStatus::CapacityExceeded;
    }
    const auto& completion = document.program.completion;
    if (addOptionalPayloadSize(
            size, completion.coolingTargetCelsius.has_value(),
            sizeof(std::uint64_t)) != ConfigurationCodecStatus::Success ||
        addOptionalPayloadSize(size, completion.holdDurationMinutes.has_value(),
                               sizeof(std::uint32_t)) !=
            ConfigurationCodecStatus::Success) {
        return ConfigurationCodecStatus::CapacityExceeded;
    }
    return ConfigurationCodecStatus::Success;
}

configuration_codec_internal::ProgramCatalogPayloadSizeResult
calculateProgramCatalogPayloadSizeImpl(const ProgramCatalog& catalog) {
    if (catalog.programs.size() > std::numeric_limits<std::uint8_t>::max()) {
        return {ConfigurationCodecStatus::InvalidDocument, 0U};
    }
    ProgramCatalogPayloadSizeAccumulator size;
    if (!size.add(sizeof(std::uint8_t))) {
        return {ConfigurationCodecStatus::CapacityExceeded, 0U};
    }
    for (const auto& document : catalog.programs) {
        const auto status = addProgramPayloadSize(size, document);
        if (status != ConfigurationCodecStatus::Success) {
            return {status, 0U};
        }
    }
    return {ConfigurationCodecStatus::Success, size.size()};
}

ConfigurationCodecStatus readProgramSchema(ByteReader& reader,
                                           ProgramDocument& candidate) {
    if (!big_endian::readUint32(reader, candidate.schema.version) ||
        !big_endian::readUint64(reader, candidate.schema.presentFields)) {
        return ConfigurationCodecStatus::Truncated;
    }
    ProgramFieldMask knownFields = 0U;
    ProgramFieldMask requiredFields = 0U;
    if (candidate.schema.version == kMinimumMigratableProgramSchemaVersion) {
        knownFields = kSchema4RequiredProgramFields;
        requiredFields = kSchema4RequiredProgramFields;
    } else if (candidate.schema.version ==
               kProductWaitFieldIntroducedInSchema) {
        knownFields = kSchema5RequiredProgramFields;
        requiredFields = kSchema5RequiredProgramFields;
    } else if (candidate.schema.version == kCurrentProgramSchemaVersion) {
        knownFields = kCurrentKnownProgramFields;
        requiredFields = kCurrentRequiredProgramFields;
    } else {
        return ConfigurationCodecStatus::UnsupportedSchema;
    }
    if ((candidate.schema.presentFields & ~knownFields) != 0U ||
        (candidate.schema.presentFields & requiredFields) != requiredFields) {
        return ConfigurationCodecStatus::InvalidWireValue;
    }
    return ConfigurationCodecStatus::Success;
}

ConfigurationCodecStatus readProgramIdentityAndFlags(
    ByteReader& reader, std::uint32_t schemaVersion,
    ProgramDefinition& program) {
    using namespace configuration_limits;
    if (!readString(reader, kMaximumProgramIdBytes, program.id) ||
        !readString(reader, kMaximumVisibleNameBytes, program.name) ||
        !readString(reader, kMaximumNotesBytes, program.notes)) {
        return ConfigurationCodecStatus::Truncated;
    }
    bool ok = big_endian::readBool(reader, program.builtIn);
    ok = ok && big_endian::readBool(reader, program.factoryCatalogEntry);
    ok = ok && big_endian::readBool(reader, program.resettable);
    ok = ok && big_endian::readBool(reader, program.userDeletable);
    ok = ok && big_endian::readBool(reader, program.installed);
    ok = ok && big_endian::readBool(reader, program.enabled);
    ok = ok && big_endian::readBool(reader, program.preheat);
    ok = ok && readSensor(reader, program.sensorPreference);
    ok = ok && readFailure(reader, program.productSensorFailure.policy);
    ok = ok && readOptionalUint32(
                   reader, program.productSensorFailure.fallbackDelaySeconds);
    if (ok && schemaVersion >= kReturnStrategyFieldIntroducedInSchema) {
        ok = readReturnStrategy(reader,
                                program.productSensorFailure.returnStrategy);
    }
    return ok ? ConfigurationCodecStatus::Success
              : ConfigurationCodecStatus::InvalidWireValue;
}

ConfigurationCodecStatus readProgramStages(ByteReader& reader,
                                           ProgramDefinition& program) {
    std::uint8_t stageCount = 0U;
    if (!big_endian::readUint8(reader, stageCount) || stageCount != 1U) {
        return ConfigurationCodecStatus::InvalidWireValue;
    }
    program.fermentationStages.reserve(stageCount);
    for (std::uint8_t index = 0U; index < stageCount; ++index) {
        FermentationStage stage;
        if (!readOptionalDouble(reader, stage.targetTemperatureCelsius) ||
            !readOptionalUint32(reader, stage.durationMinutes)) {
            return ConfigurationCodecStatus::Truncated;
        }
        program.fermentationStages.push_back(stage);
    }
    return ConfigurationCodecStatus::Success;
}

ConfigurationCodecStatus readProgramLimitsAndCompletion(
    ByteReader& reader, std::uint32_t schemaVersion,
    ProgramDefinition& program) {
    if (!readOptionalDouble(reader, program.targetQualification.bandCelsius) ||
        !readOptionalUint32(reader,
                            program.targetQualification.durationMinutes) ||
        !readOptionalUint32(reader, program.maximumTargetReachMinutes)) {
        return ConfigurationCodecStatus::Truncated;
    }
    if (schemaVersion >= kProductWaitFieldIntroducedInSchema &&
        !readOptionalUint32(reader, program.maximumProductWaitMinutes)) {
        return ConfigurationCodecStatus::Truncated;
    }
    if (!readCompletion(reader, program.completion.mode) ||
        !readOptionalDouble(reader, program.completion.coolingTargetCelsius) ||
        !readOptionalUint32(reader, program.completion.holdDurationMinutes)) {
        return ConfigurationCodecStatus::Truncated;
    }
    return ConfigurationCodecStatus::Success;
}

ConfigurationCodecStatus readProgram(ByteReader& reader, ProgramDocument& out) {
    ProgramDocument candidate;
    auto status = readProgramSchema(reader, candidate);
    if (status != ConfigurationCodecStatus::Success) {
        return status;
    }
    auto& program = candidate.program;
    status =
        readProgramIdentityAndFlags(reader, candidate.schema.version, program);
    if (status == ConfigurationCodecStatus::Success) {
        status = readProgramStages(reader, program);
    }
    if (status == ConfigurationCodecStatus::Success) {
        status = readProgramLimitsAndCompletion(
            reader, candidate.schema.version, program);
    }
    if (status != ConfigurationCodecStatus::Success) {
        return status;
    }
    if (candidate.schema.version < kCurrentProgramSchemaVersion) {
        auto migrated = migrateProgramToCurrentSchema(candidate);
        if (migrated.status != MigrationStatus::Migrated ||
            !migrated.document.has_value()) {
            return ConfigurationCodecStatus::MigrationFailed;
        }
        candidate = std::move(*migrated.document);
    }
    out = std::move(candidate);
    return ConfigurationCodecStatus::Success;
}

ProgramCatalogStatus validateStreamedProgram(
    const ProgramDocument& document, std::size_t index,
    const std::array<ProgramDocument, 4>& factory,
    std::set<std::string>& identifiers, std::size_t& factoryCount) {
    using namespace configuration_limits;
    const auto& program = document.program;
    if (index < kFactoryProgramCount) {
        if (program.id != factory[index].program.id) {
            return ProgramCatalogStatus::InvalidFactoryOrder;
        }
        if (!program.builtIn || !program.factoryCatalogEntry ||
            !program.resettable) {
            return ProgramCatalogStatus::InvalidFactoryMarkers;
        }
    } else {
        if (program.builtIn || program.factoryCatalogEntry ||
            program.resettable) {
            return ProgramCatalogStatus::InvalidUserMarkers;
        }
        const auto reserved =
            std::any_of(factory.begin(), factory.end(),
                        [&program](const ProgramDocument& entry) {
                            return program.id == entry.program.id;
                        });
        if (reserved) {
            return ProgramCatalogStatus::ReservedFactoryId;
        }
    }
    if (!identifiers.insert(program.id).second) {
        return ProgramCatalogStatus::DuplicateProgramId;
    }
    if (validateLowercaseIdentifier(program.id, kMinimumProgramIdBytes,
                                    kMaximumProgramIdBytes) !=
        ConfigurationTextStatus::Success) {
        return ProgramCatalogStatus::InvalidProgramId;
    }
    if (validateVisibleName(program.name) != ConfigurationTextStatus::Success) {
        return ProgramCatalogStatus::InvalidProgramName;
    }
    if (validateProgramNotes(program.notes) !=
        ConfigurationTextStatus::Success) {
        return ProgramCatalogStatus::InvalidProgramNotes;
    }
    if (!validateProgram(document, ValidationPurpose::CatalogTemplate)
             .valid()) {
        return ProgramCatalogStatus::InvalidProgramDocument;
    }
    if (program.factoryCatalogEntry) {
        ++factoryCount;
    }
    return ProgramCatalogStatus::Success;
}

}  // namespace

namespace configuration_codec_internal {

ProgramCatalogPayloadSizeResult calculateProgramCatalogPayloadSize(
    const ProgramCatalog& catalog) {
    return calculateProgramCatalogPayloadSizeImpl(catalog);
}

}  // namespace configuration_codec_internal

ConfigurationCodecStatus
configuration_codec_internal::encodeSingleProgramDocumentPayload(
    const ProgramDocument& document, std::string& out) {
    if (!validateProgram(document, ValidationPurpose::Runnable).valid()) {
        return ConfigurationCodecStatus::InvalidDocument;
    }
    ByteWriter writer(configuration_limits::kMaximumProgramCatalogPayloadBytes);
    if (!writeProgram(writer, document)) {
        return ConfigurationCodecStatus::CapacityExceeded;
    }
    auto encoded = writer.takeBytes();
    out.swap(encoded);
    return ConfigurationCodecStatus::Success;
}

ConfigurationDecodeResult<ProgramDocument>
configuration_codec_internal::decodeSingleProgramDocumentPayload(
    const std::string& payload) {
    if (payload.size() >
        configuration_limits::kMaximumProgramCatalogPayloadBytes) {
        return {ConfigurationCodecStatus::CapacityExceeded, std::nullopt};
    }
    ByteReader reader(payload);
    ProgramDocument candidate;
    const auto status = readProgram(reader, candidate);
    if (status != ConfigurationCodecStatus::Success) {
        return {status, std::nullopt};
    }
    if (reader.remaining() != 0U) {
        return {ConfigurationCodecStatus::TrailingBytes, std::nullopt};
    }
    return {ConfigurationCodecStatus::Success, std::move(candidate)};
}

ConfigurationCodecStatus encodeUserConfigurationPayload(
    const UserConfiguration& configuration, std::uint32_t schemaVersion,
    const device_platform::ITimeZoneResolver& resolver, std::string& out) {
    if (schemaVersion !=
            static_cast<std::uint32_t>(UserConfigurationSchema::Version1) &&
        schemaVersion !=
            static_cast<std::uint32_t>(UserConfigurationSchema::Version2)) {
        return ConfigurationCodecStatus::UnsupportedSchema;
    }
    if (validateUserConfiguration(configuration, resolver).status !=
        UserConfigurationStatus::Success) {
        return ConfigurationCodecStatus::InvalidDocument;
    }
    ByteWriter writer(
        configuration_limits::kMaximumUserConfigurationPayloadBytes);
    if (!writeString(writer, configuration.displayLanguageId) ||
        !writeString(writer, configuration.timeZoneId) ||
        !writeString(writer, configuration.deviceName) ||
        (schemaVersion ==
             static_cast<std::uint32_t>(UserConfigurationSchema::Version2) &&
         !writeString(writer, configuration.activeThemeId))) {
        return ConfigurationCodecStatus::CapacityExceeded;
    }
    auto encoded = writer.takeBytes();
    out.swap(encoded);
    return ConfigurationCodecStatus::Success;
}

ConfigurationDecodeResult<UserConfiguration> decodeUserConfigurationPayload(
    std::uint32_t schemaVersion, const std::string& payload,
    const device_platform::ITimeZoneResolver& resolver) {
    if (schemaVersion !=
            static_cast<std::uint32_t>(UserConfigurationSchema::Version1) &&
        schemaVersion !=
            static_cast<std::uint32_t>(UserConfigurationSchema::Version2)) {
        return {ConfigurationCodecStatus::UnsupportedSchema, std::nullopt};
    }
    if (payload.size() >
        configuration_limits::kMaximumUserConfigurationPayloadBytes) {
        return {ConfigurationCodecStatus::CapacityExceeded, std::nullopt};
    }
    ByteReader reader(payload);
    UserConfiguration candidate;
    if (!readString(reader, configuration_limits::kMaximumLanguageIdBytes,
                    candidate.displayLanguageId) ||
        !readString(reader, configuration_limits::kMaximumTimeZoneIdBytes,
                    candidate.timeZoneId) ||
        !readString(reader, configuration_limits::kMaximumVisibleNameBytes,
                    candidate.deviceName) ||
        (schemaVersion ==
             static_cast<std::uint32_t>(UserConfigurationSchema::Version2) &&
         !readString(reader, configuration_limits::kMaximumThemeIdBytes,
                     candidate.activeThemeId))) {
        return {ConfigurationCodecStatus::Truncated, std::nullopt};
    }
    if (reader.remaining() != 0U) {
        return {ConfigurationCodecStatus::TrailingBytes, std::nullopt};
    }
    if (validateUserConfiguration(candidate, resolver).status !=
        UserConfigurationStatus::Success) {
        return {ConfigurationCodecStatus::InvalidDocument, std::nullopt};
    }
    return {ConfigurationCodecStatus::Success, std::move(candidate)};
}

ConfigurationCodecStatus encodeServiceConfigurationPayload(
    const ServiceConfiguration& /*configuration*/, std::string& out) {
    std::string encoded;
    out.swap(encoded);
    return ConfigurationCodecStatus::Success;
}

ConfigurationDecodeResult<ServiceConfiguration>
decodeServiceConfigurationPayload(std::uint32_t schemaVersion,
                                  const std::string& payload) {
    if (schemaVersion !=
        static_cast<std::uint32_t>(ServiceConfigurationSchema::Version1)) {
        return {ConfigurationCodecStatus::UnsupportedSchema, std::nullopt};
    }
    if (!payload.empty()) {
        return {ConfigurationCodecStatus::TrailingBytes, std::nullopt};
    }
    return {ConfigurationCodecStatus::Success, ServiceConfiguration{}};
}

ConfigurationCodecStatus encodeProgramCatalogPayload(
    const ProgramCatalog& catalog, std::string& out) {
    const auto size =
        configuration_codec_internal::calculateProgramCatalogPayloadSize(
            catalog);
    if (size.status != ConfigurationCodecStatus::Success) {
        return size.status;
    }
    if (validateProgramCatalog(catalog) != ProgramCatalogStatus::Success) {
        return ConfigurationCodecStatus::InvalidDocument;
    }
    ByteWriter writer(size.payloadSize);
    if (!big_endian::writeUint8(
            writer, static_cast<std::uint8_t>(catalog.programs.size()))) {
        return ConfigurationCodecStatus::CapacityExceeded;
    }
    for (const auto& document : catalog.programs) {
        if (!writeProgram(writer, document)) {
            return ConfigurationCodecStatus::CapacityExceeded;
        }
    }
    if (writer.size() != size.payloadSize) {
        return ConfigurationCodecStatus::CapacityExceeded;
    }
    auto encoded = writer.takeBytes();
    out.swap(encoded);
    return ConfigurationCodecStatus::Success;
}

ConfigurationDecodeResult<ProgramCatalog> decodeProgramCatalogPayload(
    std::uint32_t schemaVersion, const std::string& payload) {
    if (schemaVersion !=
        static_cast<std::uint32_t>(ProgramCatalogSchema::Version1)) {
        return {ConfigurationCodecStatus::UnsupportedSchema, std::nullopt};
    }
    if (payload.size() >
        configuration_limits::kMaximumProgramCatalogPayloadBytes) {
        return {ConfigurationCodecStatus::CapacityExceeded, std::nullopt};
    }
    ByteReader reader(payload);
    std::uint8_t count = 0U;
    if (!big_endian::readUint8(reader, count)) {
        return {ConfigurationCodecStatus::Truncated, std::nullopt};
    }
    if (count < configuration_limits::kFactoryProgramCount ||
        count > configuration_limits::kMaximumProgramCount) {
        return {ConfigurationCodecStatus::InvalidWireValue, std::nullopt};
    }
    ProgramCatalog candidate;
    candidate.programs.reserve(count);
    for (std::uint8_t index = 0U; index < count; ++index) {
        ProgramDocument document;
        const auto status = readProgram(reader, document);
        if (status != ConfigurationCodecStatus::Success) {
            return {status, std::nullopt};
        }
        candidate.programs.push_back(std::move(document));
    }
    if (reader.remaining() != 0U) {
        return {ConfigurationCodecStatus::TrailingBytes, std::nullopt};
    }
    if (validateProgramCatalog(candidate) != ProgramCatalogStatus::Success) {
        return {ConfigurationCodecStatus::InvalidDocument, std::nullopt};
    }
    return {ConfigurationCodecStatus::Success, std::move(candidate)};
}

ConfigurationCodecStatus validateProgramCatalogPayload(
    std::uint32_t schemaVersion, const std::string& payload,
    const ProgramCatalog* expected) {
    if (schemaVersion !=
        static_cast<std::uint32_t>(ProgramCatalogSchema::Version1)) {
        return ConfigurationCodecStatus::UnsupportedSchema;
    }
    if (payload.size() >
        configuration_limits::kMaximumProgramCatalogPayloadBytes) {
        return ConfigurationCodecStatus::CapacityExceeded;
    }
    ByteReader reader(payload);
    std::uint8_t count = 0U;
    if (!big_endian::readUint8(reader, count)) {
        return ConfigurationCodecStatus::Truncated;
    }
    if (count < configuration_limits::kFactoryProgramCount ||
        count > configuration_limits::kMaximumProgramCount) {
        return ConfigurationCodecStatus::InvalidWireValue;
    }
    if (expected != nullptr && expected->programs.size() != count) {
        return ConfigurationCodecStatus::InvalidDocument;
    }
    const auto factory = FactoryProgramCatalog::programs();
    std::set<std::string> identifiers;
    std::size_t factoryCount = 0U;
    for (std::size_t index = 0U; index < count; ++index) {
        ProgramDocument document;
        const auto status = readProgram(reader, document);
        if (status != ConfigurationCodecStatus::Success) {
            return status;
        }
        if (validateStreamedProgram(document, index, factory, identifiers,
                                    factoryCount) !=
            ProgramCatalogStatus::Success) {
            return ConfigurationCodecStatus::InvalidDocument;
        }
        if (expected != nullptr &&
            !configurationContentEquals(document, expected->programs[index])) {
            return ConfigurationCodecStatus::InvalidDocument;
        }
    }
    if (reader.remaining() != 0U) {
        return ConfigurationCodecStatus::TrailingBytes;
    }
    return factoryCount == configuration_limits::kFactoryProgramCount
               ? ConfigurationCodecStatus::Success
               : ConfigurationCodecStatus::InvalidDocument;
}

}  // namespace fermentation
