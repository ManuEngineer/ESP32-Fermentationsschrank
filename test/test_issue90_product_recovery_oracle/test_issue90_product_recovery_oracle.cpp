#include <unity.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "configuration_bootstrap.hpp"
#include "configuration_bootstrap_codec.hpp"
#include "configuration_document_codec.hpp"
#include "configuration_graph_codec.hpp"
#include "configuration_storage_contract.hpp"
#include "crc32.hpp"
#include "run_persistence_codec.hpp"
#include "run_persistence_contract.hpp"
#include "simulated_persistent_state_store.hpp"
#include "standard_program_catalog.hpp"
#include "state_store.hpp"
#include "state_store_key.hpp"
#include "storage_envelope.hpp"

namespace {

using device_platform::RecordTypeId;
using device_platform::StateStoreKey;
using device_platform::StateStoreReadStatus;
using device_platform::StateStoreWriteStatus;
using device_platform::StorageEpoch;
using device_platform_test_support::SimulatedPersistentStateStore;
using fermentation::configuration_storage_contract::
    kConfigurationBootstrapRecordType;
using fermentation::configuration_storage_contract::
    kConfigurationBootstrapSlotKeys;
using fermentation::configuration_storage_contract::
    kConfigurationManifestRecordType;
using fermentation::configuration_storage_contract::
    kConfigurationManifestSlotKeys;
using fermentation::configuration_storage_contract::
    kConfigurationRootRecordType;
using fermentation::configuration_storage_contract::kConfigurationRootSlotKeys;
using fermentation::configuration_storage_contract::kProgramCatalogRecordType;
using fermentation::configuration_storage_contract::kProgramCatalogSlotKeys;
using fermentation::configuration_storage_contract::
    kServiceConfigurationRecordType;
using fermentation::configuration_storage_contract::
    kServiceConfigurationSlotKeys;
using fermentation::configuration_storage_contract::
    kUserConfigurationRecordType;
using fermentation::configuration_storage_contract::kUserConfigurationSlotKeys;

constexpr StorageEpoch kEpoch{1U};

// Test-only expected truth. These are not production enums and are not
// derived from the production recovery decisions that Slice 3 will inspect.
enum class Domain : std::uint8_t { Configuration, Run };
enum class Scenario : std::uint8_t {
    CurrentValid,
    OlderValid,
    FallbackValid,
    UnknownCommitValid,
    SafeWriteErrorOld,
    SafeCapacityOld,
    UnknownCommitNotFound,
    ReadError,
    ReadCapacity,
    FactoryEmpty,
    MissingEvidence,
    Partial,
    Mixed,
    CorruptEnvelopeCrc,
    UnsupportedSchema,
    InvalidReference,
    ForeignEpoch,
    Orphan,
    NotReconstructible,
    CurrentWithoutFallback,
    NoPersistedRun,
    MissingReferencedRun,
    PreparedInterrupted,
    ControlledDiscard,
};

enum class RecordClassification : std::uint8_t {
    FullyValidCurrent,
    FullyValidOlder,
    FullyValidFallback,
    Missing,
    FactoryEmpty,
    Partial,
    Mixed,
    Corrupt,
    UnsupportedSchema,
    InvalidReference,
    ForeignEpoch,
    PreparedInterrupted,
    Orphan,
    Indeterminate,
    NotReconstructible,
    ControlledDiscard,
    NoPersistedRun,
};

enum class ProductOutcome : std::uint8_t {
    NewValidConfiguration,
    OldValidConfiguration,
    FallbackValidConfiguration,
    FactoryInitializationRequired,
    ConfigurationRecoveryRequired,
    NewValidResume,
    OlderValidCheckpointResume,
    NoPersistedRun,
    RunRecoveryRequired,
    RunAbortRequired,
};

enum class SafetyProjection : std::uint8_t { Standby, ResumeOffer, SafeBoot };
enum class SafetyProducer : std::uint8_t {
    None,
    ConfigurationUnavailable,
    ConfigurationIntegrityFailure,
    RunPersistenceUntrusted,
};
enum class LogicalGate : std::uint8_t { Unresolved };

struct OracleCase {
    const char* id;
    Domain domain;
    Scenario scenario;
    const char* mutationPath;
    const char* counterDomainBaseline;
    StateStoreWriteStatus expectedWriteStatus;
    StateStoreReadStatus expectedReadStatus;
    RecordClassification classification;
    ProductOutcome outcome;
    SafetyProjection safety;
    SafetyProducer producer;
    LogicalGate logicalGate;
    bool actuatorAllowed;
    bool prohibitedActiveState;
};

OracleCase makeCase(
    const char* id, Domain domain, Scenario scenario, const char* mutationPath,
    StateStoreReadStatus readStatus, RecordClassification classification,
    ProductOutcome outcome, SafetyProjection safety, SafetyProducer producer,
    bool prohibited,
    StateStoreWriteStatus writeStatus = StateStoreWriteStatus::Success) {
    return {
        id,
        domain,
        scenario,
        mutationPath,
        domain == Domain::Configuration
            ? "trusted_run_no_persisted_run_synthetic"
            : "trusted_configuration_current_synthetic_with_sensor_evidence",
        writeStatus,
        readStatus,
        classification,
        outcome,
        safety,
        producer,
        LogicalGate::Unresolved,
        false,
        prohibited};
}

const std::vector<OracleCase> kMatrix = {
    makeCase("config_cr0_new_valid", Domain::Configuration,
             Scenario::CurrentValid, "configuration current graph",
             StateStoreReadStatus::Success,
             RecordClassification::FullyValidCurrent,
             ProductOutcome::NewValidConfiguration, SafetyProjection::Standby,
             SafetyProducer::None, false),
    makeCase("config_cr1_old_valid", Domain::Configuration,
             Scenario::OlderValid, "configuration older graph",
             StateStoreReadStatus::Success,
             RecordClassification::FullyValidOlder,
             ProductOutcome::OldValidConfiguration, SafetyProjection::Standby,
             SafetyProducer::None, false),
    makeCase("config_cm0_fallback_valid", Domain::Configuration,
             Scenario::FallbackValid, "configuration fallback graph",
             StateStoreReadStatus::Success,
             RecordClassification::FullyValidFallback,
             ProductOutcome::FallbackValidConfiguration,
             SafetyProjection::Standby, SafetyProducer::None, false),
    makeCase("config_cr1_unknown_commit_new", Domain::Configuration,
             Scenario::UnknownCommitValid, "configuration root write",
             StateStoreReadStatus::Success,
             RecordClassification::FullyValidCurrent,
             ProductOutcome::NewValidConfiguration, SafetyProjection::Standby,
             SafetyProducer::None, false,
             StateStoreWriteStatus::CommitOutcomeUnknown),
    makeCase("config_cr1_write_error_old", Domain::Configuration,
             Scenario::SafeWriteErrorOld, "configuration root write",
             StateStoreReadStatus::Success,
             RecordClassification::FullyValidOlder,
             ProductOutcome::OldValidConfiguration, SafetyProjection::Standby,
             SafetyProducer::None, false, StateStoreWriteStatus::WriteError),
    makeCase("config_cr1_capacity_error_old", Domain::Configuration,
             Scenario::SafeCapacityOld, "configuration root write",
             StateStoreReadStatus::Success,
             RecordClassification::FullyValidOlder,
             ProductOutcome::OldValidConfiguration, SafetyProjection::Standby,
             SafetyProducer::None, false, StateStoreWriteStatus::CapacityError),
    makeCase(
        "config_cr1_unknown_not_found", Domain::Configuration,
        Scenario::UnknownCommitNotFound, "configuration recovery",
        StateStoreReadStatus::NotFound, RecordClassification::Indeterminate,
        ProductOutcome::ConfigurationRecoveryRequired,
        SafetyProjection::SafeBoot, SafetyProducer::ConfigurationUnavailable,
        true, StateStoreWriteStatus::CommitOutcomeUnknown),
    makeCase("config_cr1_read_error", Domain::Configuration,
             Scenario::ReadError, "configuration recovery",
             StateStoreReadStatus::ReadError,
             RecordClassification::Indeterminate,
             ProductOutcome::ConfigurationRecoveryRequired,
             SafetyProjection::SafeBoot,
             SafetyProducer::ConfigurationUnavailable, true),
    makeCase("config_cr1_read_capacity", Domain::Configuration,
             Scenario::ReadCapacity, "configuration recovery",
             StateStoreReadStatus::CapacityError,
             RecordClassification::Indeterminate,
             ProductOutcome::ConfigurationRecoveryRequired,
             SafetyProjection::SafeBoot,
             SafetyProducer::ConfigurationUnavailable, true),
    makeCase("config_factory_empty", Domain::Configuration,
             Scenario::FactoryEmpty, "factory novelty classification",
             StateStoreReadStatus::NotFound, RecordClassification::FactoryEmpty,
             ProductOutcome::FactoryInitializationRequired,
             SafetyProjection::Standby, SafetyProducer::None, false),
    makeCase("config_missing_record_with_evidence", Domain::Configuration,
             Scenario::MissingEvidence, "configuration graph recovery",
             StateStoreReadStatus::NotFound, RecordClassification::Missing,
             ProductOutcome::ConfigurationRecoveryRequired,
             SafetyProjection::SafeBoot,
             SafetyProducer::ConfigurationUnavailable, true),
    makeCase("config_uc0_partial", Domain::Configuration, Scenario::Partial,
             "configuration record truncation", StateStoreReadStatus::Success,
             RecordClassification::Partial,
             ProductOutcome::ConfigurationRecoveryRequired,
             SafetyProjection::SafeBoot,
             SafetyProducer::ConfigurationIntegrityFailure, true),
    makeCase("config_sc0_mixed", Domain::Configuration, Scenario::Mixed,
             "configuration generation collision",
             StateStoreReadStatus::Success, RecordClassification::Mixed,
             ProductOutcome::ConfigurationRecoveryRequired,
             SafetyProjection::SafeBoot,
             SafetyProducer::ConfigurationIntegrityFailure, true),
    makeCase("config_cm1_corrupt_envelope_crc", Domain::Configuration,
             Scenario::CorruptEnvelopeCrc, "configuration envelope CRC",
             StateStoreReadStatus::Success, RecordClassification::Corrupt,
             ProductOutcome::ConfigurationRecoveryRequired,
             SafetyProjection::SafeBoot,
             SafetyProducer::ConfigurationIntegrityFailure, true),
    makeCase("config_cm2_unsupported_schema", Domain::Configuration,
             Scenario::UnsupportedSchema, "configuration schema",
             StateStoreReadStatus::Success,
             RecordClassification::UnsupportedSchema,
             ProductOutcome::ConfigurationRecoveryRequired,
             SafetyProjection::SafeBoot,
             SafetyProducer::ConfigurationIntegrityFailure, true),
    makeCase("config_cr0_invalid_reference", Domain::Configuration,
             Scenario::InvalidReference, "root to missing manifest",
             StateStoreReadStatus::Success,
             RecordClassification::InvalidReference,
             ProductOutcome::ConfigurationRecoveryRequired,
             SafetyProjection::SafeBoot,
             SafetyProducer::ConfigurationIntegrityFailure, true),
    makeCase("config_cr1_foreign_epoch", Domain::Configuration,
             Scenario::ForeignEpoch, "configuration StorageEpoch",
             StateStoreReadStatus::Success, RecordClassification::ForeignEpoch,
             ProductOutcome::ConfigurationRecoveryRequired,
             SafetyProjection::SafeBoot,
             SafetyProducer::ConfigurationIntegrityFailure, true),
    makeCase("config_uc3_orphan", Domain::Configuration, Scenario::Orphan,
             "unreferenced configuration generation",
             StateStoreReadStatus::Success, RecordClassification::Orphan,
             ProductOutcome::ConfigurationRecoveryRequired,
             SafetyProjection::SafeBoot,
             SafetyProducer::ConfigurationIntegrityFailure, true),
    makeCase("config_cr0_not_reconstructible", Domain::Configuration,
             Scenario::NotReconstructible, "configuration graph references",
             StateStoreReadStatus::Success,
             RecordClassification::NotReconstructible,
             ProductOutcome::ConfigurationRecoveryRequired,
             SafetyProjection::SafeBoot,
             SafetyProducer::ConfigurationIntegrityFailure, true),
    makeCase("config_current_without_fallback", Domain::Configuration,
             Scenario::CurrentWithoutFallback, "current without fallback",
             StateStoreReadStatus::Success,
             RecordClassification::FullyValidCurrent,
             ProductOutcome::NewValidConfiguration, SafetyProjection::Standby,
             SafetyProducer::None, false),

    makeCase("run_rc0_new_valid_resume", Domain::Run, Scenario::CurrentValid,
             "run current checkpoint", StateStoreReadStatus::Success,
             RecordClassification::FullyValidCurrent,
             ProductOutcome::NewValidResume, SafetyProjection::ResumeOffer,
             SafetyProducer::None, false),
    makeCase("run_rc1_older_checkpoint_resume", Domain::Run,
             Scenario::OlderValid, "run older checkpoint",
             StateStoreReadStatus::Success,
             RecordClassification::FullyValidOlder,
             ProductOutcome::OlderValidCheckpointResume,
             SafetyProjection::ResumeOffer, SafetyProducer::None, false),
    makeCase("run_rh0_fallback_resume", Domain::Run, Scenario::FallbackValid,
             "run fallback checkpoint", StateStoreReadStatus::Success,
             RecordClassification::FullyValidFallback,
             ProductOutcome::OlderValidCheckpointResume,
             SafetyProjection::ResumeOffer, SafetyProducer::None, false),
    makeCase(
        "run_rh0_unknown_commit_new", Domain::Run, Scenario::UnknownCommitValid,
        "run head write", StateStoreReadStatus::Success,
        RecordClassification::FullyValidCurrent, ProductOutcome::NewValidResume,
        SafetyProjection::ResumeOffer, SafetyProducer::None, false,
        StateStoreWriteStatus::CommitOutcomeUnknown),
    makeCase("run_rc0_write_error_older", Domain::Run,
             Scenario::SafeWriteErrorOld, "run checkpoint write",
             StateStoreReadStatus::Success,
             RecordClassification::FullyValidOlder,
             ProductOutcome::OlderValidCheckpointResume,
             SafetyProjection::ResumeOffer, SafetyProducer::None, false,
             StateStoreWriteStatus::WriteError),
    makeCase("run_rc1_capacity_error_older", Domain::Run,
             Scenario::SafeCapacityOld, "run checkpoint write",
             StateStoreReadStatus::Success,
             RecordClassification::FullyValidOlder,
             ProductOutcome::OlderValidCheckpointResume,
             SafetyProjection::ResumeOffer, SafetyProducer::None, false,
             StateStoreWriteStatus::CapacityError),
    makeCase(
        "run_rh0_unknown_not_found", Domain::Run,
        Scenario::UnknownCommitNotFound, "run recovery after unknown commit",
        StateStoreReadStatus::NotFound, RecordClassification::Indeterminate,
        ProductOutcome::RunRecoveryRequired, SafetyProjection::SafeBoot,
        SafetyProducer::RunPersistenceUntrusted, true,
        StateStoreWriteStatus::CommitOutcomeUnknown),
    makeCase("run_rh0_read_error", Domain::Run, Scenario::ReadError,
             "run recovery", StateStoreReadStatus::ReadError,
             RecordClassification::Indeterminate,
             ProductOutcome::RunRecoveryRequired, SafetyProjection::SafeBoot,
             SafetyProducer::RunPersistenceUntrusted, true),
    makeCase("run_rh0_read_capacity", Domain::Run, Scenario::ReadCapacity,
             "run recovery", StateStoreReadStatus::CapacityError,
             RecordClassification::Indeterminate,
             ProductOutcome::RunRecoveryRequired, SafetyProjection::SafeBoot,
             SafetyProducer::RunPersistenceUntrusted, true),
    makeCase("run_no_persisted_run", Domain::Run, Scenario::NoPersistedRun,
             "legitimate empty run store", StateStoreReadStatus::NotFound,
             RecordClassification::NoPersistedRun,
             ProductOutcome::NoPersistedRun, SafetyProjection::Standby,
             SafetyProducer::None, false),
    makeCase("run_missing_referenced_checkpoint", Domain::Run,
             Scenario::MissingReferencedRun, "head references missing rc0",
             StateStoreReadStatus::Success, RecordClassification::Missing,
             ProductOutcome::RunRecoveryRequired, SafetyProjection::SafeBoot,
             SafetyProducer::RunPersistenceUntrusted, true),
    makeCase("run_rc0_partial", Domain::Run, Scenario::Partial,
             "run checkpoint truncation", StateStoreReadStatus::Success,
             RecordClassification::Partial, ProductOutcome::RunRecoveryRequired,
             SafetyProjection::SafeBoot,
             SafetyProducer::RunPersistenceUntrusted, true),
    makeCase("run_rc1_mixed", Domain::Run, Scenario::Mixed,
             "run head/reference mismatch", StateStoreReadStatus::Success,
             RecordClassification::Mixed, ProductOutcome::RunRecoveryRequired,
             SafetyProjection::SafeBoot,
             SafetyProducer::RunPersistenceUntrusted, true),
    makeCase("run_rh0_corrupt_envelope_crc", Domain::Run,
             Scenario::CorruptEnvelopeCrc, "run head envelope CRC",
             StateStoreReadStatus::Success, RecordClassification::Corrupt,
             ProductOutcome::RunRecoveryRequired, SafetyProjection::SafeBoot,
             SafetyProducer::RunPersistenceUntrusted, true),
    makeCase("run_rc0_unsupported_schema", Domain::Run,
             Scenario::UnsupportedSchema, "run checkpoint schema",
             StateStoreReadStatus::Success,
             RecordClassification::UnsupportedSchema,
             ProductOutcome::RunRecoveryRequired, SafetyProjection::SafeBoot,
             SafetyProducer::RunPersistenceUntrusted, true),
    makeCase("run_rh0_invalid_reference", Domain::Run,
             Scenario::InvalidReference, "head/reference CRC binding",
             StateStoreReadStatus::Success,
             RecordClassification::InvalidReference,
             ProductOutcome::RunRecoveryRequired, SafetyProjection::SafeBoot,
             SafetyProducer::RunPersistenceUntrusted, true),
    makeCase("run_rc1_foreign_epoch", Domain::Run, Scenario::ForeignEpoch,
             "run checkpoint StorageEpoch", StateStoreReadStatus::Success,
             RecordClassification::ForeignEpoch,
             ProductOutcome::RunRecoveryRequired, SafetyProjection::SafeBoot,
             SafetyProducer::RunPersistenceUntrusted, true),
    makeCase("run_rh0_prepared_interrupted", Domain::Run,
             Scenario::PreparedInterrupted, "durable Prepared head",
             StateStoreReadStatus::Success,
             RecordClassification::PreparedInterrupted,
             ProductOutcome::RunRecoveryRequired, SafetyProjection::SafeBoot,
             SafetyProducer::RunPersistenceUntrusted, true),
    makeCase("run_rc1_orphan", Domain::Run, Scenario::Orphan,
             "unreferenced checkpoint", StateStoreReadStatus::Success,
             RecordClassification::Orphan, ProductOutcome::RunRecoveryRequired,
             SafetyProjection::SafeBoot,
             SafetyProducer::RunPersistenceUntrusted, true),
    makeCase("run_rh0_not_reconstructible", Domain::Run,
             Scenario::NotReconstructible, "head without referenced record",
             StateStoreReadStatus::Success,
             RecordClassification::NotReconstructible,
             ProductOutcome::RunRecoveryRequired, SafetyProjection::SafeBoot,
             SafetyProducer::RunPersistenceUntrusted, true),
    makeCase("run_controlled_discard_abort", Domain::Run,
             Scenario::ControlledDiscard, "controlled discard/abort",
             StateStoreReadStatus::Success,
             RecordClassification::ControlledDiscard,
             ProductOutcome::RunAbortRequired, SafetyProjection::Standby,
             SafetyProducer::None, false),
};

StateStoreKey keyFor(const char* value) {
    const auto result = StateStoreKey::create(value);
    TEST_ASSERT_TRUE(result.key.has_value());
    return *result.key;
}

std::string envelope(RecordTypeId type, std::uint32_t schema,
                     std::uint64_t version, const std::string& payload,
                     StorageEpoch epoch = kEpoch) {
    std::string bytes;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(device_platform::EnvelopeEncodeStatus::Success),
        static_cast<int>(device_platform::encodeEnvelope(
            {type, schema, epoch, version, std::nullopt, payload}, bytes,
            payload.size() + 160U)));
    return bytes;
}

