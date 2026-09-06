#include "run_persistence_codec.hpp"

#include <limits>
#include <utility>

#include "big_endian_codec.hpp"
#include "binary64_codec.hpp"
#include "byte_buffer.hpp"
#include "crc32.hpp"
#include "program_document_codec.hpp"
#include "storage_envelope.hpp"

namespace fermentation {
namespace {

using device_platform::ByteReader;
using device_platform::ByteWriter;
namespace be = device_platform::big_endian;

constexpr std::size_t kMaximumCheckpointPayloadBytes =
    kMaximumRunPersistencePayloadBytes;
// #21, 6.12: the sensor-selection field only exists from schema 2 onward.
constexpr std::uint32_t kSensorSelectionFieldIntroducedInSchema = 2U;
// #18, 5.28: the recovery/progress block only exists from schema 3 onward.
constexpr std::uint32_t kRecoveryFieldsIntroducedInSchema = 3U;
constexpr device_platform::RecordTypeId kCheckpointRecordType{7U};
constexpr device_platform::RecordTypeId kHeadRecordType{8U};
constexpr std::size_t kMaximumCheckpointRecordBytes = 8240U;
constexpr std::size_t kMaximumHeadRecordBytes = 256U;

bool writeString(ByteWriter& writer, const std::string& value) {
    return value.size() <= std::numeric_limits<std::uint16_t>::max() &&
           be::writeUint16(writer, static_cast<std::uint16_t>(value.size())) &&
           writer.writeBytes(value.data(), value.size());
}

bool readString(ByteReader& reader, std::size_t max, std::string& out) {
    std::uint16_t length = 0U;
    if (!be::readUint16(reader, length) || length > max ||
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

bool writeOptionalInt64(ByteWriter& writer,
                        const std::optional<std::int64_t>& value) {
    return be::writeOptionalTag(writer, value.has_value()) &&
           (!value.has_value() || be::writeInt64(writer, *value));
}

bool readOptionalInt64(ByteReader& reader, std::optional<std::int64_t>& out) {
    bool present = false;
    std::int64_t value = 0;
    if (!be::readOptionalTag(reader, present)) {
        return false;
    }
    if (!present) {
        out.reset();
        return true;
    }
    if (!be::readInt64(reader, value)) {
        return false;
    }
    out = value;
    return true;
}

bool writeOptionalUint32(ByteWriter& writer,
                         const std::optional<std::uint32_t>& value) {
    return be::writeOptionalTag(writer, value.has_value()) &&
           (!value.has_value() || be::writeUint32(writer, *value));
}

bool readOptionalUint32(ByteReader& reader, std::optional<std::uint32_t>& out) {
    bool present = false;
    std::uint32_t value = 0U;
    if (!be::readOptionalTag(reader, present)) {
        return false;
    }
    if (!present) {
        out.reset();
        return true;
    }
    if (!be::readUint32(reader, value)) {
        return false;
    }
    out = value;
    return true;
}

bool writeOptionalUint64(ByteWriter& writer,
                         const std::optional<std::uint64_t>& value) {
    return be::writeOptionalTag(writer, value.has_value()) &&
           (!value.has_value() || be::writeUint64(writer, *value));
}

bool writeOptionalDouble(ByteWriter& writer,
                         const std::optional<double>& value) {
    return be::writeOptionalTag(writer, value.has_value()) &&
           (!value.has_value() ||
            device_platform::binary64::encode(*value, writer));
}

bool readOptionalDouble(ByteReader& reader, std::optional<double>& out) {
    bool present = false;
    double value = 0.0;
    if (!be::readOptionalTag(reader, present)) {
        return false;
    }
    if (!present) {
        out.reset();
        return true;
    }
    if (!device_platform::binary64::decode(reader, value)) {
        return false;
    }
    out = value;
    return true;
}

bool readOptionalUint64(ByteReader& reader, std::optional<std::uint64_t>& out) {
    bool present = false;
    std::uint64_t value = 0U;
    if (!be::readOptionalTag(reader, present)) {
        return false;
    }
    if (!present) {
        out.reset();
        return true;
    }
    if (!be::readUint64(reader, value)) {
        return false;
    }
    out = value;
    return true;
}

bool writeRunProgramSourceRevision(ByteWriter& writer,
                                   RunProgramSourceRevision value) {
    return be::writeUint64(writer, value.value());
}

// Stable schema-1 enum mapping: no C++ enum representation is serialized
// implicitly. The matching readers below reject every unassigned wire value.
bool writeEnum(ByteWriter& w, RunCheckpointVariant v) {
    switch (v) {
        case RunCheckpointVariant::ProgramRun:
            return be::writeUint8(w, 1U);
        case RunCheckpointVariant::ManualRun:
            return be::writeUint8(w, 2U);
        case RunCheckpointVariant::NoActiveRun:
            return be::writeUint8(w, 3U);
    }
    return false;
}
bool writeEnum(ByteWriter& w, RunCheckpointTrigger v) {
    switch (v) {
        case RunCheckpointTrigger::Command:
            return be::writeUint8(w, 1U);
        case RunCheckpointTrigger::Transition:
            return be::writeUint8(w, 2U);
        case RunCheckpointTrigger::Periodic:
            return be::writeUint8(w, 3U);
        case RunCheckpointTrigger::SensorSelection:
            return be::writeUint8(w, 4U);
    }
    return false;
}
bool writeEnum(ByteWriter& w, SensorSelectionProvenance v) {
    switch (v) {
        case SensorSelectionProvenance::InitialSelection:
            return be::writeUint8(w, 1U);
        case SensorSelectionProvenance::FallbackActive:
            return be::writeUint8(w, 2U);
        case SensorSelectionProvenance::ReturnedToProduct:
            return be::writeUint8(w, 3U);
        case SensorSelectionProvenance::LegacyUnknown:
            return be::writeUint8(w, 4U);
    }
    return false;
}
bool writeEnum(ByteWriter& w, SensorSelectionDecisionCause v) {
    switch (v) {
        case SensorSelectionDecisionCause::None:
            return be::writeUint8(w, 1U);
        case SensorSelectionDecisionCause::StartSelection:
            return be::writeUint8(w, 2U);
        case SensorSelectionDecisionCause::ProductFailureBlock:
            return be::writeUint8(w, 3U);
        case SensorSelectionDecisionCause::FallbackToAir:
            return be::writeUint8(w, 4U);
        case SensorSelectionDecisionCause::ManualUserFallback:
            return be::writeUint8(w, 5U);
        case SensorSelectionDecisionCause::AutomaticValidatedReturn:
            return be::writeUint8(w, 6U);
        case SensorSelectionDecisionCause::ManualUserReturn:
            return be::writeUint8(w, 7U);
        case SensorSelectionDecisionCause::RecoveryRevalidation:
            return be::writeUint8(w, 8U);
        case SensorSelectionDecisionCause::SafeStateEntry:
            return be::writeUint8(w, 9U);
        case SensorSelectionDecisionCause::ReturnValidationAborted:
            return be::writeUint8(w, 10U);
    }
    return false;
}
bool writeEnum(ByteWriter& w, RunSensorMode v) {
    switch (v) {
        case RunSensorMode::Product:
            return be::writeUint8(w, 1U);
        case RunSensorMode::Air:
            return be::writeUint8(w, 2U);
    }
    return false;
}
bool writeEnum(ByteWriter& w, CommandSource v) {
    switch (v) {
        case CommandSource::LocalDisplay:
            return be::writeUint8(w, 1U);
        case CommandSource::WebInterface:
            return be::writeUint8(w, 2U);
    }
    return false;
}
bool writeEnum(ByteWriter& w, ProcessKind v) {
    switch (v) {
        case ProcessKind::Timed:
            return be::writeUint8(w, 1U);
        case ProcessKind::ManualHolding:
            return be::writeUint8(w, 2U);
    }
    return false;
}
bool writeEnum(ByteWriter& w, CompletionMode v) {
    switch (v) {
        case CompletionMode::FinishWithoutCooling:
            return be::writeUint8(w, 1U);
        case CompletionMode::CoolThenFinish:
            return be::writeUint8(w, 2U);
        case CompletionMode::CoolAndHoldForDuration:
            return be::writeUint8(w, 3U);
        case CompletionMode::CoolAndHoldUntilManualStop:
            return be::writeUint8(w, 4U);
    }
    return false;
}
bool writeEnum(ByteWriter& w, ProcessState v) {
    switch (v) {
        case ProcessState::Boot:
            return be::writeUint8(w, 1U);
        case ProcessState::SafeBoot:
            return be::writeUint8(w, 2U);
        case ProcessState::Standby:
            return be::writeUint8(w, 3U);
        case ProcessState::Preheating:
            return be::writeUint8(w, 4U);
        case ProcessState::WaitingForProduct:
            return be::writeUint8(w, 5U);
        case ProcessState::ReachingTarget:
            return be::writeUint8(w, 6U);
        case ProcessState::QualifyingTarget:
            return be::writeUint8(w, 7U);
        case ProcessState::Fermenting:
            return be::writeUint8(w, 8U);
        case ProcessState::Cooling:
            return be::writeUint8(w, 9U);
        case ProcessState::CoolHolding:
            return be::writeUint8(w, 10U);
        case ProcessState::ManualHolding:
            return be::writeUint8(w, 11U);
        case ProcessState::Completed:
            return be::writeUint8(w, 12U);
        case ProcessState::RecoveryEvaluation:
            return be::writeUint8(w, 13U);
        case ProcessState::Fault:
            return be::writeUint8(w, 14U);
        case ProcessState::ServiceMode:
            return be::writeUint8(w, 15U);
    }
    return false;
}
bool writeEnum(ByteWriter& w, ProgramSourceKind v) {
    switch (v) {
        case ProgramSourceKind::FactoryCatalog:
            return be::writeUint8(w, 1U);
        case ProgramSourceKind::UserProgram:
            return be::writeUint8(w, 2U);
        case ProgramSourceKind::ManualTimed:
            return be::writeUint8(w, 3U);
    }
    return false;
}
bool writeEnum(ByteWriter& w, RunAdjustmentEffect v) {
    switch (v) {
        case RunAdjustmentEffect::None:
            return be::writeUint8(w, 1U);
        case RunAdjustmentEffect::RestartTargetQualification:
            return be::writeUint8(w, 2U);
        case RunAdjustmentEffect::ContinueFermentationWithoutRequalification:
            return be::writeUint8(w, 3U);
    }
    return false;
}
bool writeEnum(ByteWriter& w, RunChangeSource v) {
    switch (v) {
        case RunChangeSource::LocalDisplay:
            return be::writeUint8(w, 1U);
        case RunChangeSource::WebInterface:
            return be::writeUint8(w, 2U);
        case RunChangeSource::Recovery:
            return be::writeUint8(w, 3U);
    }
    return false;
}
bool writeEnum(ByteWriter& w, RunChangeReason v) {
    switch (v) {
        case RunChangeReason::UserAdjustment:
            return be::writeUint8(w, 1U);
        case RunChangeReason::RecoveryCorrection:
            return be::writeUint8(w, 2U);
    }
    return false;
}
bool writeEnum(ByteWriter& w, device_platform::SensorQuality v) {
    switch (v) {
        case device_platform::SensorQuality::Valid:
            return be::writeUint8(w, 1U);
        case device_platform::SensorQuality::Stale:
            return be::writeUint8(w, 2U);
        case device_platform::SensorQuality::Failed:
            return be::writeUint8(w, 3U);
    }
    return false;
}
bool writeEnum(ByteWriter& w, RunProgressBasis v) {
    switch (v) {
        case RunProgressBasis::KnownTotal:
            return be::writeUint8(w, 1U);
        case RunProgressBasis::PartialUnknownHistory:
            return be::writeUint8(w, 2U);
    }
    return false;
}
bool writeEnum(ByteWriter& w, WeightedProgressConfidence v) {
    switch (v) {
        case WeightedProgressConfidence::ProductPreferred:
            return be::writeUint8(w, 1U);
        case WeightedProgressConfidence::AirReduced:
            return be::writeUint8(w, 2U);
    }
    return false;
}
bool writeEnum(ByteWriter& w, WeightedProgressCoverage v) {
    switch (v) {
        case WeightedProgressCoverage::Complete:
            return be::writeUint8(w, 1U);
        case WeightedProgressCoverage::PartialUnknown:
            return be::writeUint8(w, 2U);
    }
    return false;
}

bool readVariant(ByteReader& reader, RunCheckpointVariant& out) {
    std::uint8_t value = 0U;
    if (!be::readUint8(reader, value)) return false;
    switch (value) {
        case 1U:
            out = RunCheckpointVariant::ProgramRun;
            return true;
        case 2U:
            out = RunCheckpointVariant::ManualRun;
            return true;
        case 3U:
            out = RunCheckpointVariant::NoActiveRun;
            return true;
        default:
            return false;
    }
}
bool readTrigger(ByteReader& reader, RunCheckpointTrigger& out) {
    std::uint8_t value = 0U;
    if (!be::readUint8(reader, value)) return false;
    switch (value) {
        case 1U:
            out = RunCheckpointTrigger::Command;
            return true;
        case 2U:
            out = RunCheckpointTrigger::Transition;
            return true;
        case 3U:
            out = RunCheckpointTrigger::Periodic;
            return true;
        case 4U:
            out = RunCheckpointTrigger::SensorSelection;
            return true;
        default:
            return false;
    }
}
bool readProvenance(ByteReader& reader, SensorSelectionProvenance& out) {
    std::uint8_t value = 0U;
    if (!be::readUint8(reader, value)) return false;
    switch (value) {
        case 1U:
            out = SensorSelectionProvenance::InitialSelection;
            return true;
        case 2U:
            out = SensorSelectionProvenance::FallbackActive;
            return true;
        case 3U:
            out = SensorSelectionProvenance::ReturnedToProduct;
            return true;
        case 4U:
            out = SensorSelectionProvenance::LegacyUnknown;
            return true;
        default:
            return false;
    }
}
bool readDecisionCause(ByteReader& reader, SensorSelectionDecisionCause& out) {
    std::uint8_t value = 0U;
    if (!be::readUint8(reader, value)) return false;
    switch (value) {
        case 1U:
            out = SensorSelectionDecisionCause::None;
            return true;
        case 2U:
            out = SensorSelectionDecisionCause::StartSelection;
            return true;
        case 3U:
            out = SensorSelectionDecisionCause::ProductFailureBlock;
            return true;
        case 4U:
            out = SensorSelectionDecisionCause::FallbackToAir;
            return true;
        case 5U:
            out = SensorSelectionDecisionCause::ManualUserFallback;
            return true;
        case 6U:
            out = SensorSelectionDecisionCause::AutomaticValidatedReturn;
            return true;
        case 7U:
            out = SensorSelectionDecisionCause::ManualUserReturn;
            return true;
        case 8U:
            out = SensorSelectionDecisionCause::RecoveryRevalidation;
            return true;
        case 9U:
            out = SensorSelectionDecisionCause::SafeStateEntry;
            return true;
        case 10U:
            out = SensorSelectionDecisionCause::ReturnValidationAborted;
            return true;
        default:
            return false;
    }
}
bool writePersistedSensorSelectionState(
    ByteWriter& writer, const PersistedSensorSelectionState& s) {
    return writeEnum(writer, s.provenance) &&
           writeEnum(writer, s.lastDecisionCause) &&
           be::writeUint32(writer, s.lastDecisionRunRevision);
}
bool readPersistedSensorSelectionState(ByteReader& reader,
                                       PersistedSensorSelectionState& s) {
    return readProvenance(reader, s.provenance) &&
           readDecisionCause(reader, s.lastDecisionCause) &&
           be::readUint32(reader, s.lastDecisionRunRevision);
}
bool readSensor(ByteReader& reader, RunSensorMode& out) {
    std::uint8_t value = 0U;
    if (!be::readUint8(reader, value)) return false;
    switch (value) {
        case 1U:
            out = RunSensorMode::Product;
            return true;
        case 2U:
            out = RunSensorMode::Air;
            return true;
        default:
            return false;
    }
}
bool readCommandSource(ByteReader& reader, CommandSource& out) {
    std::uint8_t value = 0U;
    if (!be::readUint8(reader, value)) return false;
    switch (value) {
        case 1U:
            out = CommandSource::LocalDisplay;
            return true;
        case 2U:
            out = CommandSource::WebInterface;
            return true;
        default:
            return false;
    }
}
bool readProcessKind(ByteReader& reader, ProcessKind& out) {
    std::uint8_t value = 0U;
    if (!be::readUint8(reader, value)) return false;
    switch (value) {
        case 1U:
            out = ProcessKind::Timed;
            return true;
        case 2U:
            out = ProcessKind::ManualHolding;
            return true;
        default:
            return false;
    }
}
bool readCompletion(ByteReader& reader, CompletionMode& out) {
    std::uint8_t value = 0U;
    if (!be::readUint8(reader, value)) return false;
    switch (value) {
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
bool readProcessState(ByteReader& reader, ProcessState& out) {
    std::uint8_t value = 0U;
    if (!be::readUint8(reader, value)) return false;
    switch (value) {
        case 1U:
            out = ProcessState::Boot;
            return true;
        case 2U:
            out = ProcessState::SafeBoot;
            return true;
        case 3U:
            out = ProcessState::Standby;
            return true;
        case 4U:
            out = ProcessState::Preheating;
            return true;
        case 5U:
            out = ProcessState::WaitingForProduct;
            return true;
        case 6U:
            out = ProcessState::ReachingTarget;
            return true;
        case 7U:
            out = ProcessState::QualifyingTarget;
            return true;
        case 8U:
            out = ProcessState::Fermenting;
            return true;
        case 9U:
            out = ProcessState::Cooling;
            return true;
        case 10U:
            out = ProcessState::CoolHolding;
            return true;
        case 11U:
            out = ProcessState::ManualHolding;
            return true;
        case 12U:
            out = ProcessState::Completed;
            return true;
        case 13U:
            out = ProcessState::RecoveryEvaluation;
            return true;
        case 14U:
            out = ProcessState::Fault;
            return true;
        case 15U:
            out = ProcessState::ServiceMode;
            return true;
        default:
            return false;
    }
}
bool readProgramSource(ByteReader& reader, ProgramSourceKind& out) {
    std::uint8_t value = 0U;
    if (!be::readUint8(reader, value)) return false;
    switch (value) {
        case 1U:
            out = ProgramSourceKind::FactoryCatalog;
            return true;
        case 2U:
            out = ProgramSourceKind::UserProgram;
            return true;
        case 3U:
            out = ProgramSourceKind::ManualTimed;
            return true;
        default:
            return false;
    }
}

bool writeManualTimedSource(ByteWriter& writer,
                            const ManualTimedRunSource& source) {
    return writeOptionalDouble(writer, source.stage.targetTemperatureCelsius) &&
           writeOptionalUint32(writer, source.stage.durationMinutes) &&
           be::writeBool(writer, source.preheatEnabled) &&
           writeOptionalUint32(writer, source.maximumProductWaitMinutes) &&
           writeOptionalDouble(writer,
                               source.targetQualification.bandCelsius) &&
           writeOptionalUint32(writer,
                               source.targetQualification.durationMinutes) &&
           writeOptionalUint32(writer, source.maximumTargetReachMinutes) &&
           writeEnum(writer, source.completion.mode) &&
           writeOptionalDouble(writer,
                               source.completion.coolingTargetCelsius) &&
           writeOptionalUint32(writer, source.completion.holdDurationMinutes);
}

bool readManualTimedSource(ByteReader& reader, ManualTimedRunSource& source) {
    return readOptionalDouble(reader, source.stage.targetTemperatureCelsius) &&
           readOptionalUint32(reader, source.stage.durationMinutes) &&
           be::readBool(reader, source.preheatEnabled) &&
           readOptionalUint32(reader, source.maximumProductWaitMinutes) &&
           readOptionalDouble(reader, source.targetQualification.bandCelsius) &&
           readOptionalUint32(reader,
                              source.targetQualification.durationMinutes) &&
           readOptionalUint32(reader, source.maximumTargetReachMinutes) &&
           readCompletion(reader, source.completion.mode) &&
           readOptionalDouble(reader, source.completion.coolingTargetCelsius) &&
           readOptionalUint32(reader, source.completion.holdDurationMinutes);
}
bool readEffect(ByteReader& reader, RunAdjustmentEffect& out) {
    std::uint8_t value = 0U;
    if (!be::readUint8(reader, value)) return false;
    switch (value) {
        case 1U:
            out = RunAdjustmentEffect::None;
            return true;
        case 2U:
            out = RunAdjustmentEffect::RestartTargetQualification;
            return true;
        case 3U:
            out =
                RunAdjustmentEffect::ContinueFermentationWithoutRequalification;
            return true;
        default:
            return false;
    }
}
bool readChangeSource(ByteReader& reader, RunChangeSource& out) {
    std::uint8_t value = 0U;
    if (!be::readUint8(reader, value)) return false;
    switch (value) {
        case 1U:
            out = RunChangeSource::LocalDisplay;
            return true;
        case 2U:
            out = RunChangeSource::WebInterface;
            return true;
        case 3U:
            out = RunChangeSource::Recovery;
            return true;
        default:
            return false;
    }
}
bool readChangeReason(ByteReader& reader, RunChangeReason& out) {
    std::uint8_t value = 0U;
    if (!be::readUint8(reader, value)) return false;
    switch (value) {
        case 1U:
            out = RunChangeReason::UserAdjustment;
            return true;
        case 2U:
            out = RunChangeReason::RecoveryCorrection;
            return true;
        default:
            return false;
    }
}
bool readQuality(ByteReader& reader, device_platform::SensorQuality& out) {
    std::uint8_t value = 0U;
    if (!be::readUint8(reader, value)) return false;
    switch (value) {
        case 1U:
            out = device_platform::SensorQuality::Valid;
            return true;
        case 2U:
            out = device_platform::SensorQuality::Stale;
            return true;
        case 3U:
            out = device_platform::SensorQuality::Failed;
            return true;
        default:
            return false;
    }
}
bool readProgressBasis(ByteReader& reader, RunProgressBasis& out) {
    std::uint8_t value = 0U;
    if (!be::readUint8(reader, value)) return false;
    switch (value) {
        case 1U:
            out = RunProgressBasis::KnownTotal;
            return true;
        case 2U:
            out = RunProgressBasis::PartialUnknownHistory;
            return true;
        default:
            return false;
    }
}
bool readWeightedProgressConfidence(ByteReader& reader,
                                    WeightedProgressConfidence& out) {
    std::uint8_t value = 0U;
    if (!be::readUint8(reader, value)) return false;
    switch (value) {
        case 1U:
            out = WeightedProgressConfidence::ProductPreferred;
            return true;
        case 2U:
            out = WeightedProgressConfidence::AirReduced;
            return true;
        default:
            return false;
    }
}
bool readWeightedProgressCoverage(ByteReader& reader,
                                  WeightedProgressCoverage& out) {
    std::uint8_t value = 0U;
    if (!be::readUint8(reader, value)) return false;
    switch (value) {
        case 1U:
            out = WeightedProgressCoverage::Complete;
            return true;
        case 2U:
            out = WeightedProgressCoverage::PartialUnknown;
            return true;
        default:
            return false;
    }
}

bool writeValues(ByteWriter& writer, const EffectiveRunValues& values) {
    return device_platform::binary64::encode(values.targetTemperatureCelsius,
                                             writer) &&
           be::writeUint32(writer, values.remainingDurationMinutes);
}
bool readValues(ByteReader& reader, EffectiveRunValues& values) {
    return device_platform::binary64::decode(reader,
                                             values.targetTemperatureCelsius) &&
           be::readUint32(reader, values.remainingDurationMinutes);
}
bool writeRevision(ByteWriter& writer, const RunRevision& r) {
    if (r.stageIndex > std::numeric_limits<std::uint32_t>::max() ||
        r.completedStageCount > std::numeric_limits<std::uint32_t>::max())
        return false;
    return be::writeUint32(writer, r.sequence) &&
           be::writeUint32(writer, r.monotonicEpoch) &&
           be::writeUint32(writer, static_cast<std::uint32_t>(r.stageIndex)) &&
           be::writeUint32(writer,
                           static_cast<std::uint32_t>(r.completedStageCount)) &&
           writeValues(writer, r.before) && writeValues(writer, r.after) &&
           be::writeBool(writer, r.targetTemperatureChanged) &&
           be::writeBool(writer, r.remainingDurationChanged) &&
           writeEnum(writer, r.effect) && writeEnum(writer, r.source) &&
           writeEnum(writer, r.reason) &&
           be::writeUint64(writer, r.timestamp.monotonicMillis) &&
           writeOptionalInt64(writer, r.timestamp.unixTimeSeconds);
}
bool readRevision(ByteReader& reader, RunRevision& r) {
    std::uint32_t index = 0U, completed = 0U;
    return be::readUint32(reader, r.sequence) &&
           be::readUint32(reader, r.monotonicEpoch) &&
           be::readUint32(reader, index) && be::readUint32(reader, completed) &&
           readValues(reader, r.before) && readValues(reader, r.after) &&
           be::readBool(reader, r.targetTemperatureChanged) &&
           be::readBool(reader, r.remainingDurationChanged) &&
           readEffect(reader, r.effect) && readChangeSource(reader, r.source) &&
           readChangeReason(reader, r.reason) &&
           be::readUint64(reader, r.timestamp.monotonicMillis) &&
           readOptionalInt64(reader, r.timestamp.unixTimeSeconds) &&
           ((r.stageIndex = index), (r.completedStageCount = completed), true);
}
bool writeProcessSnapshot(ByteWriter& writer, const ProcessRunSnapshot& p) {
    return writeEnum(writer, p.kind) &&
           be::writeBool(writer, p.preheatEnabled) &&
           writeEnum(writer, p.completionMode) &&
           be::writeUint32(writer, p.qualificationDurationMinutes) &&
           be::writeUint32(writer, p.maximumTargetReachMinutes) &&
           writeOptionalUint32(writer, p.maximumProductWaitMinutes) &&
           writeOptionalUint32(writer, p.fermentationDurationMinutes) &&
           writeOptionalUint32(writer, p.holdDurationMinutes);
}
bool readProcessSnapshot(ByteReader& reader, ProcessRunSnapshot& p) {
    return readProcessKind(reader, p.kind) &&
           be::readBool(reader, p.preheatEnabled) &&
           readCompletion(reader, p.completionMode) &&
           be::readUint32(reader, p.qualificationDurationMinutes) &&
           be::readUint32(reader, p.maximumTargetReachMinutes) &&
           readOptionalUint32(reader, p.maximumProductWaitMinutes) &&
           readOptionalUint32(reader, p.fermentationDurationMinutes) &&
           readOptionalUint32(reader, p.holdDurationMinutes) &&
           validateProcessRunSnapshot(p);
}
bool writeRuntime(ByteWriter& writer, const ProcessRuntimeState& p) {
    return writeEnum(writer, p.state) &&
           be::writeUint64(writer, p.stateEnteredAtMillis) &&
           be::writeUint64(writer, p.targetReachStartedAtMillis) &&
           writeOptionalUint64(writer, p.qualificationValidSinceMillis) &&
           be::writeBool(writer, p.targetReachWarningIssued) &&
           be::writeUint32(writer, p.transitionSequence);
}
bool readRuntime(ByteReader& reader, ProcessRuntimeState& p) {
    return readProcessState(reader, p.state) &&
           be::readUint64(reader, p.stateEnteredAtMillis) &&
           be::readUint64(reader, p.targetReachStartedAtMillis) &&
           readOptionalUint64(reader, p.qualificationValidSinceMillis) &&
           be::readBool(reader, p.targetReachWarningIssued) &&
           be::readUint32(reader, p.transitionSequence);
}
bool writeManual(ByteWriter& writer, const ManualRunPlan& p) {
    const auto& v = p.values;
    return device_platform::binary64::encode(v.targetTemperatureCelsius,
                                             writer) &&
           writeEnum(writer, v.sensorMode) &&
           be::writeBool(writer, v.preheatEnabled) &&
           writeOptionalUint32(writer, v.maximumProductWaitMinutes) &&
           device_platform::binary64::encode(v.qualificationBandCelsius,
                                             writer) &&
           be::writeUint32(writer, v.qualificationDurationMinutes) &&
           be::writeUint32(writer, v.maximumTargetReachMinutes) &&
           writeEnum(writer, p.source) &&
           be::writeUint64(writer, p.createdAtMonotonicMillis) &&
           writeEnum(writer, p.kind);
}
bool readManual(ByteReader& reader, const std::string& id, ManualRunPlan& p) {
    return device_platform::binary64::decode(
               reader, p.values.targetTemperatureCelsius) &&
           readSensor(reader, p.values.sensorMode) &&
           be::readBool(reader, p.values.preheatEnabled) &&
           readOptionalUint32(reader, p.values.maximumProductWaitMinutes) &&
           device_platform::binary64::decode(
               reader, p.values.qualificationBandCelsius) &&
           be::readUint32(reader, p.values.qualificationDurationMinutes) &&
           be::readUint32(reader, p.values.maximumTargetReachMinutes) &&
           readCommandSource(reader, p.source) &&
           be::readUint64(reader, p.createdAtMonotonicMillis) &&
           readProcessKind(reader, p.kind) && ((p.values.runId = id), true);
}

// Schema 3 (#18, 5.20/5.12/5.23/5.22/5.21): Recovery-/Progressfeldblock,
// unbedingt geschrieben (encode() erzeugt ausschliesslich
// kCurrentRunPersistenceSchema-Payloads) und schemabewacht gelesen (s.
// decodeRunPersistenceSnapshot, kRecoveryFieldsIntroducedInSchema).
bool writeRoleEvidence(ByteWriter& writer, const RoleTemperatureEvidence& v) {
    return writeOptionalDouble(writer, v.filteredCelsius) &&
           writeEnum(writer, v.quality);
}
bool readRoleEvidence(ByteReader& reader, RoleTemperatureEvidence& v) {
    return readOptionalDouble(reader, v.filteredCelsius) &&
           readQuality(reader, v.quality);
}
bool writeCrossRoleEvidence(ByteWriter& writer, const CrossRoleEvidence& v) {
    return writeRoleEvidence(writer, v.air) &&
           writeRoleEvidence(writer, v.product) &&
           writeRoleEvidence(writer, v.cooling);
}
bool readCrossRoleEvidence(ByteReader& reader, CrossRoleEvidence& v) {
    return readRoleEvidence(reader, v.air) &&
           readRoleEvidence(reader, v.product) &&
           readRoleEvidence(reader, v.cooling);
}
bool writeOptionalRoleEvidence(
    ByteWriter& writer, const std::optional<RoleTemperatureEvidence>& v) {
    return be::writeOptionalTag(writer, v.has_value()) &&
           (!v.has_value() || writeRoleEvidence(writer, *v));
}
bool readOptionalRoleEvidence(ByteReader& reader,
                              std::optional<RoleTemperatureEvidence>& out) {
    bool present = false;
    if (!be::readOptionalTag(reader, present)) return false;
    if (!present) {
        out.reset();
        return true;
    }
    RoleTemperatureEvidence value;
    if (!readRoleEvidence(reader, value)) return false;
    out = value;
    return true;
}
bool writeFirstAfterRestartEvidence(ByteWriter& writer,
                                    const FirstAfterRestartEvidence& v) {
    return writeOptionalRoleEvidence(writer, v.air) &&
           writeOptionalRoleEvidence(writer, v.product) &&
           writeOptionalRoleEvidence(writer, v.cooling);
}
bool readFirstAfterRestartEvidence(ByteReader& reader,
                                   FirstAfterRestartEvidence& v) {
    return readOptionalRoleEvidence(reader, v.air) &&
           readOptionalRoleEvidence(reader, v.product) &&
           readOptionalRoleEvidence(reader, v.cooling);
}
bool writeRecoveryEpisodeEvidence(ByteWriter& writer,
                                  const RecoveryEpisodeEvidence& v) {
    return writeCrossRoleEvidence(writer, v.beforeOutage) &&
           writeFirstAfterRestartEvidence(writer, v.firstAfterRestart) &&
           writeOptionalUint32(writer, v.weightedProgressSegmentId);
}
bool readRecoveryEpisodeEvidence(ByteReader& reader,
                                 RecoveryEpisodeEvidence& v) {
    return readCrossRoleEvidence(reader, v.beforeOutage) &&
           readFirstAfterRestartEvidence(reader, v.firstAfterRestart) &&
           readOptionalUint32(reader, v.weightedProgressSegmentId);
}
bool writeOptionalRecoveryEpisodeEvidence(
    ByteWriter& writer, const std::optional<RecoveryEpisodeEvidence>& v) {
    return be::writeOptionalTag(writer, v.has_value()) &&
           (!v.has_value() || writeRecoveryEpisodeEvidence(writer, *v));
}
bool readOptionalRecoveryEpisodeEvidence(
    ByteReader& reader, std::optional<RecoveryEpisodeEvidence>& out) {
    bool present = false;
    if (!be::readOptionalTag(reader, present)) return false;
    if (!present) {
        out.reset();
        return true;
    }
    RecoveryEpisodeEvidence value;
    if (!readRecoveryEpisodeEvidence(reader, value)) return false;
    out = value;
    return true;
}
bool writePendingRecoveryAnchor(ByteWriter& writer,
                                const PendingRecoveryAnchor& v) {
    return writeRuntime(writer, v.originalProcessState) &&
           be::writeUint64(writer, v.knownPhaseSecondsAtOriginalCheckpoint) &&
           writeOptionalInt64(writer, v.originalCheckpointUtc) &&
           writeEnum(writer, v.originalCheckpointTrigger) &&
           be::writeUint32(writer, v.originalCheckpointIntervalMinutes) &&
           be::writeUint32(writer,
                           v.accumulatedBeforeEpisode.lowerBoundSeconds) &&
           writeOptionalUint32(writer,
                               v.accumulatedBeforeEpisode.upperBoundSeconds) &&
           be::writeUint64(writer, v.knownSecondsSinceOriginalCheckpoint);
}
bool readPendingRecoveryAnchor(ByteReader& reader, PendingRecoveryAnchor& v) {
    return readRuntime(reader, v.originalProcessState) &&
           be::readUint64(reader, v.knownPhaseSecondsAtOriginalCheckpoint) &&
           readOptionalInt64(reader, v.originalCheckpointUtc) &&
           readTrigger(reader, v.originalCheckpointTrigger) &&
           be::readUint32(reader, v.originalCheckpointIntervalMinutes) &&
           be::readUint32(reader,
                          v.accumulatedBeforeEpisode.lowerBoundSeconds) &&
           readOptionalUint32(reader,
                              v.accumulatedBeforeEpisode.upperBoundSeconds) &&
           be::readUint64(reader, v.knownSecondsSinceOriginalCheckpoint);
}
bool writeOptionalPendingRecoveryAnchor(
    ByteWriter& writer, const std::optional<PendingRecoveryAnchor>& v) {
    return be::writeOptionalTag(writer, v.has_value()) &&
           (!v.has_value() || writePendingRecoveryAnchor(writer, *v));
}
bool readOptionalPendingRecoveryAnchor(
    ByteReader& reader, std::optional<PendingRecoveryAnchor>& out) {
    bool present = false;
    if (!be::readOptionalTag(reader, present)) return false;
    if (!present) {
        out.reset();
        return true;
    }
    PendingRecoveryAnchor value;
    if (!readPendingRecoveryAnchor(reader, value)) return false;
    out = value;
    return true;
}
bool writeTaggedPriorBootPhaseElapsed(ByteWriter& writer,
                                      const TaggedPriorBootPhaseElapsed& v) {
    return writeEnum(writer, v.taggedState) &&
           be::writeUint32(writer, v.elapsed.lowerBoundSeconds) &&
           writeOptionalUint32(writer, v.elapsed.upperBoundSeconds);
}
bool readTaggedPriorBootPhaseElapsed(ByteReader& reader,
                                     TaggedPriorBootPhaseElapsed& v) {
    return readProcessState(reader, v.taggedState) &&
           be::readUint32(reader, v.elapsed.lowerBoundSeconds) &&
           readOptionalUint32(reader, v.elapsed.upperBoundSeconds);
}
bool writeOptionalTaggedPriorBootPhaseElapsed(
    ByteWriter& writer, const std::optional<TaggedPriorBootPhaseElapsed>& v) {
    return be::writeOptionalTag(writer, v.has_value()) &&
           (!v.has_value() || writeTaggedPriorBootPhaseElapsed(writer, *v));
}
bool readOptionalTaggedPriorBootPhaseElapsed(
    ByteReader& reader, std::optional<TaggedPriorBootPhaseElapsed>& out) {
    bool present = false;
    if (!be::readOptionalTag(reader, present)) return false;
    if (!present) {
        out.reset();
        return true;
    }
    TaggedPriorBootPhaseElapsed value;
    if (!readTaggedPriorBootPhaseElapsed(reader, value)) return false;
    out = value;
    return true;
}
bool writeNominalRecoveryAdjustmentState(
    ByteWriter& writer, const NominalRecoveryAdjustmentState& v) {
    return be::writeUint32(writer, v.cumulativeAppliedSeconds) &&
           be::writeUint32(writer, v.lastAppliedEpisodeRevision) &&
           be::writeUint32(writer, v.lastAppliedEpisodeDelta);
}
bool readNominalRecoveryAdjustmentState(ByteReader& reader,
                                        NominalRecoveryAdjustmentState& v) {
    return be::readUint32(reader, v.cumulativeAppliedSeconds) &&
           be::readUint32(reader, v.lastAppliedEpisodeRevision) &&
           be::readUint32(reader, v.lastAppliedEpisodeDelta);
}
bool writeOptionalNominalRecoveryAdjustmentState(
    ByteWriter& writer,
    const std::optional<NominalRecoveryAdjustmentState>& v) {
    return be::writeOptionalTag(writer, v.has_value()) &&
           (!v.has_value() || writeNominalRecoveryAdjustmentState(writer, *v));
}
bool readOptionalNominalRecoveryAdjustmentState(
    ByteReader& reader, std::optional<NominalRecoveryAdjustmentState>& out) {
    bool present = false;
    if (!be::readOptionalTag(reader, present)) return false;
    if (!present) {
        out.reset();
        return true;
    }
    NominalRecoveryAdjustmentState value;
    if (!readNominalRecoveryAdjustmentState(reader, value)) return false;
    out = value;
    return true;
}
bool writeWeightedProgressProvenance(ByteWriter& writer,
                                     const WeightedProgressProvenance& v) {
    return writeEnum(writer, v.lastSourceRole) &&
           writeEnum(writer, v.confidence) &&
           be::writeUint32(writer, v.modelRevision) &&
           be::writeUint32(writer, v.lastAppliedSegmentId);
}
bool readWeightedProgressProvenance(ByteReader& reader,
                                    WeightedProgressProvenance& v) {
    return readSensor(reader, v.lastSourceRole) &&
           readWeightedProgressConfidence(reader, v.confidence) &&
           be::readUint32(reader, v.modelRevision) &&
           be::readUint32(reader, v.lastAppliedSegmentId);
}
bool writeOptionalWeightedProgressProvenance(
    ByteWriter& writer, const std::optional<WeightedProgressProvenance>& v) {
    return be::writeOptionalTag(writer, v.has_value()) &&
           (!v.has_value() || writeWeightedProgressProvenance(writer, *v));
}
bool readOptionalWeightedProgressProvenance(
    ByteReader& reader, std::optional<WeightedProgressProvenance>& out) {
    bool present = false;
    if (!be::readOptionalTag(reader, present)) return false;
    if (!present) {
        out.reset();
        return true;
    }
    WeightedProgressProvenance value;
    if (!readWeightedProgressProvenance(reader, value)) return false;
    out = value;
    return true;
}
bool writeWeightedProgressState(ByteWriter& writer,
                                const WeightedProgressState& v) {
    return be::writeUint64(writer, v.cumulative.lowerBoundSeconds) &&
           writeOptionalUint64(writer, v.cumulative.upperBoundSeconds) &&
           writeEnum(writer, v.coverage) &&
           writeOptionalWeightedProgressProvenance(writer, v.lastApplied);
}
bool readWeightedProgressState(ByteReader& reader, WeightedProgressState& v) {
    return be::readUint64(reader, v.cumulative.lowerBoundSeconds) &&
           readOptionalUint64(reader, v.cumulative.upperBoundSeconds) &&
           readWeightedProgressCoverage(reader, v.coverage) &&
           readOptionalWeightedProgressProvenance(reader, v.lastApplied);
}
bool writeOptionalWeightedProgressState(
    ByteWriter& writer, const std::optional<WeightedProgressState>& v) {
    return be::writeOptionalTag(writer, v.has_value()) &&
           (!v.has_value() || writeWeightedProgressState(writer, *v));
}
bool readOptionalWeightedProgressState(
    ByteReader& reader, std::optional<WeightedProgressState>& out) {
    bool present = false;
    if (!be::readOptionalTag(reader, present)) return false;
    if (!present) {
        out.reset();
        return true;
    }
    WeightedProgressState value;
    if (!readWeightedProgressState(reader, value)) return false;
    out = value;
    return true;
}
bool writeRunProgressState(ByteWriter& writer, const RunProgressState& v) {
    return writeEnum(writer, v.basis) &&
           be::writeUint32(writer, v.observedRunSeconds) &&
           writeOptionalWeightedProgressState(writer, v.weightedProgress);
}
bool readRunProgressState(ByteReader& reader, RunProgressState& v) {
    return readProgressBasis(reader, v.basis) &&
           be::readUint32(reader, v.observedRunSeconds) &&
           readOptionalWeightedProgressState(reader, v.weightedProgress);
}

}  // namespace

RunPersistenceCodecStatus encodeRunPersistenceSnapshot(
    const RunPersistenceSnapshot& snapshot, std::string& out) {
    if (!validateRunPersistenceSnapshot(snapshot))
        return RunPersistenceCodecStatus::InvalidSnapshot;
    ByteWriter writer(kMaximumCheckpointPayloadBytes);
    bool ok = writeEnum(writer, snapshot.variant) &&
              writeEnum(writer, snapshot.trigger) &&
              be::writeUint64(writer, snapshot.checkpointMonotonicMillis) &&
              be::writeUint16(writer, snapshot.intervalMinutes) &&
              be::writeUint32(writer, snapshot.runRevision) &&
              writeString(writer, snapshot.activeRunId);
    if (snapshot.variant != RunCheckpointVariant::NoActiveRun) {
        ok = ok && writeEnum(writer, *snapshot.activeRunSensorMode) &&
             be::writeOptionalTag(writer,
                                  snapshot.sensorSelection.has_value()) &&
             (!snapshot.sensorSelection.has_value() ||
              writePersistedSensorSelectionState(writer,
                                                 *snapshot.sensorSelection));
    }
    if (snapshot.variant == RunCheckpointVariant::ProgramRun) {
        if (snapshot.program->sourceKind == ProgramSourceKind::ManualTimed) {
            ok = ok && writeEnum(writer, snapshot.program->sourceKind) &&
                 be::writeOptionalTag(
                     writer,
                     snapshot.program->sourceProgramRevision.has_value()) &&
                 (!snapshot.program->sourceProgramRevision.has_value() &&
                  manualTimedSource(snapshot.program->source) != nullptr) &&
                 writeManualTimedSource(
                     writer, *manualTimedSource(snapshot.program->source));
        } else {
            std::string program;
            ok = ok && writeEnum(writer, snapshot.program->sourceKind) &&
                 be::writeOptionalTag(
                     writer,
                     snapshot.program->sourceProgramRevision.has_value()) &&
                 (snapshot.program->sourceProgramRevision.has_value() &&
                  writeRunProgramSourceRevision(
                      writer, *snapshot.program->sourceProgramRevision)) &&
                 encodeProgramDocumentPayload(
                     *storedProgram(snapshot.program->source), program) ==
                     ConfigurationCodecStatus::Success &&
                 writeString(writer, program);
        }
        ok = ok && be::writeUint8(writer, static_cast<std::uint8_t>(
                                              snapshot.revisionCount));
        for (std::size_t i = 0U; ok && i < snapshot.revisionCount; ++i)
            ok = writeRevision(writer, snapshot.revisions[i]);
    } else if (snapshot.variant == RunCheckpointVariant::ManualRun) {
        ok = ok && writeManual(writer, *snapshot.manual);
    }
    if (snapshot.variant != RunCheckpointVariant::NoActiveRun)
        ok = ok && writeProcessSnapshot(writer, *snapshot.processRunSnapshot);
    ok = ok && writeRuntime(writer, snapshot.processState) &&
         be::writeUint8(writer, static_cast<std::uint8_t>(
                                    snapshot.persistedRunCommandCount));
    for (std::size_t i = 0U; ok && i < snapshot.persistedRunCommandCount; ++i)
        ok = be::writeUint64(writer, snapshot.persistedRunCommandIds[i]);
    // Schema 3 (#18, 5.28): immer angehaengt, encode() erzeugt ausschliesslich
    // kCurrentRunPersistenceSchema-Payloads. Am Ende des Payloads, damit der
    // Schema-1/2-Lesepfad byte-identisch bleibt (s.
    // decodeRunPersistenceSnapshot).
    ok = ok &&
         writeOptionalPendingRecoveryAnchor(writer,
                                            snapshot.pendingRecoveryAnchor) &&
         writeOptionalUint64(writer,
                             snapshot.recoveryBootAnchorMonotonicMillis) &&
         writeCrossRoleEvidence(
             writer, snapshot.recoveryTemperatureEvidence.lastKnown) &&
         writeOptionalRecoveryEpisodeEvidence(
             writer, snapshot.lastRecoveryEpisodeEvidence) &&
         writeOptionalTaggedPriorBootPhaseElapsed(
             writer, snapshot.priorBootPhaseElapsed) &&
         writeOptionalNominalRecoveryAdjustmentState(
             writer, snapshot.nominalRecoveryAdjustment) &&
         be::writeUint32(writer, snapshot.recoveryEpisodeRevision) &&
         writeRunProgressState(writer, snapshot.runProgress);
    if (!ok) return RunPersistenceCodecStatus::CapacityExceeded;
    auto encoded = writer.takeBytes();
    out.swap(encoded);
    return RunPersistenceCodecStatus::Success;
}

RunPersistenceCodecStatus decodeRunPersistenceSnapshotInto(
    const std::string& payload, std::uint32_t schemaVersion,
    RunPersistenceSnapshot& destination) {
    destination.activeRunId.clear();
    destination.activeRunSensorMode.reset();
    destination.sensorSelection.reset();
    destination.program.reset();
    destination.revisionCount = 0U;
    destination.manual.reset();
    destination.processRunSnapshot.reset();
    destination.pendingRecoveryAnchor.reset();
    destination.recoveryBootAnchorMonotonicMillis.reset();
    destination.recoveryTemperatureEvidence.lastKnown = CrossRoleEvidence{};
    destination.lastRecoveryEpisodeEvidence.reset();
    destination.priorBootPhaseElapsed.reset();
    destination.nominalRecoveryAdjustment.reset();
    destination.recoveryEpisodeRevision = 0U;
    destination.runProgress = RunProgressState{};
    auto& s = destination;
    if (payload.size() > kMaximumCheckpointPayloadBytes)
        return RunPersistenceCodecStatus::CapacityExceeded;
    ByteReader reader(payload);
    if (!readVariant(reader, s.variant) || !readTrigger(reader, s.trigger)) {
        return RunPersistenceCodecStatus::InvalidWireValue;
    }
    if (!be::readUint64(reader, s.checkpointMonotonicMillis) ||
        !be::readUint16(reader, s.intervalMinutes) ||
        !be::readUint32(reader, s.runRevision) ||
        !readString(reader, 48U, s.activeRunId))
        return RunPersistenceCodecStatus::Truncated;
    if (s.variant != RunCheckpointVariant::NoActiveRun) {
        RunSensorMode mode;
        if (!readSensor(reader, mode))
            return RunPersistenceCodecStatus::InvalidWireValue;
        s.activeRunSensorMode = mode;
        // A schema-1 payload predates this field entirely (6.12) and is
        // mapped onto the explicit LegacyUnknown/None/0 sentinel (Korrektur-
        // auftrag Befund 4) rather than left absent: this is what lets
        // validateRunPersistenceSnapshot require sensorSelection presence
        // unconditionally for every active-run variant, schema-1 included,
        // instead of carrying a schema-dependent exception. A schema-2
        // payload always carries an explicit presence tag.
        if (schemaVersion >= kSensorSelectionFieldIntroducedInSchema) {
            bool present = false;
            if (!be::readOptionalTag(reader, present))
                return RunPersistenceCodecStatus::InvalidWireValue;
            if (present) {
                PersistedSensorSelectionState selection;
                if (!readPersistedSensorSelectionState(reader, selection))
                    return RunPersistenceCodecStatus::InvalidWireValue;
                s.sensorSelection = selection;
            }
        } else {
            s.sensorSelection = PersistedSensorSelectionState{
                SensorSelectionProvenance::LegacyUnknown,
                SensorSelectionDecisionCause::None, 0U};
        }
    }
    if (s.variant == RunCheckpointVariant::ProgramRun) {
        RunProgramSnapshot p;
        std::string program;
        std::uint8_t count = 0U;
        if (schemaVersion >= 5U) {
            bool revisionPresent = false;
            if (!readProgramSource(reader, p.sourceKind) ||
                !be::readOptionalTag(reader, revisionPresent)) {
                return RunPersistenceCodecStatus::InvalidWireValue;
            }
            if (revisionPresent) {
                std::uint64_t revision = 0U;
                if (!be::readUint64(reader, revision) || revision == 0U) {
                    return RunPersistenceCodecStatus::InvalidWireValue;
                }
                p.sourceProgramRevision = RunProgramSourceRevision{revision};
            }
            if (p.sourceKind == ProgramSourceKind::ManualTimed) {
                ManualTimedRunSource manual;
                if (revisionPresent || !readManualTimedSource(reader, manual)) {
                    return RunPersistenceCodecStatus::InvalidWireValue;
                }
                p.source = std::move(manual);
            } else {
                if (!revisionPresent ||
                    !readString(reader, kMaximumCheckpointPayloadBytes,
                                program)) {
                    return RunPersistenceCodecStatus::InvalidWireValue;
                }
                const auto decoded = decodeProgramDocumentPayload(program);
                if (!decoded.document.has_value())
                    return RunPersistenceCodecStatus::InvalidWireValue;
                p.source = *decoded.document;
            }
        } else {
            std::uint64_t sourceProgramRevision = 0U;
            if (schemaVersion >= 4U) {
                if (!be::readUint64(reader, sourceProgramRevision))
                    return RunPersistenceCodecStatus::Truncated;
            } else {
                std::uint32_t legacySourceProgramRevision = 0U;
                if (!be::readUint32(reader, legacySourceProgramRevision))
                    return RunPersistenceCodecStatus::Truncated;
                sourceProgramRevision = legacySourceProgramRevision;
            }
            if (sourceProgramRevision == 0U ||
                !readProgramSource(reader, p.sourceKind) ||
                p.sourceKind == ProgramSourceKind::ManualTimed ||
                !readString(reader, kMaximumCheckpointPayloadBytes, program)) {
                return RunPersistenceCodecStatus::InvalidWireValue;
            }
            p.sourceProgramRevision =
                RunProgramSourceRevision{sourceProgramRevision};
            const auto decoded = decodeProgramDocumentPayload(program);
            if (!decoded.document.has_value())
                return RunPersistenceCodecStatus::InvalidWireValue;
            p.source = *decoded.document;
        }
        if (!be::readUint8(reader, count) || count > kMaximumRunRevisions)
            return RunPersistenceCodecStatus::InvalidWireValue;
        s.program = std::move(p);
        s.revisionCount = count;
        for (std::size_t i = 0U; i < s.revisionCount; ++i)
            if (!readRevision(reader, s.revisions[i]))
                return RunPersistenceCodecStatus::InvalidWireValue;
    } else if (s.variant == RunCheckpointVariant::ManualRun) {
        ManualRunPlan p;
        if (!readManual(reader, s.activeRunId, p))
            return RunPersistenceCodecStatus::InvalidWireValue;
        s.manual = std::move(p);
    }
    if (s.variant != RunCheckpointVariant::NoActiveRun) {
        ProcessRunSnapshot p;
        if (!readProcessSnapshot(reader, p))
            return RunPersistenceCodecStatus::InvalidWireValue;
        s.processRunSnapshot = std::move(p);
    }
    std::uint8_t count = 0U;
    if (!readRuntime(reader, s.processState) || !be::readUint8(reader, count) ||
        count > kMaximumPersistedRunCommandIds)
        return RunPersistenceCodecStatus::InvalidWireValue;
    s.persistedRunCommandCount = count;
    for (std::size_t i = 0U; i < count; ++i)
        if (!be::readUint64(reader, s.persistedRunCommandIds[i]))
            return RunPersistenceCodecStatus::Truncated;
    if (schemaVersion >= kRecoveryFieldsIntroducedInSchema) {
        if (!readOptionalPendingRecoveryAnchor(reader,
                                               s.pendingRecoveryAnchor) ||
            !readOptionalUint64(reader, s.recoveryBootAnchorMonotonicMillis) ||
            !readCrossRoleEvidence(reader,
                                   s.recoveryTemperatureEvidence.lastKnown) ||
            !readOptionalRecoveryEpisodeEvidence(
                reader, s.lastRecoveryEpisodeEvidence) ||
            !readOptionalTaggedPriorBootPhaseElapsed(reader,
                                                     s.priorBootPhaseElapsed) ||
            !readOptionalNominalRecoveryAdjustmentState(
                reader, s.nominalRecoveryAdjustment) ||
            !be::readUint32(reader, s.recoveryEpisodeRevision) ||
            !readRunProgressState(reader, s.runProgress)) {
            return RunPersistenceCodecStatus::InvalidWireValue;
        }
    } else if (s.variant != RunCheckpointVariant::NoActiveRun) {
        // 5.28: Schema-1/2-Migration eines aktiven Runs startet ehrlich mit
        // PartialUnknownHistory statt eines erfundenen KnownTotal-Altbestands
        // und einem gesetzten, aber unbelegten Weighting-Zustand (5.21/5.25) -
        // kein gewichteter Altbeitrag wird erfunden. NoActiveRun bleibt beim
        // Default (5.14 Punkt 6 verlangt dort ohnehin weightedProgress ==
        // nullopt).
        s.runProgress.basis = RunProgressBasis::PartialUnknownHistory;
        s.runProgress.weightedProgress = WeightedProgressState{
            WeightedProgressBounds{0U, std::nullopt},
            WeightedProgressCoverage::PartialUnknown, std::nullopt};
    }
    if (reader.remaining() != 0U)
        return RunPersistenceCodecStatus::TrailingBytes;
    if (!validateRunPersistenceSnapshotForSchema(s, schemaVersion))
        return RunPersistenceCodecStatus::InvalidWireValue;
    return RunPersistenceCodecStatus::Success;
}

RunPersistenceDecodeResult decodeRunPersistenceSnapshot(
    const std::string& payload, std::uint32_t schemaVersion) {
    RunPersistenceSnapshot snapshot;
    const auto status =
        decodeRunPersistenceSnapshotInto(payload, schemaVersion, snapshot);
    if (status != RunPersistenceCodecStatus::Success) {
        return {status, std::nullopt};
    }
    return {status, std::move(snapshot)};
}

namespace {

bool writeReference(ByteWriter& writer, const RunCheckpointReference& ref) {
    return be::writeUint8(writer, ref.slot) &&
           be::writeUint32(writer, ref.schemaVersion) &&
           be::writeUint64(writer, ref.storageEpoch) &&
           be::writeUint64(writer, ref.checkpointRevision) &&
           be::writeUint32(writer, ref.payloadLength) &&
           be::writeUint32(writer, ref.payloadCrc) &&
           writeEnum(writer, ref.variant);
}

bool readReference(ByteReader& reader, RunCheckpointReference& ref) {
    if (!be::readUint8(reader, ref.slot) || ref.slot > 1U ||
        !be::readUint32(reader, ref.schemaVersion) ||
        !be::readUint64(reader, ref.storageEpoch) ||
        !be::readUint64(reader, ref.checkpointRevision) ||
        !be::readUint32(reader, ref.payloadLength) ||
        !be::readUint32(reader, ref.payloadCrc) ||
        !readVariant(reader, ref.variant)) {
        return false;
    }
    return knownRunPersistenceSchema(ref.schemaVersion) &&
           ref.storageEpoch != 0U && ref.checkpointRevision != 0U;
}

bool validReference(const RunCheckpointReference& reference,
                    device_platform::StorageEpoch epoch) {
    if (reference.slot > 1U ||
        !knownRunPersistenceSchema(reference.schemaVersion) ||
        reference.storageEpoch != epoch.value() ||
        reference.checkpointRevision == 0U ||
        reference.payloadLength > kMaximumRunPersistencePayloadBytes) {
        return false;
    }
    switch (reference.variant) {
        case RunCheckpointVariant::ProgramRun:
        case RunCheckpointVariant::ManualRun:
        case RunCheckpointVariant::NoActiveRun:
            return true;
    }
    return false;
}

bool writeHeadState(ByteWriter& writer, RunPersistenceHeadState state) {
    switch (state) {
        case RunPersistenceHeadState::Prepared:
            return be::writeUint8(writer, 1U);
        case RunPersistenceHeadState::Committed:
            return be::writeUint8(writer, 2U);
    }
    return false;
}

bool readHeadState(ByteReader& reader, RunPersistenceHeadState& state) {
    std::uint8_t value = 0U;
    if (!be::readUint8(reader, value)) return false;
    switch (value) {
        case 1U:
            state = RunPersistenceHeadState::Prepared;
            return true;
        case 2U:
            state = RunPersistenceHeadState::Committed;
            return true;
        default:
            return false;
    }
}

bool writeMutationKind(ByteWriter& writer, RunPersistenceMutationKind kind) {
    switch (kind) {
        case RunPersistenceMutationKind::Command:
            return be::writeUint8(writer, 1U);
        case RunPersistenceMutationKind::Transition:
            return be::writeUint8(writer, 2U);
        case RunPersistenceMutationKind::SensorSelection:
            return be::writeUint8(writer, 3U);
        case RunPersistenceMutationKind::Recovery:
            return be::writeUint8(writer, 4U);
    }
    return false;
}

bool writeOptionalCommandHighWater(ByteWriter& writer,
                                   const std::optional<CommandId>& value) {
    return be::writeOptionalTag(writer, value.has_value()) &&
           (!value.has_value() || be::writeUint64(writer, *value));
}

bool readOptionalCommandHighWater(ByteReader& reader,
                                  std::optional<CommandId>& value) {
    bool present = false;
    if (!be::readOptionalTag(reader, present)) return false;
    if (!present) {
        value.reset();
        return true;
    }
    CommandId id = 0U;
    if (!be::readUint64(reader, id)) return false;
    value = id;
    return true;
}

bool readMutationKind(ByteReader& reader, std::uint32_t schemaVersion,
                      RunPersistenceMutationKind& kind) {
    std::uint8_t value = 0U;
    if (!be::readUint8(reader, value)) return false;
    switch (value) {
        case 1U:
            kind = RunPersistenceMutationKind::Command;
            return true;
        case 2U:
            kind = RunPersistenceMutationKind::Transition;
            return true;
        case 3U:
            kind = RunPersistenceMutationKind::SensorSelection;
            return true;
        case 4U:
            if (schemaVersion < 3U) return false;
            kind = RunPersistenceMutationKind::Recovery;
            return true;
        default:
            return false;
    }
}

bool validPreparedHead(const RunPersistenceHead& head,
                       device_platform::StorageEpoch epoch) {
    if (!validReference(head.target, epoch) || head.fallback.has_value() ||
        head.current.schemaVersion != 0U ||
        head.newRunRevision < head.oldRunRevision ||
        head.newTransitionSequence < head.oldTransitionSequence ||
        ((head.mutationKind == RunPersistenceMutationKind::Command) !=
         head.commandId.has_value())) {
        return false;
    }
    if (head.commandId.has_value() && *head.commandId == 0U) return false;
    const bool validSameSlotRecovery =
        head.mutationKind == RunPersistenceMutationKind::Recovery &&
        head.preparedFallback.has_value() &&
        validReference(*head.preparedFallback, epoch) &&
        head.preparedFallback->slot != head.target.slot;
    if (head.preparedCurrent.has_value() &&
        (!validReference(*head.preparedCurrent, epoch) ||
         (head.preparedCurrent->slot == head.target.slot &&
          !validSameSlotRecovery) ||
         (head.preparedCurrent->variant == RunCheckpointVariant::NoActiveRun &&
          head.preparedFallback.has_value()))) {
        return false;
    }
    if (!head.preparedCurrent.has_value() && head.target.slot != 0U) {
        return false;
    }
    return !head.preparedFallback.has_value() ||
           (head.preparedCurrent.has_value() &&
            validReference(*head.preparedFallback, epoch) &&
            head.preparedFallback->slot != head.preparedCurrent->slot);
}

bool validCommittedHead(const RunPersistenceHead& head,
                        device_platform::StorageEpoch epoch) {
    if (!validReference(head.current, epoch) ||
        head.preparedCurrent.has_value() || head.preparedFallback.has_value() ||
        head.target.schemaVersion != 0U || head.commandId.has_value() ||
        head.oldRunRevision != 0U || head.newRunRevision != 0U ||
        head.oldTransitionSequence != 0U || head.newTransitionSequence != 0U) {
        return false;
    }
    if (head.current.variant == RunCheckpointVariant::NoActiveRun) {
        return !head.fallback.has_value();
    }
    return !head.fallback.has_value() ||
           (validReference(*head.fallback, epoch) &&
            head.fallback->slot != head.current.slot);
}

}  // namespace

std::optional<std::string> encodeRunPersistenceHead(
    const RunPersistenceHead& head, device_platform::StorageEpoch epoch) {
    if (head.revision == 0U || epoch.value() == 0U) return std::nullopt;
    ByteWriter payload(200U);
    bool ok = writeHeadState(payload, head.state);
    if (head.state == RunPersistenceHeadState::Prepared) {
        ok = ok && validPreparedHead(head, epoch) &&
             be::writeOptionalTag(payload, head.preparedCurrent.has_value()) &&
             (!head.preparedCurrent.has_value() ||
              writeReference(payload, *head.preparedCurrent)) &&
             be::writeOptionalTag(payload, head.preparedFallback.has_value()) &&
             (!head.preparedFallback.has_value() ||
              writeReference(payload, *head.preparedFallback)) &&
             writeReference(payload, head.target) &&
             writeMutationKind(payload, head.mutationKind) &&
             be::writeOptionalTag(payload, head.commandId.has_value()) &&
             (!head.commandId.has_value() ||
              (head.commandId.value() != 0U &&
               be::writeUint64(payload, *head.commandId))) &&
             be::writeUint32(payload, head.oldRunRevision) &&
             be::writeUint32(payload, head.newRunRevision) &&
             be::writeUint32(payload, head.oldTransitionSequence) &&
             be::writeUint32(payload, head.newTransitionSequence) &&
             head.newRunRevision >= head.oldRunRevision &&
             head.newTransitionSequence >= head.oldTransitionSequence &&
             writeOptionalCommandHighWater(payload, head.commandIdHighWater);
    } else if (head.state == RunPersistenceHeadState::Committed) {
        ok = ok && validCommittedHead(head, epoch) &&
             writeReference(payload, head.current) &&
             be::writeOptionalTag(payload, head.fallback.has_value()) &&
             (!head.fallback.has_value() ||
              writeReference(payload, *head.fallback)) &&
             writeOptionalCommandHighWater(payload, head.commandIdHighWater);
    } else {
        ok = false;
    }
    if (!ok) return std::nullopt;
    device_platform::StorageEnvelope envelope{
        kHeadRecordType, kCurrentRunPersistenceSchema, epoch, head.revision,
        std::nullopt,    payload.takeBytes()};
    std::string bytes;
    if (device_platform::encodeEnvelope(envelope, bytes,
                                        kMaximumHeadRecordBytes) !=
        device_platform::EnvelopeEncodeStatus::Success) {
        return std::nullopt;
    }
    return bytes;
}

std::optional<RunPersistenceHead> decodeRunPersistenceHead(
    const std::string& bytes, device_platform::StorageEpoch epoch) {
    const auto envelope = device_platform::decodeEnvelope(bytes);
    if (!envelope.envelope.has_value() ||
        envelope.envelope->recordTypeId != kHeadRecordType ||
        !knownRunPersistenceSchema(envelope.envelope->schemaVersion) ||
        envelope.envelope->storageEpoch != epoch ||
        envelope.envelope->versionValue == 0U) {
        return std::nullopt;
    }
    ByteReader reader(envelope.envelope->payload);
    bool present = false;
    RunPersistenceHead head;
    head.revision = envelope.envelope->versionValue;
    if (!readHeadState(reader, head.state)) return std::nullopt;
    if (head.state == RunPersistenceHeadState::Prepared) {
        if (!be::readOptionalTag(reader, present)) return std::nullopt;
        if (present) {
            RunCheckpointReference reference;
            if (!readReference(reader, reference)) return std::nullopt;
            head.preparedCurrent = reference;
        }
        if (!be::readOptionalTag(reader, present)) return std::nullopt;
        if (present) {
            RunCheckpointReference reference;
            if (!readReference(reader, reference)) return std::nullopt;
            head.preparedFallback = reference;
        }
        if (!readReference(reader, head.target) ||
            !readMutationKind(reader, envelope.envelope->schemaVersion,
                              head.mutationKind) ||
            !be::readOptionalTag(reader, present)) {
            return std::nullopt;
        }
        if (present) {
            CommandId id = 0U;
            if (!be::readUint64(reader, id) || id == 0U) return std::nullopt;
            head.commandId = id;
        }
        if (!be::readUint32(reader, head.oldRunRevision) ||
            !be::readUint32(reader, head.newRunRevision) ||
            !be::readUint32(reader, head.oldTransitionSequence) ||
            !be::readUint32(reader, head.newTransitionSequence) ||
            head.newRunRevision < head.oldRunRevision ||
            head.newTransitionSequence < head.oldTransitionSequence ||
            !validPreparedHead(head, epoch)) {
            return std::nullopt;
        }
        if (envelope.envelope->schemaVersion >= 4U &&
            !readOptionalCommandHighWater(reader, head.commandIdHighWater)) {
            return std::nullopt;
        }
    } else if (head.state == RunPersistenceHeadState::Committed) {
        if (!readReference(reader, head.current) ||
            !be::readOptionalTag(reader, present)) {
            return std::nullopt;
        }
        if (present) {
            RunCheckpointReference reference;
            if (!readReference(reader, reference)) return std::nullopt;
            head.fallback = reference;
        }
        if (envelope.envelope->schemaVersion >= 4U &&
            !readOptionalCommandHighWater(reader, head.commandIdHighWater)) {
            return std::nullopt;
        }
    } else {
        return std::nullopt;
    }
    if (reader.remaining() != 0U ||
        (head.state == RunPersistenceHeadState::Committed &&
         !validCommittedHead(head, epoch))) {
        return std::nullopt;
    }
    head.bytes = bytes;
    return head;
}

bool decodeRunPersistenceRecordInto(const std::string& bytes,
                                    device_platform::StorageEpoch epoch,
                                    RunPersistenceRawRecord& destination) {
    const auto envelope = device_platform::decodeEnvelope(bytes);
    if (!envelope.envelope.has_value() ||
        envelope.envelope->recordTypeId != kCheckpointRecordType ||
        !knownRunPersistenceSchema(envelope.envelope->schemaVersion) ||
        envelope.envelope->storageEpoch != epoch ||
        envelope.envelope->versionValue == 0U) {
        return false;
    }
    if (decodeRunPersistenceSnapshotInto(
            envelope.envelope->payload, envelope.envelope->schemaVersion,
            destination.snapshot) != RunPersistenceCodecStatus::Success)
        return false;
    destination.bytes = bytes;
    destination.checkpointRevision = envelope.envelope->versionValue;
    destination.utcUnixSeconds = envelope.envelope->utcUnixSeconds;
    return true;
}

std::optional<RunPersistenceRawRecord> decodeRunPersistenceRecord(
    const std::string& bytes, device_platform::StorageEpoch epoch) {
    RunPersistenceRawRecord record;
    if (!decodeRunPersistenceRecordInto(bytes, epoch, record)) {
        return std::nullopt;
    }
    return record;
}

bool runCheckpointReferenceMatches(const RunCheckpointReference& reference,
                                   const RunPersistenceRawRecord& record,
                                   std::size_t slot) {
    const auto envelope = device_platform::decodeEnvelope(record.bytes);
    return envelope.envelope.has_value() && reference.slot == slot &&
           reference.schemaVersion == envelope.envelope->schemaVersion &&
           reference.storageEpoch == envelope.envelope->storageEpoch.value() &&
           reference.checkpointRevision == record.checkpointRevision &&
           reference.payloadLength == envelope.envelope->payload.size() &&
           reference.payloadCrc == device_platform::computeCrc32IsoHdlc(
                                       envelope.envelope->payload) &&
           reference.variant == record.snapshot.variant;
}

RunCheckpointReference makeRunCheckpointReference(
    std::size_t slot, const RunPersistenceRawRecord& record,
    device_platform::StorageEpoch epoch) {
    const auto envelope = device_platform::decodeEnvelope(record.bytes);
    return {static_cast<std::uint8_t>(slot),
            envelope.envelope->schemaVersion,
            epoch.value(),
            record.checkpointRevision,
            static_cast<std::uint32_t>(envelope.envelope->payload.size()),
            device_platform::computeCrc32IsoHdlc(envelope.envelope->payload),
            record.snapshot.variant};
}

}  // namespace fermentation
