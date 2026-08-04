#include "run_persistence_codec.hpp"

#include <limits>
#include <utility>

#include "big_endian_codec.hpp"
#include "binary64_codec.hpp"
#include "byte_buffer.hpp"
#include "program_document_codec.hpp"

namespace fermentation {
namespace {

using device_platform::ByteReader;
using device_platform::ByteWriter;
namespace be = device_platform::big_endian;

constexpr std::size_t kMaximumCheckpointPayloadBytes = 8192U;

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
        default:
            return false;
    }
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
        default:
            return false;
    }
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
        ok = ok && writeEnum(writer, *snapshot.activeRunSensorMode);
    }
    if (snapshot.variant == RunCheckpointVariant::ProgramRun) {
        std::string program;
        ok = ok &&
             encodeProgramDocumentPayload(snapshot.program->sourceProgram,
                                          program) ==
                 ConfigurationCodecStatus::Success &&
             be::writeUint32(writer, snapshot.program->sourceProgramRevision) &&
             writeEnum(writer, snapshot.program->sourceKind) &&
             writeString(writer, program) &&
             be::writeUint8(writer,
                            static_cast<std::uint8_t>(snapshot.revisionCount));
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
    if (!ok) return RunPersistenceCodecStatus::CapacityExceeded;
    auto encoded = writer.takeBytes();
    out.swap(encoded);
    return RunPersistenceCodecStatus::Success;
}

RunPersistenceDecodeResult decodeRunPersistenceSnapshot(
    const std::string& payload) {
    if (payload.size() > kMaximumCheckpointPayloadBytes)
        return {RunPersistenceCodecStatus::CapacityExceeded, std::nullopt};
    ByteReader reader(payload);
    RunPersistenceSnapshot s;
    if (!readVariant(reader, s.variant) || !readTrigger(reader, s.trigger)) {
        return {RunPersistenceCodecStatus::InvalidWireValue, std::nullopt};
    }
    if (!be::readUint64(reader, s.checkpointMonotonicMillis) ||
        !be::readUint16(reader, s.intervalMinutes) ||
        !be::readUint32(reader, s.runRevision) ||
        !readString(reader, 48U, s.activeRunId))
        return {RunPersistenceCodecStatus::Truncated, std::nullopt};
    if (s.variant != RunCheckpointVariant::NoActiveRun) {
        RunSensorMode mode;
        if (!readSensor(reader, mode))
            return {RunPersistenceCodecStatus::InvalidWireValue, std::nullopt};
        s.activeRunSensorMode = mode;
    }
    if (s.variant == RunCheckpointVariant::ProgramRun) {
        RunProgramSnapshot p;
        std::string program;
        std::uint8_t count = 0U;
        if (!be::readUint32(reader, p.sourceProgramRevision) ||
            !readProgramSource(reader, p.sourceKind) ||
            !readString(reader, kMaximumCheckpointPayloadBytes, program) ||
            !be::readUint8(reader, count) || count > kMaximumRunRevisions)
            return {RunPersistenceCodecStatus::Truncated, std::nullopt};
        const auto decoded = decodeProgramDocumentPayload(program);
        if (!decoded.document.has_value())
            return {RunPersistenceCodecStatus::InvalidWireValue, std::nullopt};
        p.sourceProgram = *decoded.document;
        s.program = std::move(p);
        s.revisionCount = count;
        for (std::size_t i = 0U; i < s.revisionCount; ++i)
            if (!readRevision(reader, s.revisions[i]))
                return {RunPersistenceCodecStatus::InvalidWireValue,
                        std::nullopt};
    } else if (s.variant == RunCheckpointVariant::ManualRun) {
        ManualRunPlan p;
        if (!readManual(reader, s.activeRunId, p))
            return {RunPersistenceCodecStatus::InvalidWireValue, std::nullopt};
        s.manual = std::move(p);
    }
    if (s.variant != RunCheckpointVariant::NoActiveRun) {
        ProcessRunSnapshot p;
        if (!readProcessSnapshot(reader, p))
            return {RunPersistenceCodecStatus::InvalidWireValue, std::nullopt};
        s.processRunSnapshot = std::move(p);
    }
    std::uint8_t count = 0U;
    if (!readRuntime(reader, s.processState) || !be::readUint8(reader, count) ||
        count > kMaximumPersistedRunCommandIds)
        return {RunPersistenceCodecStatus::InvalidWireValue, std::nullopt};
    s.persistedRunCommandCount = count;
    for (std::size_t i = 0U; i < count; ++i)
        if (!be::readUint64(reader, s.persistedRunCommandIds[i]))
            return {RunPersistenceCodecStatus::Truncated, std::nullopt};
    if (reader.remaining() != 0U)
        return {RunPersistenceCodecStatus::TrailingBytes, std::nullopt};
    if (!validateRunPersistenceSnapshot(s))
        return {RunPersistenceCodecStatus::InvalidWireValue, std::nullopt};
    return {RunPersistenceCodecStatus::Success, std::move(s)};
}

}  // namespace fermentation