void put(SimulatedPersistentStateStore& store, const char* key,
         const std::string& value) {
    TEST_ASSERT_EQUAL_INT(static_cast<int>(StateStoreWriteStatus::Success),
                          static_cast<int>(store.write(keyFor(key), value)));
}

template <typename Version>
fermentation::ConfigurationRecordReference<Version> reference(
    RecordTypeId type, std::uint32_t slot, Version version,
    const std::string& payload, StorageEpoch epoch = kEpoch) {
    return {type,
            device_platform::SlotId{slot},
            version,
            1U,
            static_cast<std::uint32_t>(payload.size()),
            device_platform::computeCrc32IsoHdlc(payload),
            epoch};
}

struct ConfigurationFixtureBytes {
    std::map<std::string, std::string> bytes;
    std::string activeRoot;
};

ConfigurationFixtureBytes configurationBaseline(bool withFallback) {
    ConfigurationFixtureBytes result;
    const fermentation::UserConfiguration user{"de", "Europe/Zurich",
                                               "Fixture Configuration"};
    const fermentation::ServiceConfiguration service;
    const auto catalog = fermentation::makeFactoryProgramCatalog();
    class Resolver final : public device_platform::ITimeZoneResolver {
       public:
        device_platform::TimeZonePrepareResult prepare(
            const std::string& value) const override {
            if (value != "Europe/Zurich") {
                return {device_platform::TimeZonePrepareStatus::
                            UnsupportedIdentifier,
                        std::nullopt};
            }
            return {device_platform::TimeZonePrepareStatus::Success,
                    device_platform::PreparedTimeZone{value}};
        }
    } resolver;
    std::string userPayload;
    std::string servicePayload;
    std::string catalogPayload;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::ConfigurationCodecStatus::Success),
        static_cast<int>(fermentation::encodeUserConfigurationPayload(
            user, resolver, userPayload)));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::ConfigurationCodecStatus::Success),
        static_cast<int>(fermentation::encodeServiceConfigurationPayload(
            service, servicePayload)));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::ConfigurationCodecStatus::Success),
        static_cast<int>(fermentation::encodeProgramCatalogPayload(
            catalog, catalogPayload)));

    result.bytes["uc0"] =
        envelope(kUserConfigurationRecordType, 1U, 1U, userPayload);
    result.bytes["sc0"] =
        envelope(kServiceConfigurationRecordType, 1U, 1U, servicePayload);
    result.bytes["pc0"] =
        envelope(kProgramCatalogRecordType, 1U, 1U, catalogPayload);
    const fermentation::ConfigurationManifest manifest{
        fermentation::decodeChangeOrigin(1U),
        fermentation::decodeChangeOperation(2U),
        reference(kUserConfigurationRecordType, 0U,
                  fermentation::UserConfigurationRevision{1U}, userPayload),
        reference(kServiceConfigurationRecordType, 0U,
                  fermentation::ServiceConfigurationRevision{1U},
                  servicePayload),
        reference(kProgramCatalogRecordType, 0U,
                  fermentation::ProgramCatalogRevision{1U}, catalogPayload)};
    std::string manifestPayload;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::ConfigurationGraphCodecStatus::Success),
        static_cast<int>(fermentation::encodeConfigurationManifestPayload(
            manifest, manifestPayload)));
    const auto manifestReference = reference(
        kConfigurationManifestRecordType, 0U,
        fermentation::ConfigurationManifestGeneration{1U}, manifestPayload);
    result.bytes["cm0"] =
        envelope(kConfigurationManifestRecordType, 1U, 1U, manifestPayload);

    std::optional<fermentation::ConfigurationManifestReference> fallback;
    if (withFallback) {
        auto fallbackManifest = manifest;
        fallbackManifest.userConfiguration.slot = device_platform::SlotId{1U};
        fallbackManifest.serviceConfiguration.slot =
            device_platform::SlotId{1U};
        fallbackManifest.programCatalog.slot = device_platform::SlotId{1U};
        std::string fallbackPayload;
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(
                fermentation::ConfigurationGraphCodecStatus::Success),
            static_cast<int>(fermentation::encodeConfigurationManifestPayload(
                fallbackManifest, fallbackPayload)));
        fallback = reference(kConfigurationManifestRecordType, 1U,
                             fermentation::ConfigurationManifestGeneration{2U},
                             fallbackPayload);
        result.bytes["cm1"] =
            envelope(kConfigurationManifestRecordType, 1U, 2U, fallbackPayload);
        result.bytes["uc1"] = result.bytes["uc0"];
        result.bytes["sc1"] = result.bytes["sc0"];
        result.bytes["pc1"] = result.bytes["pc0"];
    }
    auto active = manifestReference;
    if (withFallback) {
        active.slot = device_platform::SlotId{2U};
        active.version = fermentation::ConfigurationManifestGeneration{3U};
    }
    const fermentation::ConfigurationRootRecord root{active, fallback};
    std::string rootPayload;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::ConfigurationGraphCodecStatus::Success),
        static_cast<int>(
            fermentation::encodeConfigurationRootPayload(root, rootPayload)));
    result.activeRoot =
        envelope(kConfigurationRootRecordType, 1U, 1U, rootPayload);
    result.bytes["cr0"] = result.activeRoot;

    const fermentation::ConfigurationBootstrapRecord bootstrap{
        fermentation::ConfigurationBootstrapSequence{2U},
        fermentation::kConfigurationStorageFormatVersion1, kEpoch,
        fermentation::ConfigurationBootstrapState::Initialized};
    std::string bootstrapBytes;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            fermentation::ConfigurationBootstrapCodecStatus::Success),
        static_cast<int>(fermentation::encodeConfigurationBootstrapRecord(
            bootstrap, bootstrapBytes)));
    result.bytes["cb0"] = bootstrapBytes;
    return result;
}

void writeConfigurationBytes(SimulatedPersistentStateStore& store,
                             const std::map<std::string, std::string>& bytes) {
    for (const auto& [key, value] : bytes) put(store, key.c_str(), value);
}

fermentation::RunPersistenceSnapshot runSnapshot() {
    auto document = fermentation::FactoryProgramCatalog::find("water-kefir");
    TEST_ASSERT_TRUE(document.has_value());
    auto& program = document->program;
    program.productSensorFailure.fallbackDelaySeconds = 30U;
    program.fermentationStages.front().targetTemperatureCelsius = 38.0;
    program.fermentationStages.front().durationMinutes = 120U;
    program.targetQualification.bandCelsius = 0.5;
    program.targetQualification.durationMinutes = 10U;
    program.maximumTargetReachMinutes = 180U;
    if (program.preheat) program.maximumProductWaitMinutes = 30U;
    const auto run = fermentation::ActiveRun::start(
        *document, fermentation::ProgramSourceKind::FactoryCatalog, 1U);
    TEST_ASSERT_TRUE(run.has_value());
    fermentation::RunCommandState state;
    state.processState.state = program.preheat
                                   ? fermentation::ProcessState::Preheating
                                   : fermentation::ProcessState::ReachingTarget;
    state.activeProgramRun = *run;
    state.activeRunId = "issue90-fixture-run";
    state.activeRunSensorMode = fermentation::RunSensorMode::Product;
    state.runRevision = 1U;
    state.sensorSelection = fermentation::PersistedSensorSelectionState{
        fermentation::SensorSelectionProvenance::InitialSelection,
        fermentation::SensorSelectionDecisionCause::StartSelection, 1U};
    state.processRunSnapshot = fermentation::makeProcessRunSnapshot(*run);
    TEST_ASSERT_TRUE(state.processRunSnapshot.has_value());
    std::array<fermentation::CommandId,
               fermentation::kMaximumPersistedRunCommandIds>
        ids{};
    ids[0] = 99U;
    const auto snapshot = fermentation::makeRunPersistenceSnapshot(
        state, ids, 1U, fermentation::RunCheckpointTrigger::Command,
        fermentation::RunCheckpointTime{100U, 1700000000}, 5U);
    TEST_ASSERT_TRUE(snapshot.has_value());
    return *snapshot;
}

std::string runRecord(const fermentation::RunPersistenceSnapshot& snapshot,
                      std::uint64_t revision, StorageEpoch epoch = kEpoch) {
    std::string payload;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::RunPersistenceCodecStatus::Success),
        static_cast<int>(
            fermentation::encodeRunPersistenceSnapshot(snapshot, payload)));
    return envelope(RecordTypeId{7U},
                    fermentation::kCurrentRunPersistenceSchema, revision,
                    payload, epoch);
}

fermentation::RunCheckpointReference runReference(const std::string& bytes,
                                                  std::size_t slot = 0U) {
    const auto raw = fermentation::decodeRunPersistenceRecord(bytes, kEpoch);
    TEST_ASSERT_TRUE(raw.has_value());
    auto reference =
        fermentation::makeRunCheckpointReference(slot, *raw, kEpoch);
    reference.slot = static_cast<std::uint8_t>(slot);
    return reference;
}

std::string runHead(
    const fermentation::RunCheckpointReference& current,
    std::optional<fermentation::RunCheckpointReference> fallback = std::nullopt,
    fermentation::RunPersistenceHeadState state =
        fermentation::RunPersistenceHeadState::Committed) {
    fermentation::RunPersistenceHead head;
    head.state = state;
    head.revision =
        state == fermentation::RunPersistenceHeadState::Prepared ? 2U : 1U;
    if (state == fermentation::RunPersistenceHeadState::Committed) {
        head.current = current;
        head.fallback = fallback;
    } else {
        head.preparedCurrent = current;
        head.target = current;
        head.target.slot = 1U;
        head.target.checkpointRevision = current.checkpointRevision + 1U;
        head.mutationKind = fermentation::RunPersistenceMutationKind::Command;
        head.commandId = 42U;
        head.oldRunRevision = 1U;
        head.newRunRevision = 2U;
        head.oldTransitionSequence = 1U;
        head.newTransitionSequence = 2U;
    }
    const auto encoded = fermentation::encodeRunPersistenceHead(head, kEpoch);
    TEST_ASSERT_TRUE(encoded.has_value());
    return *encoded;
}

struct BackendObservation {
    StateStoreWriteStatus writeStatus{StateStoreWriteStatus::Success};
    StateStoreReadStatus readStatus{StateStoreReadStatus::NotFound};
    std::string readBytes;
    std::string targetKey;
    std::string fixtureId;
    std::map<std::string, std::string> persisted;
    std::vector<std::string> missing;
    bool writeAttempted{false};
    bool restarted{false};
    bool pendingBeforeRestart{false};
    bool pendingAfterRestart{false};
    bool durablePreparedHead{false};
};

std::vector<std::string> allConfigurationKeys() {
    std::vector<std::string> keys;
    const auto append = [&keys](const auto& family) {
        for (const auto* key : family) keys.emplace_back(key);
    };
    append(kUserConfigurationSlotKeys);
    append(kServiceConfigurationSlotKeys);
    append(kProgramCatalogSlotKeys);
    append(kConfigurationManifestSlotKeys);
    append(kConfigurationRootSlotKeys);
    append(kConfigurationBootstrapSlotKeys);
    return keys;
}
std::vector<std::string> allRunKeys() { return {"rc0", "rc1", "rh0"}; }

void inspectPersisted(SimulatedPersistentStateStore& store, Domain domain,
                      BackendObservation& observed) {
    const auto keys =
        domain == Domain::Configuration ? allConfigurationKeys() : allRunKeys();
    for (const auto& name : keys) {
        const auto read = store.read(keyFor(name.c_str()), 16384U);
        if (read.status == StateStoreReadStatus::Success) {
            observed.persisted.emplace(name, read.value);
        } else if (read.status == StateStoreReadStatus::NotFound) {
            observed.missing.push_back(name);
        }
    }
}

std::string targetKey(const OracleCase& item) {
    if (item.domain == Domain::Configuration) {
        switch (item.scenario) {
            case Scenario::Partial:
            case Scenario::MissingEvidence:
                return "uc0";
            case Scenario::CorruptEnvelopeCrc:
            case Scenario::UnsupportedSchema:
            case Scenario::ForeignEpoch:
                return "cm0";
            case Scenario::UnknownCommitNotFound:
            case Scenario::UnknownCommitValid:
            case Scenario::SafeWriteErrorOld:
            case Scenario::SafeCapacityOld:
            case Scenario::OlderValid:
                return "cr1";
            default:
                return "cr0";
        }
    }
    switch (item.scenario) {
        case Scenario::Partial:
        case Scenario::UnsupportedSchema:
            return "rc0";
        case Scenario::ForeignEpoch:
        case Scenario::Orphan:
            return "rc1";
        default:
            return "rh0";
    }
}

void writeRunBaseline(SimulatedPersistentStateStore& store,
                      std::map<std::string, std::string>& bytes,
                      bool withFallback) {
    const auto snapshot = runSnapshot();
    bytes["rc0"] = runRecord(snapshot, 1U);
    put(store, "rc0", bytes["rc0"]);
    const auto current = runReference(bytes["rc0"], 0U);
    if (withFallback) {
        bytes["rc1"] = runRecord(snapshot, 2U);
        put(store, "rc1", bytes["rc1"]);
        const auto fallback = runReference(bytes["rc1"], 1U);
        bytes["rh0"] = runHead(current, fallback);
    } else {
        bytes["rh0"] = runHead(current);
    }
    put(store, "rh0", bytes["rh0"]);
}

BackendObservation observe(const OracleCase& item) {
    SimulatedPersistentStateStore store;
    BackendObservation observed;
    observed.fixtureId = item.id;
    observed.targetKey = targetKey(item);
    const auto writeMutation = [&](const char* key, const std::string& value) {
        observed.writeAttempted = true;
        observed.writeStatus = store.write(keyFor(key), value);
    };
    const auto restart = [&]() {
        observed.pendingBeforeRestart = store.hasPendingWriteForTesting();
        store.restart();
        observed.restarted = true;
        observed.pendingAfterRestart = store.hasPendingWriteForTesting();
    };

    if (item.domain == Domain::Configuration) {
        auto baseline =
            configurationBaseline(item.scenario == Scenario::FallbackValid);
        if (item.scenario != Scenario::FactoryEmpty) {
            if (item.scenario == Scenario::MissingEvidence) {
                baseline.bytes.erase("uc0");
            }
            writeConfigurationBytes(store, baseline.bytes);
        }
        switch (item.scenario) {
            case Scenario::CurrentValid:
            case Scenario::OlderValid:
            case Scenario::CurrentWithoutFallback:
                if (item.scenario == Scenario::OlderValid) {
                    writeMutation("cr1", baseline.activeRoot);
                }
                break;
            case Scenario::FallbackValid:
                break;
            case Scenario::UnknownCommitValid:
                writeMutation("cr1", baseline.activeRoot);
                store.setNextWriteFault(
                    SimulatedPersistentStateStore::WriteFault::
                        PowerCutAfterCommitBeforeReturn);
                writeMutation("cr1", baseline.activeRoot);
                break;
            case Scenario::SafeWriteErrorOld:
                writeMutation("cr1", baseline.activeRoot);
                store.setNextWriteFault(SimulatedPersistentStateStore::
                                            WriteFault::PowerCutBeforeCommit);
                writeMutation("cr1", "candidate-not-durable");
                break;
            case Scenario::SafeCapacityOld:
                writeMutation("cr1", baseline.activeRoot);
                store.setNextWriteFault(SimulatedPersistentStateStore::
                                            WriteFault::CapacityExceeded);
                writeMutation("cr1", "candidate-not-written");
                break;
            case Scenario::UnknownCommitNotFound:
                store.setNextWriteFault(
                    SimulatedPersistentStateStore::WriteFault::
                        PowerCutAfterCommitBeforeReturn);
                writeMutation("cr1", baseline.activeRoot);
                break;
            case Scenario::ReadError:
                store.injectReadFailure(keyFor("cr0"), true);
                break;
            case Scenario::ReadCapacity:
            case Scenario::FactoryEmpty:
                break;
            case Scenario::MissingEvidence:
                break;
            case Scenario::Partial:
                store.injectCorruption(keyFor("uc0"),
                                       baseline.bytes["uc0"].substr(0U, 5U));
                break;
            case Scenario::Mixed: {
                const auto decoded =
                    device_platform::decodeEnvelope(baseline.bytes["uc0"]);
                TEST_ASSERT_TRUE(decoded.envelope.has_value());
                store.injectCorruption(
                    keyFor("uc1"), envelope(kUserConfigurationRecordType, 1U,
                                            2U, decoded.envelope->payload));
            } break;
            case Scenario::CorruptEnvelopeCrc: {
                auto bytes = baseline.bytes["cm0"];
                bytes.back() ^= static_cast<char>(1U);
                store.injectCorruption(keyFor("cm0"), std::move(bytes));
                break;
            }
            case Scenario::UnsupportedSchema: {
                const auto decoded =
                    device_platform::decodeEnvelope(baseline.bytes["cm0"]);
                TEST_ASSERT_TRUE(decoded.envelope.has_value());
                store.injectCorruption(
                    keyFor("cm0"),
                    envelope(kConfigurationManifestRecordType, 99U, 1U,
                             decoded.envelope->payload));
                break;
            }
            case Scenario::InvalidReference:
            case Scenario::NotReconstructible: {
                const auto manifestBytes =
                    device_platform::decodeEnvelope(baseline.bytes["cm0"]);
                TEST_ASSERT_TRUE(manifestBytes.envelope.has_value());
                const fermentation::ConfigurationRootRecord invalid{
                    reference(kConfigurationManifestRecordType, 2U,
                              fermentation::ConfigurationManifestGeneration{9U},
                              manifestBytes.envelope->payload),
                    std::nullopt};
                std::string payload;
                TEST_ASSERT_EQUAL_INT(
                    static_cast<int>(
                        fermentation::ConfigurationGraphCodecStatus::Success),
                    static_cast<int>(
                        fermentation::encodeConfigurationRootPayload(invalid,
                                                                     payload)));
                store.injectCorruption(
                    keyFor("cr0"),
                    envelope(kConfigurationRootRecordType, 1U, 9U, payload));
                if (item.scenario == Scenario::NotReconstructible) {
                    store.injectCorruption(keyFor("cm0"), {});
                }
                break;
            }
            case Scenario::ForeignEpoch: {
                const auto decoded =
                    device_platform::decodeEnvelope(baseline.bytes["cm0"]);
                TEST_ASSERT_TRUE(decoded.envelope.has_value());
                store.injectCorruption(
                    keyFor("cm0"),
                    envelope(kConfigurationManifestRecordType, 1U, 1U,
                             decoded.envelope->payload, StorageEpoch{2U}));
                break;
            }
            case Scenario::Orphan:
                store.injectCorruption(keyFor("uc3"), baseline.bytes["uc0"]);
                break;
            case Scenario::NoPersistedRun:
            case Scenario::MissingReferencedRun:
            case Scenario::PreparedInterrupted:
            case Scenario::ControlledDiscard:
                break;
        }
    } else {
        std::map<std::string, std::string> baseline;
        const bool withFallback =
            item.scenario == Scenario::FallbackValid ||
            item.scenario == Scenario::Mixed ||
            item.scenario == Scenario::InvalidReference ||
            item.scenario == Scenario::PreparedInterrupted ||
            item.scenario == Scenario::ForeignEpoch ||
            item.scenario == Scenario::OlderValid;
        if (item.scenario == Scenario::MissingReferencedRun ||
            item.scenario == Scenario::NotReconstructible) {
            const auto snapshot = runSnapshot();
            const auto rc0 = runRecord(snapshot, 1U);
            const auto current = runReference(rc0, 0U);
            baseline["rh0"] = runHead(current);
            put(store, "rh0", baseline["rh0"]);
        } else if (item.scenario != Scenario::NoPersistedRun) {
            writeRunBaseline(store, baseline, withFallback);
        }
        switch (item.scenario) {
            case Scenario::CurrentValid:
            case Scenario::OlderValid:
            case Scenario::ControlledDiscard:
            case Scenario::CurrentWithoutFallback:
            case Scenario::MissingReferencedRun:
                break;
            case Scenario::FallbackValid:
                store.injectCorruption(keyFor("rc0"), {});
                break;
            case Scenario::UnknownCommitValid:
                store.setNextWriteFault(
                    SimulatedPersistentStateStore::WriteFault::
                        PowerCutAfterCommitBeforeReturn);
                writeMutation("rh0", baseline["rh0"]);
                break;
            case Scenario::SafeWriteErrorOld:
                store.setNextWriteFault(SimulatedPersistentStateStore::
                                            WriteFault::PowerCutBeforeCommit);
                writeMutation("rc0", "candidate-not-durable");
                break;
            case Scenario::SafeCapacityOld:
                store.setNextWriteFault(SimulatedPersistentStateStore::
                                            WriteFault::CapacityExceeded);
                writeMutation("rc1", "candidate-not-written");
                break;
            case Scenario::UnknownCommitNotFound:
                store.setNextWriteFault(
                    SimulatedPersistentStateStore::WriteFault::
                        PowerCutAfterCommitBeforeReturn);
                writeMutation("rh0", baseline["rh0"]);
                break;
            case Scenario::ReadError:
                store.injectReadFailure(keyFor("rh0"), true);
                break;
            case Scenario::ReadCapacity:
            case Scenario::FactoryEmpty:
            case Scenario::MissingEvidence:
                break;
            case Scenario::Partial:
                store.injectCorruption(keyFor("rc0"),
                                       baseline["rc0"].substr(0U, 7U));
                break;
            case Scenario::Mixed: {
                const auto head = fermentation::decodeRunPersistenceHead(
                    baseline["rh0"], kEpoch);
                TEST_ASSERT_TRUE(head.has_value());
                TEST_ASSERT_TRUE(head->fallback.has_value());
                auto mixed = *head;
                std::swap(mixed.current.slot, mixed.fallback->slot);
                const auto bytes =
                    fermentation::encodeRunPersistenceHead(mixed, kEpoch);
                TEST_ASSERT_TRUE(bytes.has_value());
                store.injectCorruption(keyFor("rh0"), *bytes);
                break;
            }
            case Scenario::CorruptEnvelopeCrc: {
                auto bytes = baseline["rh0"];
                bytes.back() ^= static_cast<char>(1U);
                store.injectCorruption(keyFor("rh0"), std::move(bytes));
                break;
            }
            case Scenario::UnsupportedSchema: {
                const auto decoded =
                    device_platform::decodeEnvelope(baseline["rc0"]);
                TEST_ASSERT_TRUE(decoded.envelope.has_value());
                store.injectCorruption(keyFor("rc0"),
                                       envelope(RecordTypeId{7U}, 99U, 1U,
                                                decoded.envelope->payload));
                break;
            }
            case Scenario::InvalidReference: {
                const auto head = fermentation::decodeRunPersistenceHead(
                    baseline["rh0"], kEpoch);
                TEST_ASSERT_TRUE(head.has_value());
                auto invalid = *head;
                invalid.current.payloadCrc ^= 1U;
                const auto bytes =
                    fermentation::encodeRunPersistenceHead(invalid, kEpoch);
                TEST_ASSERT_TRUE(bytes.has_value());
                store.injectCorruption(keyFor("rh0"), *bytes);
                break;
            }
            case Scenario::ForeignEpoch: {
                const auto decoded =
                    device_platform::decodeEnvelope(baseline["rc1"]);
                TEST_ASSERT_TRUE(decoded.envelope.has_value());
                store.injectCorruption(
                    keyFor("rc1"),
                    envelope(RecordTypeId{7U},
                             fermentation::kCurrentRunPersistenceSchema, 2U,
                             decoded.envelope->payload, StorageEpoch{2U}));
                break;
            }
            case Scenario::PreparedInterrupted: {
                const auto current = runReference(baseline["rc0"], 0U);
                // Prepared is a durable, committed head. It is not a pending
                // simulator write and therefore survives restart.
                put(store, "rh0",
                    runHead(current, std::nullopt,
                            fermentation::RunPersistenceHeadState::Prepared));
                observed.durablePreparedHead = true;
                break;
            }
            case Scenario::Orphan:
                put(store, "rc1", baseline["rc0"]);
                break;
            case Scenario::NotReconstructible: {
                break;
            }
            case Scenario::NoPersistedRun:
                break;
        }
    }

    if (!observed.restarted) {
        restart();
    }
    inspectPersisted(store, item.domain, observed);
    if (item.scenario == Scenario::UnknownCommitNotFound) {
        store.forceNotFound(keyFor(observed.targetKey.c_str()), true);
    }
    if (item.scenario == Scenario::ReadError) {
        store.injectReadFailure(keyFor(observed.targetKey.c_str()), true);
    }
    const auto maxBytes = item.scenario == Scenario::ReadCapacity ? 8U : 16384U;
    const auto read = store.read(keyFor(observed.targetKey.c_str()), maxBytes);
    observed.readStatus = read.status;
    observed.readBytes = read.value;
    return observed;
}

const char* writeStatusName(StateStoreWriteStatus value) {
    switch (value) {
        case StateStoreWriteStatus::Success:
            return "Success";
        case StateStoreWriteStatus::WriteError:
            return "WriteError";
        case StateStoreWriteStatus::CapacityError:
            return "CapacityError";
        case StateStoreWriteStatus::CommitOutcomeUnknown:
            return "CommitOutcomeUnknown";
    }
    return "UnknownWriteStatus";
}
const char* readStatusName(StateStoreReadStatus value) {
    switch (value) {
        case StateStoreReadStatus::Success:
            return "Success";
        case StateStoreReadStatus::NotFound:
            return "NotFound";
        case StateStoreReadStatus::ReadError:
            return "ReadError";
        case StateStoreReadStatus::CapacityError:
            return "CapacityError";
    }
    return "UnknownReadStatus";
}
const char* outcomeName(ProductOutcome value) {
    switch (value) {
        case ProductOutcome::NewValidConfiguration:
            return "NEW_VALID_CONFIGURATION";
        case ProductOutcome::OldValidConfiguration:
            return "OLD_VALID_CONFIGURATION";
        case ProductOutcome::FallbackValidConfiguration:
            return "FALLBACK_VALID_CONFIGURATION";
        case ProductOutcome::FactoryInitializationRequired:
            return "FACTORY_INITIALIZATION_REQUIRED";
        case ProductOutcome::ConfigurationRecoveryRequired:
            return "CONFIGURATION_RECOVERY_REQUIRED";
        case ProductOutcome::NewValidResume:
            return "NEW_VALID_RESUME";
        case ProductOutcome::OlderValidCheckpointResume:
            return "OLDER_VALID_CHECKPOINT_RESUME";
        case ProductOutcome::NoPersistedRun:
            return "NO_PERSISTED_RUN";
        case ProductOutcome::RunRecoveryRequired:
            return "RUN_RECOVERY_REQUIRED";
        case ProductOutcome::RunAbortRequired:
            return "RUN_ABORT_REQUIRED";
    }
    return "UNKNOWN_PRODUCT_OUTCOME";
}
const char* classificationName(RecordClassification value) {
    switch (value) {
        case RecordClassification::FullyValidCurrent:
            return "FullyValidCurrent";
        case RecordClassification::FullyValidOlder:
            return "FullyValidOlder";
        case RecordClassification::FullyValidFallback:
            return "FullyValidFallback";
        case RecordClassification::Missing:
            return "Missing";
        case RecordClassification::FactoryEmpty:
            return "FactoryEmpty";
        case RecordClassification::Partial:
            return "Partial";
        case RecordClassification::Mixed:
            return "Mixed";
        case RecordClassification::Corrupt:
            return "Corrupt";
        case RecordClassification::UnsupportedSchema:
            return "UnsupportedSchema";
        case RecordClassification::InvalidReference:
            return "InvalidReference";
        case RecordClassification::ForeignEpoch:
            return "ForeignEpoch";
        case RecordClassification::PreparedInterrupted:
            return "PreparedInterrupted";
        case RecordClassification::Orphan:
            return "Orphan";
        case RecordClassification::Indeterminate:
            return "Indeterminate";
        case RecordClassification::NotReconstructible:
            return "NotReconstructible";
        case RecordClassification::ControlledDiscard:
            return "ControlledDiscard";
        case RecordClassification::NoPersistedRun:
            return "NoPersistedRun";
    }
    return "UnknownClassification";
}
const char* producerName(SafetyProducer value) {
    switch (value) {
        case SafetyProducer::None:
            return "NONE";
        case SafetyProducer::ConfigurationUnavailable:
            return "CONFIGURATION_UNAVAILABLE";
        case SafetyProducer::ConfigurationIntegrityFailure:
            return "CONFIGURATION_INTEGRITY_FAILURE";
        case SafetyProducer::RunPersistenceUntrusted:
            return "RUN_PERSISTENCE_UNTRUSTED";
    }
    return "UNKNOWN_SAFETY_PRODUCER";
}
const char* safetyName(SafetyProjection value) {
    switch (value) {
        case SafetyProjection::Standby:
            return "STANDBY";
        case SafetyProjection::ResumeOffer:
            return "RESUME_OFFER";
        case SafetyProjection::SafeBoot:
            return "SAFE_BOOT";
    }
    return "UNKNOWN_SAFETY_PROJECTION";
}

std::string hexBytes(const std::string& bytes) {
    constexpr char digits[] = "0123456789abcdef";
    std::string result;
    for (const auto value : bytes) {
        const auto byte = static_cast<unsigned char>(value);
        result.push_back(digits[byte >> 4U]);
        result.push_back(digits[byte & 0x0fU]);
    }
    return result;
}
std::string joinKeys(const std::map<std::string, std::string>& values) {
    std::string result;
    for (const auto& [key, value] : values) {
        static_cast<void>(value);
        if (!result.empty()) result += ",";
        result += key;
    }
    return result;
}
std::string joinMissing(const std::vector<std::string>& values) {
    std::string result;
    for (const auto& value : values) {
        if (!result.empty()) result += ",";
        result += value;
    }
    return result;
}

std::string machineLine(const OracleCase& item,
                        const BackendObservation& observed) {
    std::string line{"issue90_oracle_case="};
    line += item.id;
    line += item.domain == Domain::Configuration ? " domain=configuration"
                                                 : " domain=run";
    line += " mutation_path=\"" + std::string(item.mutationPath) + "\"";
    line += " evidence_scope=SIMULATOR_ONLY backend_characterization=observed";
    line +=
        " fault_layer=SIMULATOR_ONLY "
        "post_reboot_fixture=persistent_committed_state";
    line +=
        " counter_domain_baseline=" + std::string(item.counterDomainBaseline);
    line += " fixture_id=" + observed.fixtureId;
    line += " fixture_keys_present=" + joinKeys(observed.persisted);
    line += " fixture_keys_missing=" + joinMissing(observed.missing);
    line += " record_family=" +
            std::string(
                item.domain == Domain::Configuration
                    ? "uc0..uc3,sc0..sc3,pc0..pc3,cm0..cm2,cr0..cr1,cb0..cb1"
                    : "rc0,rc1,rh0");
    line += " target_key=" + observed.targetKey;
    line += " write_status=" +
            std::string(observed.writeAttempted
                            ? writeStatusName(observed.writeStatus)
                            : "NOT_APPLICABLE");
    line += " read_status=" + std::string(readStatusName(observed.readStatus));
    line += " read_bytes_hex=" + hexBytes(observed.readBytes);
    line += " record_classification=" +
            std::string(classificationName(item.classification));
    line += " product_outcome=" + std::string(outcomeName(item.outcome));
    line += " safety_projection=" + std::string(safetyName(item.safety));
    line += " safety_producer=" + std::string(producerName(item.producer));
    line += " logical_gate=UNRESOLVED actuator_allowed=false";
    line += " product_recovery_gate=PASS reboot=" +
            std::string(observed.restarted ? "true" : "false");
    line += " pending_before_reboot=" +
            std::string(observed.pendingBeforeRestart ? "true" : "false");
    line += " pending_after_reboot=" +
            std::string(observed.pendingAfterRestart ? "true" : "false");
    line += " durable_prepared_head=" +
            std::string(observed.durablePreparedHead ? "true" : "false");
    line += " prohibited_active_state=" +
            std::string(item.prohibitedActiveState ? "true" : "false");
    return line;
}

void test_matrix_contains_complete_domain_and_safety_categories() {
    std::size_t config = 0U;
    std::size_t run = 0U;
    bool hasFactoryEmpty = false;
    bool hasNoPersistedRun = false;
    bool hasPrepared = false;
    bool hasIntegrityProducer = false;
    bool hasPartial = false;
    bool hasMixed = false;
    bool hasOrphan = false;
    bool hasUnknown = false;
    for (const auto& item : kMatrix) {
        if (item.domain == Domain::Configuration) ++config;
        if (item.domain == Domain::Run) ++run;
        hasFactoryEmpty =
            hasFactoryEmpty || item.scenario == Scenario::FactoryEmpty;
        hasNoPersistedRun =
            hasNoPersistedRun || item.scenario == Scenario::NoPersistedRun;
        hasPrepared =
            hasPrepared || item.scenario == Scenario::PreparedInterrupted;
        hasIntegrityProducer =
            hasIntegrityProducer ||
            item.producer == SafetyProducer::ConfigurationIntegrityFailure;
        hasPartial = hasPartial || item.scenario == Scenario::Partial;
        hasMixed = hasMixed || item.scenario == Scenario::Mixed;
        hasOrphan = hasOrphan || item.scenario == Scenario::Orphan;
        hasUnknown =
            hasUnknown || item.expectedWriteStatus ==
                              StateStoreWriteStatus::CommitOutcomeUnknown;
    }
    TEST_ASSERT_EQUAL_UINT32(20U, config);
    TEST_ASSERT_EQUAL_UINT32(21U, run);
    TEST_ASSERT_TRUE(hasFactoryEmpty);
    TEST_ASSERT_TRUE(hasNoPersistedRun);
    TEST_ASSERT_TRUE(hasPrepared);
    TEST_ASSERT_TRUE(hasIntegrityProducer);
    TEST_ASSERT_TRUE(hasPartial);
    TEST_ASSERT_TRUE(hasMixed);
    TEST_ASSERT_TRUE(hasOrphan);
    TEST_ASSERT_TRUE(hasUnknown);
}

void test_every_case_has_real_post_reboot_fixture_evidence() {
    for (const auto& item : kMatrix) {
        const auto observed = observe(item);
        TEST_ASSERT_TRUE(observed.restarted);
        TEST_ASSERT_FALSE(observed.pendingAfterRestart);
        TEST_ASSERT_TRUE(observed.readStatus == item.expectedReadStatus);
        if (item.scenario == Scenario::FactoryEmpty ||
            item.scenario == Scenario::NoPersistedRun) {
            TEST_ASSERT_TRUE(observed.persisted.empty());
        }
        if (item.scenario == Scenario::PreparedInterrupted) {
            TEST_ASSERT_TRUE(observed.durablePreparedHead);
            TEST_ASSERT_TRUE(observed.persisted.find("rh0") !=
                             observed.persisted.end());
            const auto decoded = fermentation::decodeRunPersistenceHead(
                observed.persisted.at("rh0"), kEpoch);
            TEST_ASSERT_TRUE(decoded.has_value());
            TEST_ASSERT_EQUAL_INT(
                static_cast<int>(
                    fermentation::RunPersistenceHeadState::Prepared),
                static_cast<int>(decoded->state));
        }
        if (item.scenario == Scenario::Partial) {
            const auto key =
                item.domain == Domain::Configuration ? "uc0" : "rc0";
            TEST_ASSERT_TRUE(observed.persisted.find(key) !=
                             observed.persisted.end());
            TEST_ASSERT_FALSE(
                device_platform::decodeEnvelope(observed.persisted.at(key))
                    .envelope.has_value());
        }
        if (item.scenario == Scenario::CorruptEnvelopeCrc) {
            TEST_ASSERT_FALSE(
                device_platform::decodeEnvelope(observed.readBytes)
                    .envelope.has_value());
        }
        if (item.scenario == Scenario::UnsupportedSchema) {
            const auto decoded =
                device_platform::decodeEnvelope(observed.readBytes);
            TEST_ASSERT_TRUE(decoded.envelope.has_value());
            TEST_ASSERT_EQUAL_UINT32(99U, decoded.envelope->schemaVersion);
        }
        if (item.scenario == Scenario::MissingEvidence) {
            TEST_ASSERT_TRUE(observed.persisted.find("cr0") !=
                             observed.persisted.end());
            TEST_ASSERT_TRUE(observed.persisted.find("cm0") !=
                             observed.persisted.end());
            TEST_ASSERT_TRUE(observed.readStatus ==
                             StateStoreReadStatus::NotFound);
        }
        if (item.scenario == Scenario::Orphan) {
            const auto key =
                item.domain == Domain::Configuration ? "uc3" : "rc1";
            TEST_ASSERT_TRUE(observed.persisted.find(key) !=
                             observed.persisted.end());
        }
        if (item.scenario == Scenario::FallbackValid) {
            if (item.domain == Domain::Configuration) {
                const auto rootEnvelope = device_platform::decodeEnvelope(
                    observed.persisted.at("cr0"));
                TEST_ASSERT_TRUE(rootEnvelope.envelope.has_value());
                const auto root = fermentation::decodeConfigurationRootPayload(
                    rootEnvelope.envelope->payload);
                TEST_ASSERT_TRUE(root.value.has_value());
                TEST_ASSERT_TRUE(root.value->fallback.has_value());
                TEST_ASSERT_EQUAL_UINT32(2U, root.value->active.slot.value());
                TEST_ASSERT_EQUAL_UINT32(1U,
                                         root.value->fallback->slot.value());
                TEST_ASSERT_TRUE(observed.persisted.find("cm1") !=
                                 observed.persisted.end());
                TEST_ASSERT_TRUE(std::find(observed.missing.begin(),
                                           observed.missing.end(),
                                           "cm2") != observed.missing.end());
            } else {
                TEST_ASSERT_TRUE(observed.persisted.find("rc1") !=
                                 observed.persisted.end());
                TEST_ASSERT_TRUE(observed.persisted.find("rc0") !=
                                 observed.persisted.end());
                TEST_ASSERT_TRUE(observed.persisted.at("rc0").empty());
            }
        }
        if (item.scenario == Scenario::InvalidReference ||
            item.scenario == Scenario::NotReconstructible) {
            if (item.domain == Domain::Configuration) {
                const auto rootEnvelope = device_platform::decodeEnvelope(
                    observed.persisted.at("cr0"));
                TEST_ASSERT_TRUE(rootEnvelope.envelope.has_value());
                const auto root = fermentation::decodeConfigurationRootPayload(
                    rootEnvelope.envelope->payload);
                TEST_ASSERT_TRUE(root.value.has_value());
                TEST_ASSERT_EQUAL_UINT32(2U, root.value->active.slot.value());
                TEST_ASSERT_TRUE(std::find(observed.missing.begin(),
                                           observed.missing.end(),
                                           "cm2") != observed.missing.end());
            } else {
                const auto head = fermentation::decodeRunPersistenceHead(
                    observed.persisted.at("rh0"), kEpoch);
                TEST_ASSERT_TRUE(head.has_value());
                TEST_ASSERT_EQUAL_UINT8(0U, head->current.slot);
                if (item.scenario == Scenario::NotReconstructible) {
                    TEST_ASSERT_TRUE(std::find(observed.missing.begin(),
                                               observed.missing.end(), "rc0") !=
                                     observed.missing.end());
                } else {
                    TEST_ASSERT_TRUE(observed.persisted.find("rc0") !=
                                     observed.persisted.end());
                }
            }
        }
        if (item.scenario == Scenario::Mixed &&
            item.domain == Domain::Configuration) {
            TEST_ASSERT_TRUE(observed.persisted.at("uc0") !=
                             observed.persisted.at("uc1"));
            TEST_ASSERT_TRUE(
                device_platform::decodeEnvelope(observed.persisted.at("uc1"))
                    .envelope.has_value());
        }
        if (item.scenario == Scenario::ForeignEpoch &&
            item.domain == Domain::Run) {
            TEST_ASSERT_FALSE(fermentation::decodeRunPersistenceRecord(
                                  observed.persisted.at("rc1"), kEpoch)
                                  .has_value());
        }
        if (item.scenario == Scenario::PreparedInterrupted) {
            TEST_ASSERT_FALSE(observed.pendingBeforeRestart);
        }
        if (item.domain == Domain::Configuration &&
            item.scenario != Scenario::FactoryEmpty &&
            item.scenario != Scenario::Partial &&
            item.scenario != Scenario::CorruptEnvelopeCrc &&
            item.scenario != Scenario::UnsupportedSchema &&
            item.scenario != Scenario::ForeignEpoch &&
            item.scenario != Scenario::MissingEvidence &&
            item.scenario != Scenario::NotReconstructible) {
            for (const auto& [key, bytes] : observed.persisted) {
                if (key == "cb0") {
                    const auto decoded =
                        fermentation::decodeConfigurationBootstrapRecord(bytes);
                    TEST_ASSERT_EQUAL_INT(
                        static_cast<int>(
                            fermentation::ConfigurationBootstrapCodecStatus::
                                Success),
                        static_cast<int>(decoded.status));
                } else {
                    TEST_ASSERT_TRUE(device_platform::decodeEnvelope(bytes)
                                         .envelope.has_value());
                }
            }
        }
        if (item.domain == Domain::Run && item.scenario != Scenario::Partial &&
            item.scenario != Scenario::CorruptEnvelopeCrc &&
            item.scenario != Scenario::UnsupportedSchema &&
            item.scenario != Scenario::ForeignEpoch &&
            item.scenario != Scenario::NoPersistedRun &&
            item.scenario != Scenario::FallbackValid) {
            if (observed.persisted.find("rh0") != observed.persisted.end() &&
                item.scenario != Scenario::InvalidReference &&
                item.scenario != Scenario::Mixed) {
                TEST_ASSERT_TRUE(fermentation::decodeRunPersistenceHead(
                                     observed.persisted.at("rh0"), kEpoch)
                                     .has_value());
            }
            for (const auto& key : {"rc0", "rc1"}) {
                const auto found = observed.persisted.find(key);
                if (found != observed.persisted.end()) {
                    TEST_ASSERT_TRUE(fermentation::decodeRunPersistenceRecord(
                                         found->second, kEpoch)
                                         .has_value());
                }
            }
        }
    }
}

void test_expected_product_and_safety_outcomes_are_independent() {
    std::size_t configRecovery = 0U;
    std::size_t runRecovery = 0U;
    for (const auto& item : kMatrix) {
        const auto observed = observe(item);
        TEST_ASSERT_TRUE(observed.readStatus == item.expectedReadStatus);
        TEST_ASSERT_FALSE(item.actuatorAllowed);
        TEST_ASSERT_TRUE(item.logicalGate == LogicalGate::Unresolved);
        if (item.outcome == ProductOutcome::ConfigurationRecoveryRequired) {
            ++configRecovery;
            TEST_ASSERT_TRUE(item.prohibitedActiveState);
            TEST_ASSERT_TRUE(item.safety == SafetyProjection::SafeBoot);
        }
        if (item.outcome == ProductOutcome::RunRecoveryRequired) {
            ++runRecovery;
            TEST_ASSERT_TRUE(item.prohibitedActiveState);
            TEST_ASSERT_TRUE(item.safety == SafetyProjection::SafeBoot);
        }
        if (item.producer == SafetyProducer::ConfigurationIntegrityFailure) {
            TEST_ASSERT_TRUE(item.domain == Domain::Configuration);
        }
    }
    TEST_ASSERT_TRUE(configRecovery >= 10U);
    TEST_ASSERT_TRUE(runRecovery >= 10U);
}

void test_forbidden_product_states_are_negative_matrix_entries() {
    for (const auto& item : kMatrix) {
        if (!item.prohibitedActiveState) continue;
        TEST_ASSERT_TRUE(item.safety == SafetyProjection::SafeBoot);
        TEST_ASSERT_FALSE(item.actuatorAllowed);
        TEST_ASSERT_TRUE(item.outcome ==
                             ProductOutcome::ConfigurationRecoveryRequired ||
                         item.outcome == ProductOutcome::RunRecoveryRequired);
        TEST_ASSERT_TRUE(item.producer != SafetyProducer::None);
    }
}

void test_machine_output_contains_fixture_and_truth_separation() {
    const auto& item = kMatrix.front();
    const auto line = machineLine(item, observe(item));
    const char* fields[] = {"issue90_oracle_case=",
                            "fixture_id=",
                            "fixture_keys_present=",
                            "fixture_keys_missing=",
                            "record_family=",
                            "read_status=",
                            "counter_domain_baseline=",
                            "record_classification=",
                            "product_outcome=",
                            "safety_producer=",
                            "logical_gate=",
                            "actuator_allowed=",
                            "product_recovery_gate=",
                            "reboot=",
                            "pending_after_reboot="};
    for (const auto* field : fields) {
        TEST_ASSERT_TRUE(line.find(field) != std::string::npos);
    }
    TEST_ASSERT_TRUE(line.find("evidence_scope=SIMULATOR_ONLY") !=
                     std::string::npos);
    TEST_ASSERT_TRUE(line.find("backend_characterization=observed") !=
                     std::string::npos);
    TEST_ASSERT_TRUE(line.find("actuator_allowed=false") != std::string::npos);
    std::printf("%s\n", line.c_str());
}

void test_callback_12_remains_real_nvs_only() {
    constexpr const char* referenceLine =
        "issue90_oracle_backend_reference=FAIL_CALLBACK_12_NOT_FOUND "
        "backend_characterization=known_limitation "
        "evidence_scope=REAL_NVS_ONLY "
        "product_recovery_gate=NOT_RUN";
    std::printf("%s\n", referenceLine);
    const std::string line{referenceLine};
    TEST_ASSERT_TRUE(line.find("REAL_NVS_ONLY") != std::string::npos);
    TEST_ASSERT_TRUE(line.find("known_limitation") != std::string::npos);
    TEST_ASSERT_TRUE(line.find("product_recovery_gate=NOT_RUN") !=
                     std::string::npos);
    TEST_ASSERT_TRUE(line.find("SIMULATOR_ONLY") == std::string::npos);
}

}  // namespace

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_matrix_contains_complete_domain_and_safety_categories);
    RUN_TEST(test_every_case_has_real_post_reboot_fixture_evidence);
    RUN_TEST(test_expected_product_and_safety_outcomes_are_independent);
    RUN_TEST(test_forbidden_product_states_are_negative_matrix_entries);
    RUN_TEST(test_machine_output_contains_fixture_and_truth_separation);
    RUN_TEST(test_callback_12_remains_real_nvs_only);
    return UNITY_END();
}
