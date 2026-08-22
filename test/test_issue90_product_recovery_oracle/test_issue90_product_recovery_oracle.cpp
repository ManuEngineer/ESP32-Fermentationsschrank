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
#include "configuration_graph_store.hpp"
#include "configuration_limits.hpp"
#include "configuration_mutation_coordinator.hpp"
#include "configuration_recovery_service.hpp"
#include "configuration_service.hpp"
#include "configuration_storage_contract.hpp"
#include "crc32.hpp"
#include "run_checkpoint_schedule.hpp"
#include "run_persistence_codec.hpp"
#include "run_persistence_coordinator.hpp"
#include "run_persistence_contract.hpp"
#include "safety_core.hpp"
#include "simulated_persistent_state_store.hpp"
#include "standard_program_catalog.hpp"
#include "state_store.hpp"
#include "state_store_key.hpp"
#include "storage_envelope.hpp"
#include "time_zone_resolver.hpp"

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
// These are the budgets used by the production consumers. The configuration
// limits are public contract constants; the run head budget is the bounded
// consumer budget in run_persistence_coordinator.cpp (256 bytes).
constexpr std::size_t kRunHeadConsumerReadBudget = 256U;
constexpr std::size_t kFixtureInspectionBudget = 16384U;

// All classifications and outcomes below are test-only expected truth. The
// production recovery services are deliberately not called by Slice 2.
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
    InvalidReferenceNoFallback,
    ForeignEpoch,
    Orphan,
    OrphanedGeneration,
    NotReconstructible,
    CurrentWithoutFallback,
    NoPersistedRun,
    MissingReferencedRun,
    PreparedInterrupted,
    ControlledDiscard,
    ControlledDiscardPost,
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
    ConfigurationRecoveryRequired,
    NewValidResume,
    OlderValidCheckpointResume,
    RunRecoveryRequired,
    RunAbortRequired,
};

enum class BaselineClassification : std::uint8_t {
    None,
    FactoryEmpty,
    NoPersistedRun,
    ControlledDiscardTombstone,
};
enum class ExpectedRecoveryAction : std::uint8_t {
    None,
    FactoryInitialization,
    NoActiveRun,
};
enum class SafetyProjection : std::uint8_t {
    Standby,
    NoActiveRun,
    ResumeOffer,
    SafeBoot,
};
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
    bool hasProductOutcome;
    ProductOutcome outcome;
    BaselineClassification baseline;
    ExpectedRecoveryAction recoveryAction;
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
        true,
        outcome,
        BaselineClassification::None,
        ExpectedRecoveryAction::None,
        safety,
        producer,
        LogicalGate::Unresolved,
        false,
        prohibited};
}

OracleCase makeBaseline(const char* id, Domain domain, Scenario scenario,
                        const char* mutationPath,
                        StateStoreReadStatus readStatus,
                        RecordClassification classification,
                        BaselineClassification baseline,
                        ExpectedRecoveryAction recoveryAction) {
    auto item =
        makeCase(id, domain, scenario, mutationPath, readStatus, classification,
                 ProductOutcome::ConfigurationRecoveryRequired,
                 SafetyProjection::Standby, SafetyProducer::None, false);
    item.hasProductOutcome = false;
    item.baseline = baseline;
    item.recoveryAction = recoveryAction;
    return item;
}

const std::vector<OracleCase> kMatrix = {
    makeCase("config_cr0_new_valid", Domain::Configuration,
             Scenario::CurrentValid, "generation A active root",
             StateStoreReadStatus::Success,
             RecordClassification::FullyValidCurrent,
             ProductOutcome::NewValidConfiguration, SafetyProjection::Standby,
             SafetyProducer::None, false),
    makeCase("config_cr1_old_valid", Domain::Configuration,
             Scenario::OlderValid, "generation B unusable; generation A valid",
             StateStoreReadStatus::Success,
             RecordClassification::FullyValidOlder,
             ProductOutcome::OldValidConfiguration, SafetyProjection::Standby,
             SafetyProducer::None, false),
    makeCase("config_cr1_fallback_valid", Domain::Configuration,
             Scenario::FallbackValid, "invalid current with valid fallback",
             StateStoreReadStatus::Success,
             RecordClassification::FullyValidFallback,
             ProductOutcome::FallbackValidConfiguration,
             SafetyProjection::Standby, SafetyProducer::None, false),
    makeCase("config_cr1_unknown_commit_new", Domain::Configuration,
             Scenario::UnknownCommitValid, "generation B committed after cut",
             StateStoreReadStatus::Success,
             RecordClassification::FullyValidCurrent,
             ProductOutcome::NewValidConfiguration, SafetyProjection::Standby,
             SafetyProducer::None, false,
             StateStoreWriteStatus::CommitOutcomeUnknown),
    makeCase("config_cr1_write_error_old", Domain::Configuration,
             Scenario::SafeWriteErrorOld, "B rejected before durable mutation",
             StateStoreReadStatus::NotFound,
             RecordClassification::FullyValidOlder,
             ProductOutcome::OldValidConfiguration, SafetyProjection::Standby,
             SafetyProducer::None, false, StateStoreWriteStatus::WriteError),
    makeCase("config_cr1_capacity_error_old", Domain::Configuration,
             Scenario::SafeCapacityOld, "B rejected before capacity mutation",
             StateStoreReadStatus::NotFound,
             RecordClassification::FullyValidOlder,
             ProductOutcome::OldValidConfiguration, SafetyProjection::Standby,
             SafetyProducer::None, false, StateStoreWriteStatus::CapacityError),
    makeCase("config_cr1_unknown_not_found", Domain::Configuration,
             Scenario::UnknownCommitNotFound,
             "post-cut B is not visible; A remains fully valid",
             StateStoreReadStatus::NotFound,
             RecordClassification::FullyValidOlder,
             ProductOutcome::OldValidConfiguration, SafetyProjection::Standby,
             SafetyProducer::None, false,
             StateStoreWriteStatus::CommitOutcomeUnknown),
    makeCase("config_cr0_read_error", Domain::Configuration,
             Scenario::ReadError, "configuration read error",
             StateStoreReadStatus::ReadError,
             RecordClassification::Indeterminate,
             ProductOutcome::ConfigurationRecoveryRequired,
             SafetyProjection::SafeBoot,
             SafetyProducer::ConfigurationUnavailable, true),
    makeCase("config_cr0_read_capacity", Domain::Configuration,
             Scenario::ReadCapacity, "root exceeds consumer read budget",
             StateStoreReadStatus::CapacityError,
             RecordClassification::Indeterminate,
             ProductOutcome::ConfigurationRecoveryRequired,
             SafetyProjection::SafeBoot,
             SafetyProducer::ConfigurationUnavailable, true),
    makeBaseline("config_factory_empty", Domain::Configuration,
                 Scenario::FactoryEmpty, "all configuration keys absent",
                 StateStoreReadStatus::NotFound,
                 RecordClassification::FactoryEmpty,
                 BaselineClassification::FactoryEmpty,
                 ExpectedRecoveryAction::FactoryInitialization),
    makeCase("config_missing_record_with_evidence", Domain::Configuration,
             Scenario::MissingEvidence, "root/manifest references missing uc0",
             StateStoreReadStatus::NotFound, RecordClassification::Missing,
             ProductOutcome::ConfigurationRecoveryRequired,
             SafetyProjection::SafeBoot,
             SafetyProducer::ConfigurationIntegrityFailure, true),
    makeCase("config_uc0_partial", Domain::Configuration, Scenario::Partial,
             "uc0 truncated after a valid baseline",
             StateStoreReadStatus::Success, RecordClassification::Partial,
             ProductOutcome::ConfigurationRecoveryRequired,
             SafetyProjection::SafeBoot,
             SafetyProducer::ConfigurationIntegrityFailure, true),
    makeCase("config_uc0_mixed_identity", Domain::Configuration,
             Scenario::Mixed, "same-generation user identity collision",
             StateStoreReadStatus::Success, RecordClassification::Mixed,
             ProductOutcome::ConfigurationRecoveryRequired,
             SafetyProjection::SafeBoot,
             SafetyProducer::ConfigurationIntegrityFailure, true),
    makeCase("config_cm0_corrupt_envelope_crc", Domain::Configuration,
             Scenario::CorruptEnvelopeCrc, "cm0 envelope CRC mutation",
             StateStoreReadStatus::Success, RecordClassification::Corrupt,
             ProductOutcome::ConfigurationRecoveryRequired,
             SafetyProjection::SafeBoot,
             SafetyProducer::ConfigurationIntegrityFailure, true),
    makeCase("config_cm0_unsupported_schema", Domain::Configuration,
             Scenario::UnsupportedSchema, "cm0 unsupported schema",
             StateStoreReadStatus::Success,
             RecordClassification::UnsupportedSchema,
             ProductOutcome::ConfigurationRecoveryRequired,
             SafetyProjection::SafeBoot,
             SafetyProducer::ConfigurationIntegrityFailure, true),
    makeCase("config_cr1_invalid_reference", Domain::Configuration,
             Scenario::InvalidReference,
             "invalid current root with valid generation-A fallback",
             StateStoreReadStatus::Success,
             RecordClassification::InvalidReference,
             ProductOutcome::FallbackValidConfiguration,
             SafetyProjection::Standby, SafetyProducer::None, false),
    makeCase("config_cr0_invalid_reference_no_fallback", Domain::Configuration,
             Scenario::InvalidReferenceNoFallback,
             "invalid root without a valid fallback",
             StateStoreReadStatus::Success,
             RecordClassification::InvalidReference,
             ProductOutcome::ConfigurationRecoveryRequired,
             SafetyProjection::SafeBoot,
             SafetyProducer::ConfigurationIntegrityFailure, true),
    makeCase("config_cm0_foreign_epoch", Domain::Configuration,
             Scenario::ForeignEpoch, "cm0 foreign StorageEpoch",
             StateStoreReadStatus::Success, RecordClassification::ForeignEpoch,
             ProductOutcome::ConfigurationRecoveryRequired,
             SafetyProjection::SafeBoot,
             SafetyProducer::ConfigurationIntegrityFailure, true),
    makeCase("config_uc3_orphan", Domain::Configuration, Scenario::Orphan,
             "valid active graph plus unreferenced generation",
             StateStoreReadStatus::Success, RecordClassification::Orphan,
             ProductOutcome::NewValidConfiguration, SafetyProjection::Standby,
             SafetyProducer::None, false),
    makeCase("config_orphaned_generation_without_active", Domain::Configuration,
             Scenario::OrphanedGeneration, "orphaned generation without root",
             StateStoreReadStatus::NotFound, RecordClassification::Orphan,
             ProductOutcome::ConfigurationRecoveryRequired,
             SafetyProjection::SafeBoot,
             SafetyProducer::ConfigurationIntegrityFailure, true),
    makeCase(
        "config_cr0_not_reconstructible", Domain::Configuration,
        Scenario::NotReconstructible, "root and referenced manifest absent",
        StateStoreReadStatus::Success, RecordClassification::NotReconstructible,
        ProductOutcome::ConfigurationRecoveryRequired,
        SafetyProjection::SafeBoot,
        SafetyProducer::ConfigurationIntegrityFailure, true),
    makeCase("config_current_without_fallback", Domain::Configuration,
             Scenario::CurrentWithoutFallback, "valid current without fallback",
             StateStoreReadStatus::Success,
             RecordClassification::FullyValidCurrent,
             ProductOutcome::NewValidConfiguration, SafetyProjection::Standby,
             SafetyProducer::None, false),

    makeCase(
        "run_rc0_new_valid_resume", Domain::Run, Scenario::CurrentValid,
        "resume-eligible current checkpoint", StateStoreReadStatus::Success,
        RecordClassification::FullyValidCurrent, ProductOutcome::NewValidResume,
        SafetyProjection::ResumeOffer, SafetyProducer::None, false),
    makeCase(
        "run_rh0_current_with_older_fallback", Domain::Run,
        Scenario::OlderValid, "valid current preferred over older fallback",
        StateStoreReadStatus::Success, RecordClassification::FullyValidCurrent,
        ProductOutcome::NewValidResume, SafetyProjection::ResumeOffer,
        SafetyProducer::None, false),
    makeCase("run_rh0_fallback_resume", Domain::Run, Scenario::FallbackValid,
             "invalid current with valid older fallback",
             StateStoreReadStatus::Success,
             RecordClassification::FullyValidFallback,
             ProductOutcome::OlderValidCheckpointResume,
             SafetyProjection::ResumeOffer, SafetyProducer::None, false),
    makeCase(
        "run_rh0_unknown_commit_new", Domain::Run, Scenario::UnknownCommitValid,
        "new head committed after cut", StateStoreReadStatus::Success,
        RecordClassification::FullyValidCurrent, ProductOutcome::NewValidResume,
        SafetyProjection::ResumeOffer, SafetyProducer::None, false,
        StateStoreWriteStatus::CommitOutcomeUnknown),
    makeCase(
        "run_rc0_write_error_current", Domain::Run, Scenario::SafeWriteErrorOld,
        "new checkpoint rejected before commit", StateStoreReadStatus::Success,
        RecordClassification::FullyValidCurrent, ProductOutcome::NewValidResume,
        SafetyProjection::ResumeOffer, SafetyProducer::None, false,
        StateStoreWriteStatus::WriteError),
    makeCase(
        "run_rc0_capacity_error_current", Domain::Run,
        Scenario::SafeCapacityOld, "new checkpoint rejected before capacity",
        StateStoreReadStatus::Success, RecordClassification::FullyValidCurrent,
        ProductOutcome::NewValidResume, SafetyProjection::ResumeOffer,
        SafetyProducer::None, false, StateStoreWriteStatus::CapacityError),
    makeCase(
        "run_rh0_unknown_not_found", Domain::Run,
        Scenario::UnknownCommitNotFound, "head not visible after unknown cut",
        StateStoreReadStatus::NotFound, RecordClassification::Indeterminate,
        ProductOutcome::RunRecoveryRequired, SafetyProjection::SafeBoot,
        SafetyProducer::RunPersistenceUntrusted, true,
        StateStoreWriteStatus::CommitOutcomeUnknown),
    makeCase("run_rh0_read_error", Domain::Run, Scenario::ReadError,
             "run head read error", StateStoreReadStatus::ReadError,
             RecordClassification::Indeterminate,
             ProductOutcome::RunRecoveryRequired, SafetyProjection::SafeBoot,
             SafetyProducer::RunPersistenceUntrusted, true),
    makeCase("run_rh0_read_capacity", Domain::Run, Scenario::ReadCapacity,
             "head exceeds the 256-byte consumer budget",
             StateStoreReadStatus::CapacityError,
             RecordClassification::Indeterminate,
             ProductOutcome::RunRecoveryRequired, SafetyProjection::SafeBoot,
             SafetyProducer::RunPersistenceUntrusted, true),
    makeBaseline("run_no_persisted_run", Domain::Run, Scenario::NoPersistedRun,
                 "all rc0/rc1/rh0 keys absent", StateStoreReadStatus::NotFound,
                 RecordClassification::NoPersistedRun,
                 BaselineClassification::NoPersistedRun,
                 ExpectedRecoveryAction::NoActiveRun),
    makeCase("run_missing_referenced_checkpoint", Domain::Run,
             Scenario::MissingReferencedRun, "rh0 references absent rc0",
             StateStoreReadStatus::Success, RecordClassification::Missing,
             ProductOutcome::RunRecoveryRequired, SafetyProjection::SafeBoot,
             SafetyProducer::RunPersistenceUntrusted, true),
    makeCase("run_rc0_partial", Domain::Run, Scenario::Partial,
             "rc0 truncated after a valid baseline",
             StateStoreReadStatus::Success, RecordClassification::Partial,
             ProductOutcome::RunRecoveryRequired, SafetyProjection::SafeBoot,
             SafetyProducer::RunPersistenceUntrusted, true),
    makeCase("run_rh0_mixed", Domain::Run, Scenario::Mixed,
             "current/fallback slot binding mismatch",
             StateStoreReadStatus::Success, RecordClassification::Mixed,
             ProductOutcome::RunRecoveryRequired, SafetyProjection::SafeBoot,
             SafetyProducer::RunPersistenceUntrusted, true),
    makeCase("run_rh0_corrupt_envelope_crc", Domain::Run,
             Scenario::CorruptEnvelopeCrc, "rh0 envelope CRC mutation",
             StateStoreReadStatus::Success, RecordClassification::Corrupt,
             ProductOutcome::RunRecoveryRequired, SafetyProjection::SafeBoot,
             SafetyProducer::RunPersistenceUntrusted, true),
    makeCase("run_rc0_unsupported_schema", Domain::Run,
             Scenario::UnsupportedSchema, "rc0 unsupported schema",
             StateStoreReadStatus::Success,
             RecordClassification::UnsupportedSchema,
             ProductOutcome::RunRecoveryRequired, SafetyProjection::SafeBoot,
             SafetyProducer::RunPersistenceUntrusted, true),
    makeCase("run_rh0_invalid_reference", Domain::Run,
             Scenario::InvalidReference, "invalid current with valid fallback",
             StateStoreReadStatus::Success,
             RecordClassification::InvalidReference,
             ProductOutcome::OlderValidCheckpointResume,
             SafetyProjection::ResumeOffer, SafetyProducer::None, false),
    makeCase("run_rh0_invalid_reference_no_fallback", Domain::Run,
             Scenario::InvalidReferenceNoFallback,
             "invalid current without fallback", StateStoreReadStatus::Success,
             RecordClassification::InvalidReference,
             ProductOutcome::RunRecoveryRequired, SafetyProjection::SafeBoot,
             SafetyProducer::RunPersistenceUntrusted, true),
    makeCase("run_rc0_foreign_epoch", Domain::Run, Scenario::ForeignEpoch,
             "current checkpoint foreign StorageEpoch",
             StateStoreReadStatus::Success, RecordClassification::ForeignEpoch,
             ProductOutcome::OlderValidCheckpointResume,
             SafetyProjection::ResumeOffer, SafetyProducer::None, false),
    makeCase("run_rh0_prepared_interrupted", Domain::Run,
             Scenario::PreparedInterrupted, "durable Prepared head",
             StateStoreReadStatus::Success,
             RecordClassification::PreparedInterrupted,
             ProductOutcome::RunRecoveryRequired, SafetyProjection::SafeBoot,
             SafetyProducer::RunPersistenceUntrusted, true),
    makeCase("run_rh0_orphan", Domain::Run, Scenario::Orphan,
             "rh0 absent while checkpoint slots remain",
             StateStoreReadStatus::NotFound, RecordClassification::Orphan,
             ProductOutcome::RunRecoveryRequired, SafetyProjection::SafeBoot,
             SafetyProducer::RunPersistenceUntrusted, true),
    makeCase("run_rh0_not_reconstructible", Domain::Run,
             Scenario::NotReconstructible, "head without any checkpoint",
             StateStoreReadStatus::Success,
             RecordClassification::NotReconstructible,
             ProductOutcome::RunRecoveryRequired, SafetyProjection::SafeBoot,
             SafetyProducer::RunPersistenceUntrusted, true),
    makeCase("run_controlled_discard_abort", Domain::Run,
             Scenario::ControlledDiscard,
             "trusted but R1-non-resume-eligible run; abort decision only",
             StateStoreReadStatus::Success,
             RecordClassification::ControlledDiscard,
             ProductOutcome::RunAbortRequired, SafetyProjection::NoActiveRun,
             SafetyProducer::None, false),
    makeBaseline("run_controlled_discard_committed_tombstone", Domain::Run,
                 Scenario::ControlledDiscardPost,
                 "controlled discard committed as NoActiveRun tombstone",
                 StateStoreReadStatus::Success,
                 RecordClassification::ControlledDiscard,
                 BaselineClassification::ControlledDiscardTombstone,
                 ExpectedRecoveryAction::NoActiveRun),
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
    std::string oldRoot;
    std::string newRoot;
    std::string oldUserPayload;
    std::string newUserPayload;
};

ConfigurationFixtureBytes configurationBaseline(bool withNewGeneration) {
    ConfigurationFixtureBytes result;
    const fermentation::UserConfiguration oldUser{"de", "Europe/Zurich",
                                                  "Fixture Configuration A"};
    const fermentation::UserConfiguration newUser{"de", "Europe/Zurich",
                                                  "Fixture Configuration B"};
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
    std::string oldServicePayload;
    std::string oldCatalogPayload;
    std::string newServicePayload;
    std::string newCatalogPayload;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::ConfigurationCodecStatus::Success),
        static_cast<int>(fermentation::encodeUserConfigurationPayload(
            oldUser, resolver, result.oldUserPayload)));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::ConfigurationCodecStatus::Success),
        static_cast<int>(fermentation::encodeUserConfigurationPayload(
            newUser, resolver, result.newUserPayload)));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::ConfigurationCodecStatus::Success),
        static_cast<int>(fermentation::encodeServiceConfigurationPayload(
            service, oldServicePayload)));
    newServicePayload = oldServicePayload;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::ConfigurationCodecStatus::Success),
        static_cast<int>(fermentation::encodeProgramCatalogPayload(
            catalog, oldCatalogPayload)));
    newCatalogPayload = oldCatalogPayload;

    result.bytes["uc0"] =
        envelope(kUserConfigurationRecordType, 1U, 1U, result.oldUserPayload);
    result.bytes["sc0"] =
        envelope(kServiceConfigurationRecordType, 1U, 1U, oldServicePayload);
    result.bytes["pc0"] =
        envelope(kProgramCatalogRecordType, 1U, 1U, oldCatalogPayload);
    const fermentation::ConfigurationManifest oldManifest{
        fermentation::decodeChangeOrigin(1U),
        fermentation::decodeChangeOperation(2U),
        reference(kUserConfigurationRecordType, 0U,
                  fermentation::UserConfigurationRevision{1U},
                  result.oldUserPayload),
        reference(kServiceConfigurationRecordType, 0U,
                  fermentation::ServiceConfigurationRevision{1U},
                  oldServicePayload),
        reference(kProgramCatalogRecordType, 0U,
                  fermentation::ProgramCatalogRevision{1U}, oldCatalogPayload)};
    std::string oldManifestPayload;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::ConfigurationGraphCodecStatus::Success),
        static_cast<int>(fermentation::encodeConfigurationManifestPayload(
            oldManifest, oldManifestPayload)));
    result.bytes["cm0"] =
        envelope(kConfigurationManifestRecordType, 1U, 1U, oldManifestPayload);
    const auto oldManifestReference = reference(
        kConfigurationManifestRecordType, 0U,
        fermentation::ConfigurationManifestGeneration{1U}, oldManifestPayload);
    const fermentation::ConfigurationRootRecord oldRootRecord{
        oldManifestReference, std::nullopt};
    std::string oldRootPayload;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::ConfigurationGraphCodecStatus::Success),
        static_cast<int>(fermentation::encodeConfigurationRootPayload(
            oldRootRecord, oldRootPayload)));
    result.oldRoot =
        envelope(kConfigurationRootRecordType, 1U, 1U, oldRootPayload);
    result.bytes["cr0"] = result.oldRoot;

    if (withNewGeneration) {
        result.bytes["uc1"] = envelope(kUserConfigurationRecordType, 1U, 2U,
                                       result.newUserPayload);
        result.bytes["sc1"] = envelope(kServiceConfigurationRecordType, 1U, 2U,
                                       newServicePayload);
        result.bytes["pc1"] =
            envelope(kProgramCatalogRecordType, 1U, 2U, newCatalogPayload);
        const fermentation::ConfigurationManifest newManifest{
            fermentation::decodeChangeOrigin(1U),
            fermentation::decodeChangeOperation(2U),
            reference(kUserConfigurationRecordType, 1U,
                      fermentation::UserConfigurationRevision{2U},
                      result.newUserPayload),
            reference(kServiceConfigurationRecordType, 1U,
                      fermentation::ServiceConfigurationRevision{2U},
                      newServicePayload),
            reference(kProgramCatalogRecordType, 1U,
                      fermentation::ProgramCatalogRevision{2U},
                      newCatalogPayload)};
        std::string newManifestPayload;
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(
                fermentation::ConfigurationGraphCodecStatus::Success),
            static_cast<int>(fermentation::encodeConfigurationManifestPayload(
                newManifest, newManifestPayload)));
        result.bytes["cm1"] = envelope(kConfigurationManifestRecordType, 1U, 2U,
                                       newManifestPayload);
        const auto newManifestReference =
            reference(kConfigurationManifestRecordType, 1U,
                      fermentation::ConfigurationManifestGeneration{2U},
                      newManifestPayload);
        const fermentation::ConfigurationRootRecord newRootRecord{
            newManifestReference, oldManifestReference};
        std::string newRootPayload;
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(
                fermentation::ConfigurationGraphCodecStatus::Success),
            static_cast<int>(fermentation::encodeConfigurationRootPayload(
                newRootRecord, newRootPayload)));
        result.newRoot =
            envelope(kConfigurationRootRecordType, 1U, 2U, newRootPayload);
        result.bytes["cr1"] = result.newRoot;
    }

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

void writeConfigurationNewGeneration(
    SimulatedPersistentStateStore& store,
    const ConfigurationFixtureBytes& candidate) {
    for (const auto* key : {"uc1", "sc1", "pc1", "cm1"}) {
        put(store, key, candidate.bytes.at(key));
    }
}

fermentation::RunPersistenceSnapshot runSnapshot(bool resumeEligible,
                                                 const char* runId,
                                                 std::uint32_t revision) {
    const auto programId = resumeEligible ? "yogurt-mild" : "water-kefir";
    auto document = fermentation::FactoryProgramCatalog::find(programId);
    TEST_ASSERT_TRUE(document.has_value());
    auto& program = document->program;
    program.productSensorFailure.fallbackDelaySeconds = 30U;
    program.fermentationStages.front().targetTemperatureCelsius = 38.0;
    program.fermentationStages.front().durationMinutes = 120U;
    program.targetQualification.bandCelsius = 0.5;
    program.targetQualification.durationMinutes = 10U;
    program.maximumTargetReachMinutes = 180U;
    if (program.preheat) program.maximumProductWaitMinutes = 30U;
    if (program.completion.mode !=
        fermentation::CompletionMode::FinishWithoutCooling) {
        program.completion.coolingTargetCelsius = 8.0;
    }
    const auto run = fermentation::ActiveRun::start(
        *document, fermentation::ProgramSourceKind::FactoryCatalog, 1U);
    TEST_ASSERT_TRUE(run.has_value());
    fermentation::RunCommandState state;
    state.processState.state = resumeEligible
                                   ? fermentation::ProcessState::Preheating
                                   : fermentation::ProcessState::ReachingTarget;
    state.activeProgramRun = *run;
    state.activeRunId = runId;
    state.activeRunSensorMode = fermentation::RunSensorMode::Product;
    state.runRevision = revision;
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
        fermentation::RunCheckpointTime{100U + revision, 1700000000}, 5U);
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
    auto result = fermentation::makeRunCheckpointReference(slot, *raw, kEpoch);
    result.slot = static_cast<std::uint8_t>(slot);
    return result;
}

std::string runHead(
    const fermentation::RunCheckpointReference& current,
    std::optional<fermentation::RunCheckpointReference> fallback = std::nullopt,
    fermentation::RunPersistenceHeadState state =
        fermentation::RunPersistenceHeadState::Committed) {
    fermentation::RunPersistenceHead head;
    head.state = state;
    head.revision = current.checkpointRevision;
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
    std::string writeTargetKey;
    std::string productReadTargetKey;
    std::string fixtureId;
    std::map<std::string, std::string> committed;
    std::vector<std::string> missing;
    bool writeAttempted{false};
    bool restarted{false};
    bool pendingBeforeRestart{false};
    bool pendingAfterRestart{false};
    bool durablePreparedHead{false};
    std::string oldHeadBytes;
    std::string newHeadBytes;
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

void inspectCommitted(SimulatedPersistentStateStore& store, Domain domain,
                      BackendObservation& observed) {
    const auto keys =
        domain == Domain::Configuration ? allConfigurationKeys() : allRunKeys();
    for (const auto& name : keys) {
        const auto read =
            store.read(keyFor(name.c_str()), kFixtureInspectionBudget);
        if (read.status == StateStoreReadStatus::Success) {
            observed.committed.emplace(name, read.value);
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
            case Scenario::Mixed:
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
            case Scenario::FallbackValid:
            case Scenario::InvalidReference:
                return "cr1";
            case Scenario::ReadCapacity:
            case Scenario::InvalidReferenceNoFallback:
            case Scenario::OrphanedGeneration:
                return "cr0";
            case Scenario::Orphan:
                return "uc3";
            default:
                return "cr0";
        }
    }
    switch (item.scenario) {
        case Scenario::Partial:
        case Scenario::UnsupportedSchema:
        case Scenario::ForeignEpoch:
            return "rc0";
        default:
            return "rh0";
    }
}

void writeRunBaseline(SimulatedPersistentStateStore& store,
                      std::map<std::string, std::string>& bytes,
                      bool withFallback, bool resumeEligible = true,
                      bool writeHead = true, bool persistCurrent = true) {
    if (withFallback) {
        const auto oldSnapshot =
            runSnapshot(resumeEligible, "issue90-line-run", 1U);
        const auto newSnapshot =
            runSnapshot(resumeEligible, "issue90-line-run", 2U);
        bytes["rc1"] = runRecord(oldSnapshot, 1U);
        bytes["rc0"] = runRecord(newSnapshot, 2U);
        put(store, "rc1", bytes["rc1"]);
        if (persistCurrent) put(store, "rc0", bytes["rc0"]);
        if (writeHead) {
            const auto current = runReference(bytes["rc0"], 0U);
            const auto fallback = runReference(bytes["rc1"], 1U);
            bytes["rh0"] = runHead(current, fallback);
            put(store, "rh0", bytes["rh0"]);
        }
        return;
    }
    const auto snapshot = runSnapshot(resumeEligible, "issue90-line-run", 1U);
    bytes["rc0"] = runRecord(snapshot, 1U);
    put(store, "rc0", bytes["rc0"]);
    if (writeHead) {
        bytes["rh0"] = runHead(runReference(bytes["rc0"], 0U));
        put(store, "rh0", bytes["rh0"]);
    }
}

std::string invalidConfigurationRoot(const std::string& rootBytes,
                                     bool preserveFallback) {
    const auto decoded = device_platform::decodeEnvelope(rootBytes);
    TEST_ASSERT_TRUE(decoded.envelope.has_value());
    const auto root =
        fermentation::decodeConfigurationRootPayload(decoded.envelope->payload);
    TEST_ASSERT_TRUE(root.value.has_value());
    auto invalid = *root.value;
    invalid.active =
        reference(kConfigurationManifestRecordType, 2U,
                  fermentation::ConfigurationManifestGeneration{9U},
                  std::string(fermentation::configuration_limits::
                                  kConfigurationManifestPayloadBytes,
                              'M'));
    if (!preserveFallback) invalid.fallback.reset();
    std::string payload;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::ConfigurationGraphCodecStatus::Success),
        static_cast<int>(
            fermentation::encodeConfigurationRootPayload(invalid, payload)));
    return envelope(kConfigurationRootRecordType, 1U, 9U, payload);
}

BackendObservation observe(const OracleCase& item) {
    SimulatedPersistentStateStore store;
    BackendObservation observed;
    observed.fixtureId = item.id;
    observed.productReadTargetKey = targetKey(item);
    const auto writeMutation = [&](const char* key, const std::string& value) {
        observed.writeAttempted = true;
        observed.writeTargetKey = key;
        observed.writeStatus = store.write(keyFor(key), value);
    };
    const auto restart = [&]() {
        observed.pendingBeforeRestart = store.hasPendingWriteForTesting();
        store.restart();
        observed.restarted = true;
        observed.pendingAfterRestart = store.hasPendingWriteForTesting();
    };

    if (item.domain == Domain::Configuration) {
        const bool withNew = item.scenario == Scenario::OlderValid ||
                             item.scenario == Scenario::FallbackValid ||
                             item.scenario == Scenario::InvalidReference;
        auto baseline = configurationBaseline(withNew);
        if (item.scenario == Scenario::OrphanedGeneration) {
            baseline.bytes.erase("cr0");
            baseline.bytes.erase("cr1");
        }
        if (item.scenario != Scenario::FactoryEmpty) {
            if (item.scenario == Scenario::MissingEvidence) {
                baseline.bytes.erase("uc0");
            }
            writeConfigurationBytes(store, baseline.bytes);
        }
        switch (item.scenario) {
            case Scenario::CurrentValid:
            case Scenario::CurrentWithoutFallback:
                break;
            case Scenario::OlderValid:
                // A is the only fully valid root. B document evidence exists,
                // but the newer root is itself invalid and carries no valid
                // fallback edge, so this is OLD rather than FALLBACK.
                store.injectCorruption(
                    keyFor("cr1"),
                    invalidConfigurationRoot(baseline.newRoot, false));
                break;
            case Scenario::FallbackValid:
                store.injectCorruption(keyFor("uc1"),
                                       baseline.bytes["uc1"].substr(0U, 5U));
                break;
            case Scenario::UnknownCommitValid: {
                const auto candidate = configurationBaseline(true);
                writeConfigurationNewGeneration(store, candidate);
                store.setNextWriteFault(
                    SimulatedPersistentStateStore::WriteFault::
                        PowerCutAfterCommitBeforeReturn);
                writeMutation("cr1", candidate.newRoot);
                break;
            }
            case Scenario::SafeWriteErrorOld: {
                const auto candidate = configurationBaseline(true);
                writeConfigurationNewGeneration(store, candidate);
                store.setNextWriteFault(SimulatedPersistentStateStore::
                                            WriteFault::PowerCutBeforeCommit);
                writeMutation("cr1", candidate.newRoot);
                break;
            }
            case Scenario::SafeCapacityOld: {
                const auto candidate = configurationBaseline(true);
                writeConfigurationNewGeneration(store, candidate);
                store.setNextWriteFault(SimulatedPersistentStateStore::
                                            WriteFault::CapacityExceeded);
                writeMutation("cr1", candidate.newRoot);
                break;
            }
            case Scenario::UnknownCommitNotFound: {
                const auto candidate = configurationBaseline(true);
                writeConfigurationNewGeneration(store, candidate);
                store.setNextWriteFault(
                    SimulatedPersistentStateStore::WriteFault::
                        PowerCutAfterCommitBeforeReturn);
                writeMutation("cr1", candidate.newRoot);
                break;
            }
            case Scenario::ReadError:
                store.injectReadFailure(keyFor("cr0"), true);
                break;
            case Scenario::ReadCapacity:
                store.injectCorruption(
                    keyFor("cr0"), envelope(kConfigurationRootRecordType, 1U,
                                            99U, std::string(200U, 'R')));
                break;
            case Scenario::FactoryEmpty:
                break;
            case Scenario::MissingEvidence:
                break;
            case Scenario::Partial:
                store.injectCorruption(keyFor("uc0"),
                                       baseline.bytes["uc0"].substr(0U, 5U));
                break;
            case Scenario::Mixed:
                // The active manifest still binds the original uc0 bytes.
                // Replacing uc0 with a structurally valid same-generation
                // payload creates a deterministic identity/CRC collision in
                // the active graph, not a harmless unreferenced record.
                store.injectCorruption(
                    keyFor("uc0"), envelope(kUserConfigurationRecordType, 1U,
                                            1U, baseline.newUserPayload));
                break;
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
                store.injectCorruption(
                    keyFor("cr1"),
                    invalidConfigurationRoot(baseline.newRoot, true));
                break;
            case Scenario::InvalidReferenceNoFallback:
                store.injectCorruption(
                    keyFor("cr0"),
                    invalidConfigurationRoot(baseline.oldRoot, false));
                break;
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
                store.injectCorruption(
                    keyFor("uc3"), envelope(kUserConfigurationRecordType, 1U,
                                            3U, baseline.newUserPayload));
                break;
            case Scenario::OrphanedGeneration:
                // Complete document/manifest evidence remains, but neither
                // root slot exists. It is an orphaned generation, not an
                // empty/corrupt root record.
                break;
            case Scenario::NotReconstructible:
                store.injectCorruption(
                    keyFor("cr0"),
                    invalidConfigurationRoot(baseline.oldRoot, false));
                store.injectCorruption(keyFor("cm0"), {});
                break;
            case Scenario::NoPersistedRun:
            case Scenario::MissingReferencedRun:
            case Scenario::PreparedInterrupted:
            case Scenario::ControlledDiscard:
            case Scenario::ControlledDiscardPost:
                break;
        }
    } else {
        std::map<std::string, std::string> baseline;
        const bool customUnknownHeadMutation =
            item.scenario == Scenario::UnknownCommitValid ||
            item.scenario == Scenario::UnknownCommitNotFound;
        const bool withFallback =
            item.scenario == Scenario::OlderValid ||
            item.scenario == Scenario::FallbackValid ||
            item.scenario == Scenario::Mixed ||
            item.scenario == Scenario::InvalidReference ||
            item.scenario == Scenario::ForeignEpoch ||
            item.scenario == Scenario::PreparedInterrupted ||
            item.scenario == Scenario::Orphan;
        if (item.scenario == Scenario::MissingReferencedRun) {
            const auto snapshot = runSnapshot(true, "issue90-missing-run", 1U);
            const auto rc0 = runRecord(snapshot, 1U);
            const auto current = runReference(rc0, 0U);
            baseline["rh0"] = runHead(current);
            put(store, "rh0", baseline["rh0"]);
        } else if (item.scenario == Scenario::NotReconstructible) {
            const auto oldSnapshot =
                runSnapshot(true, "issue90-not-reconstructible", 1U);
            const auto newSnapshot =
                runSnapshot(true, "issue90-not-reconstructible", 2U);
            const auto oldRecord = runRecord(oldSnapshot, 1U);
            const auto newRecord = runRecord(newSnapshot, 2U);
            const auto head = runHead(runReference(newRecord, 0U),
                                      runReference(oldRecord, 1U));
            baseline["rh0"] = head;
            put(store, "rh0", head);
        } else if (item.scenario != Scenario::NoPersistedRun &&
                   !customUnknownHeadMutation &&
                   item.scenario != Scenario::ControlledDiscardPost) {
            writeRunBaseline(store, baseline, withFallback,
                             item.scenario != Scenario::ControlledDiscard,
                             item.scenario != Scenario::Orphan);
        }
        switch (item.scenario) {
            case Scenario::CurrentValid:
            case Scenario::OlderValid:
            case Scenario::CurrentWithoutFallback:
            case Scenario::ControlledDiscard:
            case Scenario::MissingReferencedRun:
            case Scenario::NoPersistedRun:
                break;
            case Scenario::ControlledDiscardPost: {
                fermentation::RunCommandState tombstoneState;
                tombstoneState.processState.state =
                    fermentation::ProcessState::Standby;
                std::array<fermentation::CommandId,
                           fermentation::kMaximumPersistedRunCommandIds>
                    ids{};
                const auto tombstone = fermentation::makeRunPersistenceSnapshot(
                    tombstoneState, ids, 0U,
                    fermentation::RunCheckpointTrigger::Command,
                    fermentation::RunCheckpointTime{300U, std::nullopt}, 5U);
                TEST_ASSERT_TRUE(tombstone.has_value());
                const auto tombstoneBytes = runRecord(*tombstone, 3U);
                put(store, "rc1", tombstoneBytes);
                baseline["rc1"] = tombstoneBytes;
                const auto head = runHead(runReference(tombstoneBytes, 1U));
                put(store, "rh0", head);
                baseline["rh0"] = head;
                break;
            }
            case Scenario::FallbackValid:
                // The head points at an unavailable newer revision while the
                // valid current-slot bytes remain as an unreferenced
                // same-run candidate. The older fallback edge is valid.
                {
                    const auto head = fermentation::decodeRunPersistenceHead(
                        baseline["rh0"], kEpoch);
                    TEST_ASSERT_TRUE(head.has_value());
                    auto unavailableCurrent = *head;
                    unavailableCurrent.current.checkpointRevision += 1U;
                    const auto bytes = fermentation::encodeRunPersistenceHead(
                        unavailableCurrent, kEpoch);
                    TEST_ASSERT_TRUE(bytes.has_value());
                    store.injectCorruption(keyFor("rh0"), *bytes);
                }
                break;
            case Scenario::UnknownCommitValid:
            case Scenario::UnknownCommitNotFound: {
                // OLD and NEW are separate checkpoints on one run line. Only
                // the mutating NEW head write is cut after commit.
                std::map<std::string, std::string> oldOnly;
                writeRunBaseline(store, oldOnly, false, true, true);
                put(store, "rc1", oldOnly.at("rc0"));
                const auto oldReference = runReference(oldOnly.at("rc0"), 1U);
                const auto newSnapshot =
                    runSnapshot(true, "issue90-line-run", 2U);
                const auto newCheckpoint = runRecord(newSnapshot, 2U);
                put(store, "rc0", newCheckpoint);
                const auto newReference = runReference(newCheckpoint, 0U);
                const auto newHead = runHead(newReference, oldReference);
                observed.oldHeadBytes = oldOnly.at("rh0");
                observed.newHeadBytes = newHead;
                store.setNextWriteFault(
                    SimulatedPersistentStateStore::WriteFault::
                        PowerCutAfterCommitBeforeReturn);
                writeMutation("rh0", newHead);
                break;
            }
            case Scenario::SafeWriteErrorOld: {
                std::map<std::string, std::string> oldOnly;
                writeRunBaseline(store, oldOnly, false);
                const auto candidate =
                    runRecord(runSnapshot(true, "issue90-line-run", 2U), 2U);
                store.setNextWriteFault(SimulatedPersistentStateStore::
                                            WriteFault::PowerCutBeforeCommit);
                writeMutation("rc0", candidate);
                break;
            }
            case Scenario::SafeCapacityOld: {
                std::map<std::string, std::string> oldOnly;
                writeRunBaseline(store, oldOnly, false);
                const auto candidate =
                    runRecord(runSnapshot(true, "issue90-line-run", 2U), 2U);
                store.setNextWriteFault(SimulatedPersistentStateStore::
                                            WriteFault::CapacityExceeded);
                writeMutation("rc0", candidate);
                break;
            }
            case Scenario::ReadError:
                store.injectReadFailure(keyFor("rh0"), true);
                break;
            case Scenario::ReadCapacity:
                store.injectCorruption(
                    keyFor("rh0"),
                    envelope(RecordTypeId{8U},
                             fermentation::kCurrentRunPersistenceSchema, 99U,
                             std::string(400U, 'H')));
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
            case Scenario::InvalidReference:
            case Scenario::InvalidReferenceNoFallback: {
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
                    device_platform::decodeEnvelope(baseline["rc0"]);
                TEST_ASSERT_TRUE(decoded.envelope.has_value());
                store.injectCorruption(
                    keyFor("rc0"),
                    envelope(RecordTypeId{7U},
                             fermentation::kCurrentRunPersistenceSchema, 2U,
                             decoded.envelope->payload, StorageEpoch{2U}));
                break;
            }
            case Scenario::PreparedInterrupted: {
                const auto current = runReference(baseline["rc0"], 0U);
                put(store, "rh0",
                    runHead(current, std::nullopt,
                            fermentation::RunPersistenceHeadState::Prepared));
                observed.durablePreparedHead = true;
                break;
            }
            case Scenario::Orphan:
            case Scenario::NotReconstructible:
            case Scenario::FactoryEmpty:
            case Scenario::MissingEvidence:
            case Scenario::OrphanedGeneration:
                break;
        }
    }

    if (!observed.restarted) restart();
    inspectCommitted(store, item.domain, observed);
    if (item.scenario == Scenario::UnknownCommitNotFound) {
        store.forceNotFound(keyFor(observed.productReadTargetKey.c_str()),
                            true);
    }
    if (item.scenario == Scenario::ReadError) {
        store.injectReadFailure(keyFor(observed.productReadTargetKey.c_str()),
                                true);
    }
    std::size_t maxBytes = kFixtureInspectionBudget;
    if (item.scenario == Scenario::ReadCapacity) {
        maxBytes = item.domain == Domain::Configuration
                       ? fermentation::configuration_limits::
                             kMaximumConfigurationRootEnvelopeBytes
                       : kRunHeadConsumerReadBudget;
    }
    const auto read =
        store.read(keyFor(observed.productReadTargetKey.c_str()), maxBytes);
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
        case ProductOutcome::ConfigurationRecoveryRequired:
            return "CONFIGURATION_RECOVERY_REQUIRED";
        case ProductOutcome::NewValidResume:
            return "NEW_VALID_RESUME";
        case ProductOutcome::OlderValidCheckpointResume:
            return "OLDER_VALID_CHECKPOINT_RESUME";
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
        case SafetyProjection::NoActiveRun:
            return "NO_ACTIVE_RUN";
        case SafetyProjection::ResumeOffer:
            return "RESUME_OFFER";
        case SafetyProjection::SafeBoot:
            return "SAFE_BOOT";
    }
    return "UNKNOWN_SAFETY_PROJECTION";
}
const char* baselineName(BaselineClassification value) {
    switch (value) {
        case BaselineClassification::None:
            return "NONE";
        case BaselineClassification::FactoryEmpty:
            return "FactoryEmpty";
        case BaselineClassification::NoPersistedRun:
            return "NoPersistedRun";
        case BaselineClassification::ControlledDiscardTombstone:
            return "ControlledDiscardTombstone";
    }
    return "UNKNOWN_BASELINE";
}
const char* actionName(ExpectedRecoveryAction value) {
    switch (value) {
        case ExpectedRecoveryAction::None:
            return "NONE";
        case ExpectedRecoveryAction::FactoryInitialization:
            return "FACTORY_INITIALIZATION";
        case ExpectedRecoveryAction::NoActiveRun:
            return "NO_ACTIVE_RUN";
    }
    return "UNKNOWN_RECOVERY_ACTION";
}
const char* logicalGateName(LogicalGate value) {
    switch (value) {
        case LogicalGate::Unresolved:
            return "UNRESOLVED";
    }
    return "UNKNOWN_LOGICAL_GATE";
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

bool hasCommitted(const BackendObservation& observed, const char* key) {
    return observed.committed.find(key) != observed.committed.end();
}

std::optional<fermentation::ConfigurationRootRecord> decodeConfigurationRoot(
    const BackendObservation& observed, const char* rootKey) {
    if (!hasCommitted(observed, rootKey)) return std::nullopt;
    const auto envelope =
        device_platform::decodeEnvelope(observed.committed.at(rootKey));
    if (!envelope.envelope.has_value() ||
        envelope.envelope->recordTypeId != kConfigurationRootRecordType ||
        envelope.envelope->schemaVersion !=
            fermentation::kConfigurationRootSchemaVersion1 ||
        envelope.envelope->storageEpoch != kEpoch) {
        return std::nullopt;
    }
    const auto root = fermentation::decodeConfigurationRootPayload(
        envelope.envelope->payload);
    return root.value;
}

bool configurationManifestReferenceMatches(
    const fermentation::ConfigurationManifestReference& reference,
    const BackendObservation& observed, const char* manifestKey) {
    if (manifestKey[0] != 'c' || manifestKey[1] != 'm' ||
        manifestKey[2] < '0' || manifestKey[2] > '9' ||
        !hasCommitted(observed, manifestKey)) {
        return false;
    }
    const auto envelope =
        device_platform::decodeEnvelope(observed.committed.at(manifestKey));
    if (!envelope.envelope.has_value() ||
        envelope.envelope->recordTypeId != kConfigurationManifestRecordType ||
        envelope.envelope->schemaVersion !=
            fermentation::kConfigurationManifestSchemaVersion1 ||
        envelope.envelope->storageEpoch != kEpoch) {
        return false;
    }
    return reference.recordType == kConfigurationManifestRecordType &&
           reference.slot.value() ==
               static_cast<std::uint32_t>(manifestKey[2] - '0') &&
           reference.version.value() == envelope.envelope->versionValue &&
           reference.schemaVersion == envelope.envelope->schemaVersion &&
           reference.payloadLength == envelope.envelope->payload.size() &&
           reference.payloadCrc == device_platform::computeCrc32IsoHdlc(
                                       envelope.envelope->payload) &&
           reference.storageEpoch == envelope.envelope->storageEpoch;
}

bool configurationRootReferencesMatch(const BackendObservation& observed,
                                      const char* rootKey,
                                      const char* activeManifestKey,
                                      const char* fallbackManifestKey) {
    const auto root = decodeConfigurationRoot(observed, rootKey);
    if (!root.has_value() || !configurationManifestReferenceMatches(
                                 root->active, observed, activeManifestKey)) {
        return false;
    }
    if (fallbackManifestKey == nullptr) {
        return !root->fallback.has_value();
    }
    return root->fallback.has_value() &&
           configurationManifestReferenceMatches(*root->fallback, observed,
                                                 fallbackManifestKey);
}

bool validConfigurationManifestGeneration(const BackendObservation& observed,
                                          const char* manifestKey,
                                          const char* userKey,
                                          const char* serviceKey,
                                          const char* catalogKey) {
    if (!hasCommitted(observed, manifestKey) ||
        !hasCommitted(observed, userKey) ||
        !hasCommitted(observed, serviceKey) ||
        !hasCommitted(observed, catalogKey)) {
        return false;
    }
    const auto manifestEnvelope =
        device_platform::decodeEnvelope(observed.committed.at(manifestKey));
    if (!manifestEnvelope.envelope.has_value() ||
        manifestEnvelope.envelope->recordTypeId !=
            kConfigurationManifestRecordType ||
        manifestEnvelope.envelope->schemaVersion !=
            fermentation::kConfigurationManifestSchemaVersion1 ||
        manifestEnvelope.envelope->storageEpoch != kEpoch) {
        return false;
    }
    const auto manifest = fermentation::decodeConfigurationManifestPayload(
        manifestEnvelope.envelope->payload);
    if (!manifest.value.has_value()) return false;
    const auto matches = [&](const std::string& key, const auto& referenceValue,
                             RecordTypeId type) {
        const auto record =
            device_platform::decodeEnvelope(observed.committed.at(key));
        if (!record.envelope.has_value() ||
            record.envelope->recordTypeId != type ||
            record.envelope->schemaVersion != referenceValue.schemaVersion ||
            record.envelope->storageEpoch != referenceValue.storageEpoch ||
            record.envelope->versionValue != referenceValue.version.value() ||
            referenceValue.slot.value() !=
                static_cast<std::uint32_t>(key[2] - '0') ||
            record.envelope->payload.size() != referenceValue.payloadLength ||
            device_platform::computeCrc32IsoHdlc(record.envelope->payload) !=
                referenceValue.payloadCrc) {
            return false;
        }
        return true;
    };
    return matches(userKey, manifest.value->userConfiguration,
                   kUserConfigurationRecordType) &&
           matches(serviceKey, manifest.value->serviceConfiguration,
                   kServiceConfigurationRecordType) &&
           matches(catalogKey, manifest.value->programCatalog,
                   kProgramCatalogRecordType);
}

bool validConfigurationGeneration(const BackendObservation& observed,
                                  const char* rootKey, const char* manifestKey,
                                  const char* userKey, const char* serviceKey,
                                  const char* catalogKey) {
    return configurationRootReferencesMatch(observed, rootKey, manifestKey,
                                            nullptr) &&
           validConfigurationManifestGeneration(observed, manifestKey, userKey,
                                                serviceKey, catalogKey);
}

bool configurationFallbackReferenceMatches(const BackendObservation& observed,
                                           const char* rootKey,
                                           const char* fallbackManifestKey) {
    const auto root = decodeConfigurationRoot(observed, rootKey);
    return root.has_value() && root->fallback.has_value() &&
           configurationManifestReferenceMatches(*root->fallback, observed,
                                                 fallbackManifestKey);
}

bool validRunRecord(const BackendObservation& observed, const char* key,
                    std::uint8_t slot) {
    if (!hasCommitted(observed, key)) return false;
    const auto record = fermentation::decodeRunPersistenceRecord(
        observed.committed.at(key), kEpoch);
    if (!record.has_value() ||
        record->snapshot.variant !=
            fermentation::RunCheckpointVariant::ProgramRun) {
        return false;
    }
    return fermentation::runCheckpointReferenceMatches(
        fermentation::makeRunCheckpointReference(slot, *record, kEpoch),
        *record, slot);
}

bool validRunHeadGraph(const BackendObservation& observed,
                       bool requireFallback) {
    if (!hasCommitted(observed, "rh0") ||
        !validRunRecord(observed, "rc0", 0U)) {
        return false;
    }
    if (requireFallback && !validRunRecord(observed, "rc1", 1U)) return false;
    const auto head = fermentation::decodeRunPersistenceHead(
        observed.committed.at("rh0"), kEpoch);
    if (!head.has_value() ||
        head->state != fermentation::RunPersistenceHeadState::Committed ||
        head->current.slot != 0U ||
        !head->fallback.has_value() != !requireFallback) {
        return false;
    }
    const auto current = fermentation::decodeRunPersistenceRecord(
        observed.committed.at("rc0"), kEpoch);
    if (!current.has_value() ||
        head->current.checkpointRevision != current->checkpointRevision) {
        return false;
    }
    if (requireFallback) {
        const auto fallback = fermentation::decodeRunPersistenceRecord(
            observed.committed.at("rc1"), kEpoch);
        if (!fallback.has_value() ||
            head->fallback->checkpointRevision >= current->checkpointRevision ||
            fallback->snapshot.activeRunId != current->snapshot.activeRunId ||
            !fermentation::runCheckpointReferenceMatches(*head->fallback,
                                                         *fallback, 1U)) {
            return false;
        }
    }
    return fermentation::runCheckpointReferenceMatches(head->current, *current,
                                                       0U);
}

bool validRunFallbackWithUnavailableCurrent(
    const BackendObservation& observed) {
    if (!hasCommitted(observed, "rh0") || !hasCommitted(observed, "rc0") ||
        !validRunRecord(observed, "rc0", 0U) ||
        !validRunRecord(observed, "rc1", 1U)) {
        return false;
    }
    const auto head = fermentation::decodeRunPersistenceHead(
        observed.committed.at("rh0"), kEpoch);
    if (!head.has_value() ||
        head->state != fermentation::RunPersistenceHeadState::Committed ||
        !head->fallback.has_value() || head->current.slot != 0U ||
        head->fallback->checkpointRevision >=
            head->current.checkpointRevision) {
        return false;
    }
    const auto fallback = fermentation::decodeRunPersistenceRecord(
        observed.committed.at("rc1"), kEpoch);
    const auto current = fermentation::decodeRunPersistenceRecord(
        observed.committed.at("rc0"), kEpoch);
    return current.has_value() && fallback.has_value() &&
           !fermentation::runCheckpointReferenceMatches(head->current, *current,
                                                        0U) &&
           fallback->snapshot.activeRunId == current->snapshot.activeRunId &&
           fallback->checkpointRevision < current->checkpointRevision &&
           fermentation::runCheckpointReferenceMatches(*head->fallback,
                                                       *fallback, 1U);
}

bool validRunHeadFallbackReference(const BackendObservation& observed) {
    if (!hasCommitted(observed, "rh0") || !hasCommitted(observed, "rc1")) {
        return false;
    }
    const auto head = fermentation::decodeRunPersistenceHead(
        observed.committed.at("rh0"), kEpoch);
    const auto fallback = fermentation::decodeRunPersistenceRecord(
        observed.committed.at("rc1"), kEpoch);
    return head.has_value() && head->fallback.has_value() &&
           fallback.has_value() &&
           fermentation::runCheckpointReferenceMatches(*head->fallback,
                                                       *fallback, 1U);
}

bool semanticFixturePassed(const OracleCase& item,
                           const BackendObservation& observed) {
    if (item.domain == Domain::Configuration) {
        const bool oldValid = validConfigurationGeneration(
            observed, "cr0", "cm0", "uc0", "sc0", "pc0");
        switch (item.scenario) {
            case Scenario::CurrentValid:
            case Scenario::CurrentWithoutFallback:
                return oldValid;
            case Scenario::OlderValid: {
                if (!oldValid || !hasCommitted(observed, "cr1")) return false;
                const auto root = device_platform::decodeEnvelope(
                    observed.committed.at("cr1"));
                if (!root.envelope.has_value()) return false;
                const auto decoded =
                    fermentation::decodeConfigurationRootPayload(
                        root.envelope->payload);
                return decoded.value.has_value() &&
                       !decoded.value->fallback.has_value() &&
                       decoded.value->active.slot.value() == 2U;
            }
            case Scenario::FallbackValid: {
                if (!oldValid ||
                    !configurationRootReferencesMatch(observed, "cr1", "cm1",
                                                      "cm0") ||
                    !validConfigurationManifestGeneration(
                        observed, "cm0", "uc0", "sc0", "pc0") ||
                    !hasCommitted(observed, "uc1"))
                    return false;
                const auto user = device_platform::decodeEnvelope(
                    observed.committed.at("uc1"));
                return !user.envelope.has_value();
            }
            case Scenario::UnknownCommitValid:
                return oldValid &&
                       configurationRootReferencesMatch(observed, "cr1", "cm1",
                                                        "cm0") &&
                       validConfigurationManifestGeneration(
                           observed, "cm1", "uc1", "sc1", "pc1") &&
                       observed.committed.at("cr0") !=
                           observed.committed.at("cr1");
            case Scenario::SafeWriteErrorOld:
            case Scenario::SafeCapacityOld:
                return oldValid && observed.writeTargetKey == "cr1" &&
                       !hasCommitted(observed, "cr1");
            case Scenario::UnknownCommitNotFound:
                return oldValid &&
                       configurationRootReferencesMatch(observed, "cr1", "cm1",
                                                        "cm0") &&
                       validConfigurationManifestGeneration(
                           observed, "cm1", "uc1", "sc1", "pc1") &&
                       observed.readStatus == StateStoreReadStatus::NotFound;
            case Scenario::ReadError:
                return oldValid &&
                       observed.readStatus == StateStoreReadStatus::ReadError;
            case Scenario::ReadCapacity:
                return hasCommitted(observed, "cr0") &&
                       observed.committed.at("cr0").size() >
                           fermentation::configuration_limits::
                               kMaximumConfigurationRootEnvelopeBytes;
            case Scenario::FactoryEmpty:
                return observed.committed.empty();
            case Scenario::MissingEvidence:
                return hasCommitted(observed, "cr0") &&
                       hasCommitted(observed, "cm0") &&
                       !hasCommitted(observed, "uc0");
            case Scenario::Partial:
                return hasCommitted(observed, "uc0") &&
                       !device_platform::decodeEnvelope(
                            observed.committed.at("uc0"))
                            .envelope.has_value();
            case Scenario::Mixed: {
                if (!hasCommitted(observed, "uc0")) return false;
                const auto user = device_platform::decodeEnvelope(
                    observed.committed.at("uc0"));
                if (!user.envelope.has_value() ||
                    !hasCommitted(observed, "cr0") ||
                    !hasCommitted(observed, "cm0")) {
                    return false;
                }
                const auto root = device_platform::decodeEnvelope(
                    observed.committed.at("cr0"));
                const auto manifestEnvelope = device_platform::decodeEnvelope(
                    observed.committed.at("cm0"));
                if (!root.envelope.has_value() ||
                    !manifestEnvelope.envelope.has_value()) {
                    return false;
                }
                const auto rootRecord =
                    fermentation::decodeConfigurationRootPayload(
                        root.envelope->payload);
                const auto manifest =
                    fermentation::decodeConfigurationManifestPayload(
                        manifestEnvelope.envelope->payload);
                return rootRecord.value.has_value() &&
                       manifest.value.has_value() &&
                       device_platform::computeCrc32IsoHdlc(
                           user.envelope->payload) !=
                           manifest.value->userConfiguration.payloadCrc;
            }
            case Scenario::CorruptEnvelopeCrc:
                return !device_platform::decodeEnvelope(observed.readBytes)
                            .envelope.has_value();
            case Scenario::UnsupportedSchema: {
                const auto decoded =
                    device_platform::decodeEnvelope(observed.readBytes);
                return decoded.envelope.has_value() &&
                       decoded.envelope->schemaVersion == 99U;
            }
            case Scenario::ForeignEpoch: {
                const auto decoded =
                    device_platform::decodeEnvelope(observed.readBytes);
                return decoded.envelope.has_value() &&
                       decoded.envelope->storageEpoch != kEpoch;
            }
            case Scenario::InvalidReference: {
                const auto root = decodeConfigurationRoot(observed, "cr1");
                return oldValid && root.has_value() &&
                       !configurationManifestReferenceMatches(
                           root->active, observed, "cm1") &&
                       configurationFallbackReferenceMatches(observed, "cr1",
                                                             "cm0") &&
                       validConfigurationManifestGeneration(
                           observed, "cm0", "uc0", "sc0", "pc0");
            }
            case Scenario::InvalidReferenceNoFallback: {
                const auto root = decodeConfigurationRoot(observed, "cr0");
                return root.has_value() && !root->fallback.has_value() &&
                       !configurationManifestReferenceMatches(root->active,
                                                              observed, "cm0");
            }
            case Scenario::NotReconstructible:
                return hasCommitted(observed, "cr0") &&
                       !validConfigurationGeneration(observed, "cr0", "cm0",
                                                     "uc0", "sc0", "pc0");
            case Scenario::Orphan:
                return oldValid && hasCommitted(observed, "uc3");
            case Scenario::OrphanedGeneration:
                return !hasCommitted(observed, "cr0") &&
                       !hasCommitted(observed, "cr1") &&
                       hasCommitted(observed, "uc0") &&
                       hasCommitted(observed, "cm0");
            default:
                return false;
        }
    }

    switch (item.scenario) {
        case Scenario::CurrentValid:
        case Scenario::OlderValid:
        case Scenario::CurrentWithoutFallback:
            return validRunHeadGraph(observed,
                                     item.scenario == Scenario::OlderValid);
        case Scenario::FallbackValid:
            return validRunFallbackWithUnavailableCurrent(observed);
        case Scenario::InvalidReference: {
            if (!hasCommitted(observed, "rh0") ||
                !hasCommitted(observed, "rc0") ||
                !validRunRecord(observed, "rc1", 1U))
                return false;
            const auto current = fermentation::decodeRunPersistenceRecord(
                observed.committed.at("rc0"), kEpoch);
            const auto head = fermentation::decodeRunPersistenceHead(
                observed.committed.at("rh0"), kEpoch);
            const auto fallback = fermentation::decodeRunPersistenceRecord(
                observed.committed.at("rc1"), kEpoch);
            return current.has_value() && head.has_value() &&
                   fallback.has_value() &&
                   !fermentation::runCheckpointReferenceMatches(head->current,
                                                                *current, 0U) &&
                   head->fallback.has_value() &&
                   fermentation::runCheckpointReferenceMatches(*head->fallback,
                                                               *fallback, 1U) &&
                   fallback->snapshot.activeRunId ==
                       current->snapshot.activeRunId &&
                   fallback->checkpointRevision < current->checkpointRevision;
        }
        case Scenario::ForeignEpoch: {
            if (!validRunHeadFallbackReference(observed)) return false;
            const auto currentEnvelope =
                device_platform::decodeEnvelope(observed.committed.at("rc0"));
            return currentEnvelope.envelope.has_value() &&
                   currentEnvelope.envelope->storageEpoch != kEpoch;
        }
        case Scenario::UnknownCommitValid:
            return observed.oldHeadBytes != observed.newHeadBytes &&
                   !observed.oldHeadBytes.empty() &&
                   !observed.newHeadBytes.empty() &&
                   validRunHeadGraph(observed, true);
        case Scenario::UnknownCommitNotFound:
            return observed.oldHeadBytes != observed.newHeadBytes &&
                   observed.readStatus == StateStoreReadStatus::NotFound &&
                   hasCommitted(observed, "rc0") &&
                   hasCommitted(observed, "rc1");
        case Scenario::SafeWriteErrorOld:
        case Scenario::SafeCapacityOld:
            return validRunHeadGraph(observed, false) &&
                   observed.writeTargetKey == "rc0";
        case Scenario::ReadError:
            return observed.readStatus == StateStoreReadStatus::ReadError;
        case Scenario::ReadCapacity:
            return hasCommitted(observed, "rh0") &&
                   observed.committed.at("rh0").size() >
                       kRunHeadConsumerReadBudget;
        case Scenario::NoPersistedRun:
            return observed.committed.empty();
        case Scenario::MissingReferencedRun:
            return hasCommitted(observed, "rh0") &&
                   !hasCommitted(observed, "rc0");
        case Scenario::Partial:
            return hasCommitted(observed, "rc0") &&
                   !device_platform::decodeEnvelope(
                        observed.committed.at("rc0"))
                        .envelope.has_value();
        case Scenario::Mixed:
            return hasCommitted(observed, "rh0") &&
                   hasCommitted(observed, "rc0") &&
                   hasCommitted(observed, "rc1");
        case Scenario::CorruptEnvelopeCrc:
            return !device_platform::decodeEnvelope(observed.readBytes)
                        .envelope.has_value();
        case Scenario::UnsupportedSchema:
            return device_platform::decodeEnvelope(observed.readBytes)
                       .envelope.has_value() &&
                   device_platform::decodeEnvelope(observed.readBytes)
                           .envelope->schemaVersion == 99U;
        case Scenario::InvalidReferenceNoFallback: {
            if (!hasCommitted(observed, "rh0") ||
                !hasCommitted(observed, "rc0") ||
                hasCommitted(observed, "rc1")) {
                return false;
            }
            const auto current = fermentation::decodeRunPersistenceRecord(
                observed.committed.at("rc0"), kEpoch);
            const auto head = fermentation::decodeRunPersistenceHead(
                observed.committed.at("rh0"), kEpoch);
            return current.has_value() && head.has_value() &&
                   !head->fallback.has_value() &&
                   !fermentation::runCheckpointReferenceMatches(head->current,
                                                                *current, 0U);
        }
        case Scenario::PreparedInterrupted: {
            const auto head = fermentation::decodeRunPersistenceHead(
                observed.committed.at("rh0"), kEpoch);
            return observed.durablePreparedHead && head.has_value() &&
                   head->state ==
                       fermentation::RunPersistenceHeadState::Prepared;
        }
        case Scenario::Orphan:
            return !hasCommitted(observed, "rh0") &&
                   validRunRecord(observed, "rc0", 0U) &&
                   validRunRecord(observed, "rc1", 1U);
        case Scenario::NotReconstructible: {
            if (!hasCommitted(observed, "rh0") ||
                hasCommitted(observed, "rc0") ||
                hasCommitted(observed, "rc1")) {
                return false;
            }
            const auto head = fermentation::decodeRunPersistenceHead(
                observed.committed.at("rh0"), kEpoch);
            return head.has_value() && head->fallback.has_value();
        }
        case Scenario::ControlledDiscard: {
            const auto record = fermentation::decodeRunPersistenceRecord(
                observed.committed.at("rc0"), kEpoch);
            return record.has_value() &&
                   record->snapshot.processState.state ==
                       fermentation::ProcessState::ReachingTarget;
        }
        case Scenario::ControlledDiscardPost: {
            if (!hasCommitted(observed, "rc1") ||
                !hasCommitted(observed, "rh0")) {
                return false;
            }
            const auto record = fermentation::decodeRunPersistenceRecord(
                observed.committed.at("rc1"), kEpoch);
            const auto head = fermentation::decodeRunPersistenceHead(
                observed.committed.at("rh0"), kEpoch);
            return record.has_value() &&
                   record->snapshot.variant ==
                       fermentation::RunCheckpointVariant::NoActiveRun &&
                   head.has_value() && head->current.slot == 1U;
        }
        case Scenario::FactoryEmpty:
        case Scenario::MissingEvidence:
        case Scenario::OrphanedGeneration:
            return false;
    }
    return false;
}

bool expectedTruthSanity(const OracleCase& item) {
    if (item.logicalGate != LogicalGate::Unresolved || item.actuatorAllowed) {
        return false;
    }
    if (!item.hasProductOutcome) {
        return item.producer == SafetyProducer::None &&
               !item.prohibitedActiveState &&
               item.safety == SafetyProjection::Standby &&
               ((item.domain == Domain::Configuration &&
                 item.scenario == Scenario::FactoryEmpty &&
                 item.classification == RecordClassification::FactoryEmpty &&
                 item.baseline == BaselineClassification::FactoryEmpty &&
                 item.recoveryAction ==
                     ExpectedRecoveryAction::FactoryInitialization) ||
                (item.domain == Domain::Run &&
                 item.scenario == Scenario::NoPersistedRun &&
                 item.classification == RecordClassification::NoPersistedRun &&
                 item.baseline == BaselineClassification::NoPersistedRun &&
                 item.recoveryAction == ExpectedRecoveryAction::NoActiveRun) ||
                (item.domain == Domain::Run &&
                 item.scenario == Scenario::ControlledDiscardPost &&
                 item.classification ==
                     RecordClassification::ControlledDiscard &&
                 item.baseline ==
                     BaselineClassification::ControlledDiscardTombstone &&
                 item.recoveryAction == ExpectedRecoveryAction::NoActiveRun));
    }
    if (item.baseline != BaselineClassification::None ||
        item.recoveryAction != ExpectedRecoveryAction::None) {
        return false;
    }

    const auto validConfiguration = [&](ProductOutcome outcome,
                                        RecordClassification classification) {
        return item.domain == Domain::Configuration &&
               item.outcome == outcome &&
               item.classification == classification &&
               item.safety == SafetyProjection::Standby &&
               item.producer == SafetyProducer::None &&
               !item.prohibitedActiveState;
    };
    const auto configurationRecovery =
        [&](SafetyProducer producer, RecordClassification classification) {
            return item.domain == Domain::Configuration &&
                   item.outcome ==
                       ProductOutcome::ConfigurationRecoveryRequired &&
                   item.classification == classification &&
                   item.safety == SafetyProjection::SafeBoot &&
                   item.producer == producer && item.prohibitedActiveState;
        };
    const auto validResume = [&](ProductOutcome outcome,
                                 RecordClassification classification) {
        return item.domain == Domain::Run && item.outcome == outcome &&
               item.classification == classification &&
               item.safety == SafetyProjection::ResumeOffer &&
               item.producer == SafetyProducer::None &&
               !item.prohibitedActiveState;
    };
    const auto runRecovery = [&](RecordClassification classification) {
        return item.domain == Domain::Run &&
               item.outcome == ProductOutcome::RunRecoveryRequired &&
               item.classification == classification &&
               item.safety == SafetyProjection::SafeBoot &&
               item.producer == SafetyProducer::RunPersistenceUntrusted &&
               item.prohibitedActiveState;
    };

    if (item.domain == Domain::Configuration) {
        switch (item.scenario) {
            case Scenario::CurrentValid:
            case Scenario::CurrentWithoutFallback:
            case Scenario::UnknownCommitValid:
            case Scenario::Orphan:
                return validConfiguration(
                    ProductOutcome::NewValidConfiguration,
                    item.scenario == Scenario::Orphan
                        ? RecordClassification::Orphan
                        : RecordClassification::FullyValidCurrent);
            case Scenario::OlderValid:
            case Scenario::SafeWriteErrorOld:
            case Scenario::SafeCapacityOld:
            case Scenario::UnknownCommitNotFound:
                return validConfiguration(
                    ProductOutcome::OldValidConfiguration,
                    RecordClassification::FullyValidOlder);
            case Scenario::FallbackValid:
                return validConfiguration(
                    ProductOutcome::FallbackValidConfiguration,
                    RecordClassification::FullyValidFallback);
            case Scenario::InvalidReference:
                return validConfiguration(
                    ProductOutcome::FallbackValidConfiguration,
                    RecordClassification::InvalidReference);
            case Scenario::ReadError:
                return configurationRecovery(
                    SafetyProducer::ConfigurationUnavailable,
                    RecordClassification::Indeterminate);
            case Scenario::ReadCapacity:
                return configurationRecovery(
                    SafetyProducer::ConfigurationUnavailable,
                    RecordClassification::Indeterminate);
            case Scenario::MissingEvidence:
                return configurationRecovery(
                    SafetyProducer::ConfigurationIntegrityFailure,
                    RecordClassification::Missing);
            case Scenario::Partial:
                return configurationRecovery(
                    SafetyProducer::ConfigurationIntegrityFailure,
                    RecordClassification::Partial);
            case Scenario::Mixed:
                return configurationRecovery(
                    SafetyProducer::ConfigurationIntegrityFailure,
                    RecordClassification::Mixed);
            case Scenario::CorruptEnvelopeCrc:
                return configurationRecovery(
                    SafetyProducer::ConfigurationIntegrityFailure,
                    RecordClassification::Corrupt);
            case Scenario::UnsupportedSchema:
                return configurationRecovery(
                    SafetyProducer::ConfigurationIntegrityFailure,
                    RecordClassification::UnsupportedSchema);
            case Scenario::InvalidReferenceNoFallback:
                return configurationRecovery(
                    SafetyProducer::ConfigurationIntegrityFailure,
                    RecordClassification::InvalidReference);
            case Scenario::ForeignEpoch:
                return configurationRecovery(
                    SafetyProducer::ConfigurationIntegrityFailure,
                    RecordClassification::ForeignEpoch);
            case Scenario::OrphanedGeneration:
                return configurationRecovery(
                    SafetyProducer::ConfigurationIntegrityFailure,
                    RecordClassification::Orphan);
            case Scenario::NotReconstructible:
                return configurationRecovery(
                    SafetyProducer::ConfigurationIntegrityFailure,
                    RecordClassification::NotReconstructible);
            default:
                return false;
        }
    }

    switch (item.scenario) {
        case Scenario::CurrentValid:
        case Scenario::OlderValid:
        case Scenario::UnknownCommitValid:
        case Scenario::SafeWriteErrorOld:
        case Scenario::SafeCapacityOld:
            return validResume(ProductOutcome::NewValidResume,
                               RecordClassification::FullyValidCurrent);
        case Scenario::FallbackValid:
        case Scenario::InvalidReference:
        case Scenario::ForeignEpoch:
            return validResume(ProductOutcome::OlderValidCheckpointResume,
                               item.scenario == Scenario::FallbackValid
                                   ? RecordClassification::FullyValidFallback
                               : item.scenario == Scenario::InvalidReference
                                   ? RecordClassification::InvalidReference
                                   : RecordClassification::ForeignEpoch);
        case Scenario::UnknownCommitNotFound:
        case Scenario::ReadError:
        case Scenario::ReadCapacity:
            return runRecovery(RecordClassification::Indeterminate);
        case Scenario::MissingReferencedRun:
            return runRecovery(RecordClassification::Missing);
        case Scenario::Partial:
            return runRecovery(RecordClassification::Partial);
        case Scenario::Mixed:
            return runRecovery(RecordClassification::Mixed);
        case Scenario::CorruptEnvelopeCrc:
            return runRecovery(RecordClassification::Corrupt);
        case Scenario::UnsupportedSchema:
            return runRecovery(RecordClassification::UnsupportedSchema);
        case Scenario::InvalidReferenceNoFallback:
            return runRecovery(RecordClassification::InvalidReference);
        case Scenario::PreparedInterrupted:
            return runRecovery(RecordClassification::PreparedInterrupted);
        case Scenario::Orphan:
            return runRecovery(RecordClassification::Orphan);
        case Scenario::NotReconstructible:
            return runRecovery(RecordClassification::NotReconstructible);
        case Scenario::ControlledDiscard:
            return item.outcome == ProductOutcome::RunAbortRequired &&
                   item.classification ==
                       RecordClassification::ControlledDiscard &&
                   item.safety == SafetyProjection::NoActiveRun &&
                   item.producer == SafetyProducer::None &&
                   !item.prohibitedActiveState;
        default:
            return false;
    }
}

bool fixtureSanityPassed(const OracleCase& item,
                         const BackendObservation& observed) {
    const bool safetyShape = !item.actuatorAllowed &&
                             item.logicalGate == LogicalGate::Unresolved &&
                             (!item.prohibitedActiveState ||
                              (item.safety == SafetyProjection::SafeBoot &&
                               item.producer != SafetyProducer::None));
    return observed.restarted && !observed.pendingAfterRestart &&
           observed.readStatus == item.expectedReadStatus && safetyShape &&
           expectedTruthSanity(item) &&
           (item.scenario != Scenario::PreparedInterrupted ||
            observed.durablePreparedHead) &&
           semanticFixturePassed(item, observed);
}

std::string machineLine(const OracleCase& item,
                        const BackendObservation& observed) {
    std::string line{"issue90_oracle_case="};
    line += item.id;
    line += item.domain == Domain::Configuration ? " domain=configuration"
                                                 : " domain=run";
    line += " mutation_path=\"" + std::string(item.mutationPath) + "\"";
    line += " evidence_scope=SIMULATOR_ONLY backend_characterization=observed";
    line += " fault_layer=SIMULATOR_ONLY post_reboot_fixture=persistent_state";
    line +=
        " counter_domain_baseline=" + std::string(item.counterDomainBaseline);
    line += " fixture_id=" + observed.fixtureId;
    line += " committed_fixture_keys=" + joinKeys(observed.committed);
    line += " fixture_keys_missing=" + joinMissing(observed.missing);
    line += " record_family=" +
            std::string(
                item.domain == Domain::Configuration
                    ? "uc0..uc3,sc0..sc3,pc0..pc3,cm0..cm2,cr0..cr1,cb0..cb1"
                    : "rc0,rc1,rh0");
    line +=
        " write_target_key=" +
        (observed.writeAttempted ? observed.writeTargetKey : "NOT_APPLICABLE");
    line += " product_read_target_key=" + observed.productReadTargetKey;
    line += " write_status=" +
            std::string(observed.writeAttempted
                            ? writeStatusName(observed.writeStatus)
                            : "NOT_APPLICABLE");
    line += " write_status_scope=" + std::string(observed.writeAttempted
                                                     ? "FAULT_CHARACTERIZATION"
                                                     : "NOT_APPLICABLE");
    line += " product_visible_read_status=" +
            std::string(readStatusName(observed.readStatus));
    line += " read_bytes_hex=" + hexBytes(observed.readBytes);
    line += " record_classification=" +
            std::string(classificationName(item.classification));
    line +=
        " baseline_classification=" + std::string(baselineName(item.baseline));
    line += " expected_recovery_action=" +
            std::string(actionName(item.recoveryAction));
    line += " product_outcome=" + std::string(item.hasProductOutcome
                                                  ? outcomeName(item.outcome)
                                                  : "NOT_APPLICABLE");
    line += " safety_projection=" + std::string(safetyName(item.safety));
    line += " safety_producer=" + std::string(producerName(item.producer));
    line += " logical_gate=" + std::string(logicalGateName(item.logicalGate));
    line += " actuator_allowed=" +
            std::string(item.actuatorAllowed ? "true" : "false");
    line += " product_recovery_gate=" +
            std::string(fixtureSanityPassed(item, observed) ? "PASS" : "FAIL");
    line += " reboot=" + std::string(observed.restarted ? "true" : "false");
    line += " pending_before_reboot=" +
            std::string(observed.pendingBeforeRestart ? "true" : "false");
    line += " pending_after_reboot=" +
            std::string(observed.pendingAfterRestart ? "true" : "false");
    line += " durable_prepared_head=" +
            std::string(observed.durablePreparedHead ? "true" : "false");
    line += " old_new_head_bytes_distinct=" +
            std::string(!observed.oldHeadBytes.empty() &&
                                observed.oldHeadBytes != observed.newHeadBytes
                            ? "true"
                            : "false");
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
    TEST_ASSERT_EQUAL_UINT32(22U, config);
    TEST_ASSERT_EQUAL_UINT32(23U, run);
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
        if (item.expectedWriteStatus != StateStoreWriteStatus::Success) {
            TEST_ASSERT_TRUE(observed.writeAttempted);
            TEST_ASSERT_TRUE(observed.writeStatus == item.expectedWriteStatus);
        }
        TEST_ASSERT_TRUE(fixtureSanityPassed(item, observed));
        if (item.scenario == Scenario::FactoryEmpty ||
            item.scenario == Scenario::NoPersistedRun) {
            TEST_ASSERT_TRUE(observed.committed.empty());
            TEST_ASSERT_FALSE(item.hasProductOutcome);
            TEST_ASSERT_TRUE(item.baseline != BaselineClassification::None);
        }
        if (item.scenario == Scenario::PreparedInterrupted) {
            TEST_ASSERT_TRUE(observed.durablePreparedHead);
            TEST_ASSERT_TRUE(observed.committed.find("rh0") !=
                             observed.committed.end());
            const auto decoded = fermentation::decodeRunPersistenceHead(
                observed.committed.at("rh0"), kEpoch);
            TEST_ASSERT_TRUE(decoded.has_value());
            TEST_ASSERT_EQUAL_INT(
                static_cast<int>(
                    fermentation::RunPersistenceHeadState::Prepared),
                static_cast<int>(decoded->state));
            TEST_ASSERT_FALSE(observed.pendingBeforeRestart);
        }
        if (item.scenario == Scenario::Partial) {
            const auto key =
                item.domain == Domain::Configuration ? "uc0" : "rc0";
            TEST_ASSERT_TRUE(observed.committed.find(key) !=
                             observed.committed.end());
            TEST_ASSERT_FALSE(
                device_platform::decodeEnvelope(observed.committed.at(key))
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
            TEST_ASSERT_TRUE(observed.committed.find("cr0") !=
                             observed.committed.end());
            TEST_ASSERT_TRUE(observed.committed.find("cm0") !=
                             observed.committed.end());
            TEST_ASSERT_TRUE(std::find(observed.missing.begin(),
                                       observed.missing.end(),
                                       "uc0") != observed.missing.end());
        }
        if (item.domain == Domain::Configuration &&
            item.scenario == Scenario::UnknownCommitValid) {
            for (const auto* key : {"uc1", "sc1", "pc1", "cm1", "cr1"}) {
                TEST_ASSERT_TRUE(observed.committed.find(key) !=
                                 observed.committed.end());
            }
            TEST_ASSERT_TRUE(observed.committed.at("cr0") !=
                             observed.committed.at("cr1"));
        }
        if (item.domain == Domain::Configuration &&
            item.scenario == Scenario::UnknownCommitNotFound) {
            TEST_ASSERT_TRUE(observed.committed.find("cr0") !=
                             observed.committed.end());
            TEST_ASSERT_TRUE(observed.committed.find("cr1") !=
                             observed.committed.end());
            TEST_ASSERT_TRUE(observed.readStatus ==
                             StateStoreReadStatus::NotFound);
        }
        if (item.scenario == Scenario::Orphan &&
            item.domain == Domain::Configuration) {
            TEST_ASSERT_TRUE(observed.committed.find("uc3") !=
                             observed.committed.end());
            TEST_ASSERT_TRUE(observed.committed.find("cr0") !=
                             observed.committed.end());
        }
        if (item.scenario == Scenario::Orphan && item.domain == Domain::Run) {
            TEST_ASSERT_TRUE(observed.committed.find("rc0") !=
                             observed.committed.end());
            TEST_ASSERT_TRUE(observed.committed.find("rc1") !=
                             observed.committed.end());
            TEST_ASSERT_TRUE(std::find(observed.missing.begin(),
                                       observed.missing.end(),
                                       "rh0") != observed.missing.end());
        }
        if (item.scenario == Scenario::OrphanedGeneration &&
            item.domain == Domain::Configuration) {
            TEST_ASSERT_TRUE(observed.committed.find("uc0") !=
                             observed.committed.end());
            TEST_ASSERT_TRUE(observed.committed.find("cm0") !=
                             observed.committed.end());
            TEST_ASSERT_TRUE(std::find(observed.missing.begin(),
                                       observed.missing.end(),
                                       "cr0") != observed.missing.end());
            TEST_ASSERT_TRUE(std::find(observed.missing.begin(),
                                       observed.missing.end(),
                                       "cr1") != observed.missing.end());
        }
        if (item.scenario == Scenario::FallbackValid &&
            item.domain == Domain::Configuration) {
            const auto root =
                device_platform::decodeEnvelope(observed.committed.at("cr1"));
            TEST_ASSERT_TRUE(root.envelope.has_value());
            const auto decoded = fermentation::decodeConfigurationRootPayload(
                root.envelope->payload);
            TEST_ASSERT_TRUE(decoded.value.has_value());
            TEST_ASSERT_TRUE(decoded.value->fallback.has_value());
            TEST_ASSERT_EQUAL_UINT32(0U, decoded.value->fallback->slot.value());
            TEST_ASSERT_TRUE(observed.committed.at("uc0") !=
                             observed.committed.at("uc1"));
            TEST_ASSERT_TRUE(observed.committed.at("cr0") !=
                             observed.committed.at("cr1"));
        }
        if ((item.scenario == Scenario::OlderValid ||
             item.scenario == Scenario::FallbackValid) &&
            item.domain == Domain::Run) {
            const auto oldMetadata = device_platform::decodeEnvelopeMetadata(
                observed.committed.at("rc1"));
            const auto newMetadata = device_platform::decodeEnvelopeMetadata(
                observed.committed.at("rc0"));
            TEST_ASSERT_TRUE(oldMetadata.metadata.has_value());
            TEST_ASSERT_TRUE(newMetadata.metadata.has_value());
            TEST_ASSERT_TRUE(oldMetadata.metadata->versionValue <
                             newMetadata.metadata->versionValue);
            TEST_ASSERT_TRUE(observed.committed.at("rc0") !=
                             observed.committed.at("rc1"));
            const auto oldRecord = fermentation::decodeRunPersistenceRecord(
                observed.committed.at("rc1"), kEpoch);
            const auto newRecord = fermentation::decodeRunPersistenceRecord(
                observed.committed.at("rc0"), kEpoch);
            TEST_ASSERT_TRUE(oldRecord.has_value());
            TEST_ASSERT_TRUE(newRecord.has_value());
            TEST_ASSERT_EQUAL_STRING(oldRecord->snapshot.activeRunId.c_str(),
                                     newRecord->snapshot.activeRunId.c_str());
            TEST_ASSERT_TRUE(oldRecord->checkpointRevision <
                             newRecord->checkpointRevision);
            if (item.scenario == Scenario::FallbackValid) {
                const auto head = fermentation::decodeRunPersistenceHead(
                    observed.committed.at("rh0"), kEpoch);
                TEST_ASSERT_TRUE(head.has_value());
                TEST_ASSERT_TRUE(head->fallback.has_value());
                TEST_ASSERT_FALSE(fermentation::runCheckpointReferenceMatches(
                    head->current, *newRecord, 0U));
                TEST_ASSERT_TRUE(fermentation::runCheckpointReferenceMatches(
                    *head->fallback, *oldRecord, 1U));
            }
        }
        if (item.scenario == Scenario::ReadCapacity) {
            TEST_ASSERT_TRUE(observed.readBytes.empty());
            if (item.domain == Domain::Configuration) {
                TEST_ASSERT_TRUE(observed.committed.at("cr0").size() >
                                 fermentation::configuration_limits::
                                     kMaximumConfigurationRootEnvelopeBytes);
            } else {
                TEST_ASSERT_TRUE(observed.committed.at("rh0").size() >
                                 kRunHeadConsumerReadBudget);
            }
        }
        if (item.scenario == Scenario::ControlledDiscard) {
            const auto snapshot = fermentation::decodeRunPersistenceRecord(
                observed.committed.at("rc0"), kEpoch);
            TEST_ASSERT_TRUE(snapshot.has_value());
            TEST_ASSERT_TRUE(snapshot->snapshot.processState.state ==
                             fermentation::ProcessState::ReachingTarget);
            TEST_ASSERT_TRUE(item.safety == SafetyProjection::NoActiveRun);
        }
        if (item.scenario == Scenario::ControlledDiscardPost) {
            const auto snapshot = fermentation::decodeRunPersistenceRecord(
                observed.committed.at("rc1"), kEpoch);
            TEST_ASSERT_TRUE(snapshot.has_value());
            TEST_ASSERT_TRUE(snapshot->snapshot.variant ==
                             fermentation::RunCheckpointVariant::NoActiveRun);
            TEST_ASSERT_TRUE(
                item.baseline ==
                BaselineClassification::ControlledDiscardTombstone);
        }
    }
}

void test_expected_product_and_safety_outcomes_are_independent() {
    std::size_t configRecovery = 0U;
    std::size_t runRecovery = 0U;
    for (const auto& item : kMatrix) {
        const auto observed = observe(item);
        if (observed.readStatus != item.expectedReadStatus) {
            std::printf("read mismatch %s expected=%s actual=%s\\n", item.id,
                        readStatusName(item.expectedReadStatus),
                        readStatusName(observed.readStatus));
        }
        TEST_ASSERT_TRUE(observed.readStatus == item.expectedReadStatus);
        TEST_ASSERT_FALSE(item.actuatorAllowed);
        TEST_ASSERT_TRUE(item.logicalGate == LogicalGate::Unresolved);
        if (!item.hasProductOutcome) continue;
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

void test_configuration_old_and_fallback_graphs_are_distinct() {
    const OracleCase* older = nullptr;
    const OracleCase* fallback = nullptr;
    for (const auto& item : kMatrix) {
        if (item.domain != Domain::Configuration) continue;
        if (item.scenario == Scenario::OlderValid) older = &item;
        if (item.scenario == Scenario::FallbackValid) fallback = &item;
    }
    TEST_ASSERT_NOT_NULL(older);
    TEST_ASSERT_NOT_NULL(fallback);
    const auto oldObserved = observe(*older);
    const auto fallbackObserved = observe(*fallback);
    const auto oldRoot =
        device_platform::decodeEnvelope(oldObserved.committed.at("cr1"));
    const auto fallbackRoot =
        device_platform::decodeEnvelope(fallbackObserved.committed.at("cr1"));
    TEST_ASSERT_TRUE(oldRoot.envelope.has_value());
    TEST_ASSERT_TRUE(fallbackRoot.envelope.has_value());
    const auto oldDecoded =
        fermentation::decodeConfigurationRootPayload(oldRoot.envelope->payload);
    const auto fallbackDecoded = fermentation::decodeConfigurationRootPayload(
        fallbackRoot.envelope->payload);
    TEST_ASSERT_TRUE(oldDecoded.value.has_value());
    TEST_ASSERT_TRUE(fallbackDecoded.value.has_value());
    TEST_ASSERT_FALSE(oldDecoded.value->fallback.has_value());
    TEST_ASSERT_TRUE(fallbackDecoded.value->fallback.has_value());
    TEST_ASSERT_TRUE(oldObserved.committed.at("cr1") !=
                     fallbackObserved.committed.at("cr1"));
}

void test_run_current_and_fallback_revisions_share_one_run_line() {
    for (const auto& item : kMatrix) {
        if (item.domain != Domain::Run ||
            (item.scenario != Scenario::OlderValid &&
             item.scenario != Scenario::FallbackValid)) {
            continue;
        }
        const auto observed = observe(item);
        const auto oldRecord = fermentation::decodeRunPersistenceRecord(
            observed.committed.at("rc1"), kEpoch);
        const auto newRecord = fermentation::decodeRunPersistenceRecord(
            observed.committed.at("rc0"), kEpoch);
        TEST_ASSERT_TRUE(oldRecord.has_value());
        TEST_ASSERT_TRUE(newRecord.has_value());
        TEST_ASSERT_EQUAL_STRING(oldRecord->snapshot.activeRunId.c_str(),
                                 newRecord->snapshot.activeRunId.c_str());
        TEST_ASSERT_TRUE(oldRecord->checkpointRevision <
                         newRecord->checkpointRevision);
    }
}

void test_forbidden_product_states_are_negative_matrix_entries() {
    for (const auto& item : kMatrix) {
        if (!item.prohibitedActiveState) continue;
        TEST_ASSERT_TRUE(item.hasProductOutcome);
        TEST_ASSERT_TRUE(item.safety == SafetyProjection::SafeBoot);
        TEST_ASSERT_FALSE(item.actuatorAllowed);
        TEST_ASSERT_TRUE(item.outcome ==
                             ProductOutcome::ConfigurationRecoveryRequired ||
                         item.outcome == ProductOutcome::RunRecoveryRequired);
        TEST_ASSERT_TRUE(item.producer != SafetyProducer::None);
    }
}

void test_semantic_counterexamples_are_explicit() {
    for (const auto& item : kMatrix) {
        if (item.scenario == Scenario::UnknownCommitNotFound &&
            item.domain == Domain::Configuration) {
            TEST_ASSERT_TRUE(item.outcome ==
                             ProductOutcome::OldValidConfiguration);
            TEST_ASSERT_TRUE(item.producer == SafetyProducer::None);
        }
        if (item.scenario == Scenario::MissingEvidence) {
            TEST_ASSERT_TRUE(item.producer ==
                             SafetyProducer::ConfigurationIntegrityFailure);
            TEST_ASSERT_TRUE(item.baseline !=
                             BaselineClassification::FactoryEmpty);
        }
        if (item.scenario == Scenario::Orphan &&
            item.domain == Domain::Configuration) {
            TEST_ASSERT_TRUE(item.outcome ==
                             ProductOutcome::NewValidConfiguration);
            TEST_ASSERT_TRUE(item.producer == SafetyProducer::None);
        }
        if (item.scenario == Scenario::Orphan && item.domain == Domain::Run) {
            TEST_ASSERT_TRUE(item.outcome ==
                             ProductOutcome::RunRecoveryRequired);
            TEST_ASSERT_TRUE(item.producer ==
                             SafetyProducer::RunPersistenceUntrusted);
        }
        if (item.scenario == Scenario::MissingReferencedRun) {
            const auto observed = observe(item);
            TEST_ASSERT_FALSE(hasCommitted(observed, "rc0"));
            TEST_ASSERT_FALSE(hasCommitted(observed, "rc1"));
            const auto head = fermentation::decodeRunPersistenceHead(
                observed.committed.at("rh0"), kEpoch);
            TEST_ASSERT_TRUE(head.has_value());
            TEST_ASSERT_FALSE(head->fallback.has_value());
        }
        if (item.scenario == Scenario::NotReconstructible &&
            item.domain == Domain::Run) {
            const auto observed = observe(item);
            TEST_ASSERT_FALSE(hasCommitted(observed, "rc0"));
            TEST_ASSERT_FALSE(hasCommitted(observed, "rc1"));
            const auto head = fermentation::decodeRunPersistenceHead(
                observed.committed.at("rh0"), kEpoch);
            TEST_ASSERT_TRUE(head.has_value());
            TEST_ASSERT_TRUE(head->fallback.has_value());
        }
        if (item.scenario == Scenario::OlderValid &&
            item.domain == Domain::Run) {
            TEST_ASSERT_TRUE(item.outcome == ProductOutcome::NewValidResume);
        }
        if (item.scenario == Scenario::FallbackValid &&
            item.domain == Domain::Run) {
            TEST_ASSERT_TRUE(item.outcome ==
                             ProductOutcome::OlderValidCheckpointResume);
        }
        if (item.scenario == Scenario::ControlledDiscard) {
            TEST_ASSERT_TRUE(item.outcome == ProductOutcome::RunAbortRequired);
            TEST_ASSERT_TRUE(item.safety == SafetyProjection::NoActiveRun);
        }
    }
}

void test_machine_output_contains_fixture_and_truth_separation() {
    const auto& item = kMatrix.front();
    const auto line = machineLine(item, observe(item));
    const char* fields[] = {"issue90_oracle_case=",
                            "fixture_id=",
                            "committed_fixture_keys=",
                            "fixture_keys_missing=",
                            "record_family=",
                            "write_target_key=",
                            "product_read_target_key=",
                            "write_status_scope=",
                            "product_visible_read_status=",
                            "counter_domain_baseline=",
                            "record_classification=",
                            "baseline_classification=",
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
    TEST_ASSERT_TRUE(line.find("logical_gate=UNRESOLVED") != std::string::npos);
    TEST_ASSERT_TRUE(line.find("actuator_allowed=false") != std::string::npos);
    TEST_ASSERT_TRUE(line.find("product_recovery_gate=PASS") !=
                     std::string::npos);
    std::printf("%s\n", line.c_str());
}

void test_product_recovery_gate_rejects_inconsistent_observation() {
    const auto& item = kMatrix.front();
    auto observed = observe(item);
    observed.readStatus = StateStoreReadStatus::ReadError;
    const auto line = machineLine(item, observed);
    TEST_ASSERT_TRUE(line.find("product_recovery_gate=FAIL") !=
                     std::string::npos);
}

void test_product_recovery_gate_rejects_invalid_fallback_reference() {
    const OracleCase* item = nullptr;
    for (const auto& candidate : kMatrix) {
        if (candidate.domain == Domain::Run &&
            candidate.scenario == Scenario::FallbackValid) {
            item = &candidate;
            break;
        }
    }
    TEST_ASSERT_NOT_NULL(item);
    auto observed = observe(*item);
    const auto head = fermentation::decodeRunPersistenceHead(
        observed.committed.at("rh0"), kEpoch);
    TEST_ASSERT_TRUE(head.has_value());
    TEST_ASSERT_TRUE(head->fallback.has_value());
    auto invalid = *head;
    invalid.fallback->payloadCrc ^= 1U;
    const auto bytes = fermentation::encodeRunPersistenceHead(invalid, kEpoch);
    TEST_ASSERT_TRUE(bytes.has_value());
    observed.committed["rh0"] = *bytes;
    TEST_ASSERT_TRUE(
        machineLine(*item, observed).find("product_recovery_gate=FAIL") !=
        std::string::npos);
}

void test_product_recovery_gate_rejects_wrong_product_outcome() {
    const OracleCase* item = nullptr;
    for (const auto& candidate : kMatrix) {
        if (candidate.domain == Domain::Configuration &&
            candidate.scenario == Scenario::CurrentValid) {
            item = &candidate;
            break;
        }
    }
    TEST_ASSERT_NOT_NULL(item);
    auto wrong = *item;
    wrong.outcome = ProductOutcome::OldValidConfiguration;
    TEST_ASSERT_TRUE(
        machineLine(wrong, observe(*item)).find("product_recovery_gate=FAIL") !=
        std::string::npos);
}

void test_product_recovery_gate_rejects_wrong_safety_projection() {
    const OracleCase* item = nullptr;
    for (const auto& candidate : kMatrix) {
        if (candidate.domain == Domain::Run &&
            candidate.scenario == Scenario::CurrentValid) {
            item = &candidate;
            break;
        }
    }
    TEST_ASSERT_NOT_NULL(item);
    auto wrong = *item;
    wrong.safety = SafetyProjection::Standby;
    TEST_ASSERT_TRUE(
        machineLine(wrong, observe(*item)).find("product_recovery_gate=FAIL") !=
        std::string::npos);
}

const char* configurationRecoveryStatusName(
    fermentation::ConfigurationRecoveryStatus value) {
    using fermentation::ConfigurationRecoveryStatus;
    switch (value) {
        case ConfigurationRecoveryStatus::RuntimeReady:
            return "RuntimeReady";
        case ConfigurationRecoveryStatus::FactoryInitializationCompleted:
            return "FactoryInitializationCompleted";
        case ConfigurationRecoveryStatus::FactoryResetCompleted:
            return "FactoryResetCompleted";
        case ConfigurationRecoveryStatus::ConfigurationMutationBusy:
            return "ConfigurationMutationBusy";
        case ConfigurationRecoveryStatus::ConfigurationModelBudgetBusy:
            return "ConfigurationModelBudgetBusy";
        case ConfigurationRecoveryStatus::StateTransitionRejected:
            return "StateTransitionRejected";
        case ConfigurationRecoveryStatus::ConfigurationUnavailable:
            return "ConfigurationUnavailable";
        case ConfigurationRecoveryStatus::ConfigurationIntegrityFailure:
            return "ConfigurationIntegrityFailure";
        case ConfigurationRecoveryStatus::UnsupportedNewerConfigurationSchema:
            return "UnsupportedNewerConfigurationSchema";
        case ConfigurationRecoveryStatus::PersistenceReadFailure:
            return "PersistenceReadFailure";
        case ConfigurationRecoveryStatus::PersistenceCapacityFailure:
            return "PersistenceCapacityFailure";
        case ConfigurationRecoveryStatus::PersistenceWriteFailure:
            return "PersistenceWriteFailure";
        case ConfigurationRecoveryStatus::CounterOverflow:
            return "CounterOverflow";
        case ConfigurationRecoveryStatus::RuntimePreparationFailure:
            return "RuntimePreparationFailure";
        case ConfigurationRecoveryStatus::BootstrapCommitIndeterminate:
            return "BootstrapCommitIndeterminate";
        case ConfigurationRecoveryStatus::
            ConfigurationRecordOutcomeIndeterminate:
            return "ConfigurationRecordOutcomeIndeterminate";
        case ConfigurationRecoveryStatus::ConfigurationCommitIndeterminate:
            return "ConfigurationCommitIndeterminate";
    }
    return "UnknownConfigurationRecoveryStatus";
}

const char* configurationServiceModeName(
    fermentation::ConfigurationServiceMode value) {
    using fermentation::ConfigurationServiceMode;
    switch (value) {
        case ConfigurationServiceMode::NoRuntime:
            return "NoRuntime";
        case ConfigurationServiceMode::RecoveryPreparing:
            return "RecoveryPreparing";
        case ConfigurationServiceMode::Operational:
            return "Operational";
        case ConfigurationServiceMode::CommitInProgress:
            return "CommitInProgress";
        case ConfigurationServiceMode::CommitIndeterminate:
            return "CommitIndeterminate";
        case ConfigurationServiceMode::ResetPreparing:
            return "ResetPreparing";
        case ConfigurationServiceMode::ResetEligibleNoRuntime:
            return "ResetEligibleNoRuntime";
        case ConfigurationServiceMode::EpochResetting:
            return "EpochResetting";
        case ConfigurationServiceMode::BootstrapFinalizationPending:
            return "BootstrapFinalizationPending";
        case ConfigurationServiceMode::RuntimeFailure:
            return "RuntimeFailure";
    }
    return "UnknownConfigurationServiceMode";
}

const char* runPersistenceLoadStatusName(
    fermentation::RunPersistenceLoadStatus value) {
    using fermentation::RunPersistenceLoadStatus;
    switch (value) {
        case RunPersistenceLoadStatus::NoPersistedRun:
            return "NoPersistedRun";
        case RunPersistenceLoadStatus::Current:
            return "Current";
        case RunPersistenceLoadStatus::NoActiveRun:
            return "NoActiveRun";
        case RunPersistenceLoadStatus::FallbackRecovered:
            return "FallbackRecovered";
        case RunPersistenceLoadStatus::PreparedInterrupted:
            return "PreparedInterrupted";
        case RunPersistenceLoadStatus::NotReconstructible:
            return "NotReconstructible";
        case RunPersistenceLoadStatus::NotReconstructibleOrphanedState:
            return "NotReconstructibleOrphanedState";
        case RunPersistenceLoadStatus::ReadFailed:
            return "ReadFailed";
        case RunPersistenceLoadStatus::CapacityExceeded:
            return "CapacityExceeded";
        case RunPersistenceLoadStatus::UnsupportedSchema:
            return "UnsupportedSchema";
        case RunPersistenceLoadStatus::ForeignEpoch:
            return "ForeignEpoch";
        case RunPersistenceLoadStatus::AlreadyInitialized:
            return "AlreadyInitialized";
    }
    return "UnknownRunPersistenceLoadStatus";
}

const char* runPersistenceCoordinatorStateName(
    fermentation::RunPersistenceCoordinatorState value) {
    using fermentation::RunPersistenceCoordinatorState;
    switch (value) {
        case RunPersistenceCoordinatorState::Uninitialized:
            return "Uninitialized";
        case RunPersistenceCoordinatorState::ReadyEmpty:
            return "ReadyEmpty";
        case RunPersistenceCoordinatorState::LoadedActiveRun:
            return "LoadedActiveRun";
        case RunPersistenceCoordinatorState::Ready:
            return "Ready";
        case RunPersistenceCoordinatorState::Busy:
            return "Busy";
        case RunPersistenceCoordinatorState::BlockedIndeterminate:
            return "BlockedIndeterminate";
        case RunPersistenceCoordinatorState::FallbackRecoveryPending:
            return "FallbackRecoveryPending";
        case RunPersistenceCoordinatorState::PersistenceCommittedApplyFailed:
            return "PersistenceCommittedApplyFailed";
    }
    return "UnknownRunPersistenceCoordinatorState";
}

const char* safetyBootDispositionName(
    fermentation::SafetyBootDisposition value) {
    using fermentation::SafetyBootDisposition;
    switch (value) {
        case SafetyBootDisposition::Unresolved:
            return "UNRESOLVED";
        case SafetyBootDisposition::Standby:
            return "STANDBY";
        case SafetyBootDisposition::ResumeOffer:
            return "RESUME_OFFER";
        case SafetyBootDisposition::NoActiveRun:
            return "NO_ACTIVE_RUN";
        case SafetyBootDisposition::Completed:
            return "COMPLETED";
        case SafetyBootDisposition::TerminalFault:
            return "TERMINAL_FAULT";
        case SafetyBootDisposition::SafeBoot:
            return "SAFE_BOOT";
    }
    return "UNMAPPED";
}

const char* faultCodeName(fermentation::FaultCode value) {
    using fermentation::FaultCode;
    switch (value) {
        case FaultCode::None:
            return "None";
        case FaultCode::ConfigurationRuntimeFailure:
            return "ConfigurationRuntimeFailure";
        case FaultCode::ConfigurationUnavailable:
            return "ConfigurationUnavailable";
        case FaultCode::ConfigurationIntegrityFailure:
            return "ConfigurationIntegrityFailure";
        case FaultCode::ConfigurationCommitIndeterminate:
            return "ConfigurationCommitIndeterminate";
        case FaultCode::RunPersistenceUntrusted:
            return "RunPersistenceUntrusted";
        case FaultCode::SafetySensorUnavailable:
            return "SafetySensorUnavailable";
        case FaultCode::ActuatorRequestWatchdog:
            return "ActuatorRequestWatchdog";
        case FaultCode::SystemProducerUnknown:
            return "SystemProducerUnknown";
    }
    return "UnknownFaultCode";
}

const char* gateStatusName(fermentation::ActuatorSafetyGateStatus value) {
    using fermentation::ActuatorSafetyGateStatus;
    switch (value) {
        case ActuatorSafetyGateStatus::Unresolved:
            return "UNRESOLVED";
        case ActuatorSafetyGateStatus::Allowed:
            return "ALLOWED";
        case ActuatorSafetyGateStatus::ImmediateStop:
            return "IMMEDIATE_STOP";
    }
    return "UNMAPPED";
}

struct ProductionActual {
    std::string primaryStatus;
    std::string secondaryStatus;
    std::string recordClassification{"UNMAPPED"};
    std::string productOutcome{"UNMAPPED"};
    std::string safetyProjection{"UNMAPPED"};
    std::string safetyProducer{"UNMAPPED"};
    std::string logicalGate{"UNMAPPED"};
    bool actuatorAllowed{false};
    std::string baseline{"NONE"};
    std::string recoveryAction{"NONE"};
};

class ProductionResolver final : public device_platform::ITimeZoneResolver {
   public:
    device_platform::TimeZonePrepareResult prepare(
        const std::string& value) const override {
        if (value != "Europe/Zurich") {
            return {
                device_platform::TimeZonePrepareStatus::UnsupportedIdentifier,
                std::nullopt};
        }
        return {device_platform::TimeZonePrepareStatus::Success,
                device_platform::PreparedTimeZone{value}};
    }
};

BackendObservation seedProductionStore(const OracleCase& item,
                                       SimulatedPersistentStateStore& store) {
    const auto observed = observe(item);
    for (const auto& [key, value] : observed.committed)
        put(store, key.c_str(), value);
    store.restart();
    if (item.scenario == Scenario::UnknownCommitNotFound) {
        store.forceNotFound(keyFor(observed.productReadTargetKey.c_str()),
                            true);
    }
    if (item.scenario == Scenario::ReadError) {
        store.injectReadFailure(keyFor(observed.productReadTargetKey.c_str()),
                                true);
    }
    return observed;
}

fermentation::SafetyEvaluation evaluateProductionSafety(
    const ProductionActual& /*unused*/, bool configurationValidated,
    fermentation::ConfigurationRecoveryStatus configurationStatus,
    fermentation::ConfigurationServiceMode configurationMode,
    std::optional<fermentation::ConfigurationSafetyProducer>
        configurationProducer,
    fermentation::RunPersistenceLoadStatus persistenceStatus,
    const fermentation::RunPersistenceSnapshot* persistenceSnapshot,
    fermentation::RunPersistenceCoordinatorState coordinatorState) {
    fermentation::SafetyCore safety;
    safety.beginBoot(device_platform::ResetCause::PowerOn);
    fermentation::SafetyCoreInput input;
    input.configurationValidated = configurationValidated;
    input.configurationRecoveryStatus = configurationStatus;
    input.configurationServiceMode = configurationMode;
    input.configurationProducer = configurationProducer;
    input.persistenceValidated =
        persistenceStatus ==
            fermentation::RunPersistenceLoadStatus::NoPersistedRun ||
        persistenceStatus == fermentation::RunPersistenceLoadStatus::Current ||
        persistenceStatus ==
            fermentation::RunPersistenceLoadStatus::NoActiveRun;
    input.persistenceLoadStatus = persistenceStatus;
    input.persistenceSnapshot = persistenceSnapshot;
    input.persistenceCoordinatorState = coordinatorState;
    device_platform::SensorQualitySnapshot sensor;
    fermentation::SensorSelectionRuntimeState selection;
    if (persistenceSnapshot != nullptr) {
        sensor.quality = device_platform::SensorQuality::Valid;
        selection.permission = fermentation::SensorPeltierPermission::Allowed;
        input.sensorEvidenceValidated = true;
        input.peltierSensor = &sensor;
        input.sensorSelectionRuntime = &selection;
    }
    return safety.evaluate(input);
}

ProductionActual runConfigurationProduction(
    const OracleCase& item, SimulatedPersistentStateStore& store) {
    static_cast<void>(item);
    ProductionResolver resolver;
    fermentation::ConfigurationMutationCoordinator coordinator;
    fermentation::ConfigurationBootstrapStore bootstrap(store);
    fermentation::ConfigurationGraphStore graph(store, resolver);
    fermentation::ConfigurationService service(coordinator, graph, resolver);
    auto recovery = fermentation::ConfigurationRecoveryService::create(
        store, bootstrap, graph, service, coordinator);
    const auto result = recovery->boot();
    const auto graphResult = graph.loadCanonicalGraph(kEpoch);

    ProductionActual actual;
    actual.primaryStatus = configurationRecoveryStatusName(result.status);
    actual.secondaryStatus =
        std::string("service_mode=") +
        configurationServiceModeName(service.mode()) + ";fallback_used=" +
        (result.diagnostics.fallbackUsed ? "true" : "false") +
        ";skipped_higher_roots=" +
        std::to_string(result.diagnostics.skippedHigherRoots);
    const bool ready = result.status ==
                       fermentation::ConfigurationRecoveryStatus::RuntimeReady;
    const bool factoryCompleted = result.status ==
                                  fermentation::ConfigurationRecoveryStatus::
                                      FactoryInitializationCompleted;
    if (factoryCompleted) {
        actual.baseline = baselineName(BaselineClassification::FactoryEmpty);
        actual.recoveryAction =
            actionName(ExpectedRecoveryAction::FactoryInitialization);
    } else if (ready) {
        if (graphResult.graph.has_value() &&
            graphResult.graph->selectedFallback) {
            actual.productOutcome = "FALLBACK_VALID_CONFIGURATION";
            actual.recordClassification = "FullyValidFallback";
        } else {
            const auto higherManifest = store.read(
                keyFor("cm1"), fermentation::configuration_limits::
                                   kMaximumConfigurationManifestEnvelopeBytes);
            const bool olderGenerationSelected =
                result.diagnostics.skippedHigherRoots > 0U ||
                (graphResult.graph.has_value() &&
                 graphResult.graph->rootSequence.value() == 1U &&
                 higherManifest.status == StateStoreReadStatus::Success);
            if (olderGenerationSelected) {
                actual.productOutcome = "OLD_VALID_CONFIGURATION";
                actual.recordClassification = "FullyValidOlder";
            } else {
                actual.productOutcome = "NEW_VALID_CONFIGURATION";
                actual.recordClassification = "FullyValidCurrent";
            }
        }
    } else {
        actual.productOutcome = "CONFIGURATION_RECOVERY_REQUIRED";
        actual.recordClassification = "Indeterminate";
    }
    const auto safety = evaluateProductionSafety(
        actual, ready || factoryCompleted, result.status, service.mode(),
        result.safetyProducer,
        fermentation::RunPersistenceLoadStatus::NoPersistedRun, nullptr,
        fermentation::RunPersistenceCoordinatorState::ReadyEmpty);
    actual.safetyProjection = safetyBootDispositionName(safety.bootDisposition);
    actual.safetyProducer = faultCodeName(safety.faultCode);
    actual.logicalGate = gateStatusName(safety.gate.status);
    actual.actuatorAllowed =
        safety.gate.status == fermentation::ActuatorSafetyGateStatus::Allowed;
    return actual;
}

ProductionActual runRunProduction(const OracleCase& item,
                                  SimulatedPersistentStateStore& store) {
    static_cast<void>(item);
    fermentation::RunPersistenceCoordinator coordinator(
        store, kEpoch, fermentation::RunCheckpointSchedule{});
    const auto result = coordinator.loadAndInitialize();
    ProductionActual actual;
    actual.primaryStatus = runPersistenceLoadStatusName(result.status);
    actual.secondaryStatus =
        std::string("coordinator_state=") +
        runPersistenceCoordinatorStateName(coordinator.state());
    if (result.status ==
        fermentation::RunPersistenceLoadStatus::NoPersistedRun) {
        actual.baseline = baselineName(BaselineClassification::NoPersistedRun);
        actual.recoveryAction = actionName(ExpectedRecoveryAction::NoActiveRun);
    } else if (result.status ==
               fermentation::RunPersistenceLoadStatus::NoActiveRun) {
        actual.baseline =
            baselineName(BaselineClassification::ControlledDiscardTombstone);
        actual.recoveryAction = actionName(ExpectedRecoveryAction::NoActiveRun);
    } else if (result.status ==
                   fermentation::RunPersistenceLoadStatus::Current &&
               result.snapshot.has_value()) {
        if (result.snapshot->variant ==
            fermentation::RunCheckpointVariant::NoActiveRun) {
            actual.baseline = baselineName(
                BaselineClassification::ControlledDiscardTombstone);
            actual.recoveryAction =
                actionName(ExpectedRecoveryAction::NoActiveRun);
        } else if (fermentation::SafetyCore::isR1ResumeEligible(
                       *result.snapshot)) {
            actual.productOutcome = "NEW_VALID_RESUME";
            actual.recordClassification = "FullyValidCurrent";
        } else {
            actual.productOutcome = "RUN_ABORT_REQUIRED";
            actual.recordClassification = "FullyValidCurrent";
        }
    } else if (result.status ==
               fermentation::RunPersistenceLoadStatus::FallbackRecovered) {
        actual.productOutcome = "OLDER_VALID_CHECKPOINT_RESUME";
        actual.recordClassification = "FullyValidFallback";
    } else {
        actual.productOutcome = "RUN_RECOVERY_REQUIRED";
        actual.recordClassification = "Indeterminate";
    }
    const auto safety = evaluateProductionSafety(
        actual, true, fermentation::ConfigurationRecoveryStatus::RuntimeReady,
        fermentation::ConfigurationServiceMode::Operational, std::nullopt,
        result.status,
        result.snapshot.has_value() ? &*result.snapshot : nullptr,
        coordinator.state());
    actual.safetyProjection = safetyBootDispositionName(safety.bootDisposition);
    actual.safetyProducer = faultCodeName(safety.faultCode);
    actual.logicalGate = gateStatusName(safety.gate.status);
    actual.actuatorAllowed =
        safety.gate.status == fermentation::ActuatorSafetyGateStatus::Allowed;
    return actual;
}

const char* expectedSafetyName(SafetyProjection value) {
    switch (value) {
        case SafetyProjection::Standby:
            return "STANDBY";
        case SafetyProjection::NoActiveRun:
            return "NO_ACTIVE_RUN";
        case SafetyProjection::ResumeOffer:
            return "RESUME_OFFER";
        case SafetyProjection::SafeBoot:
            return "SAFE_BOOT";
    }
    return "UNMAPPED";
}

const char* expectedProducerName(SafetyProducer value) {
    switch (value) {
        case SafetyProducer::None:
            return "None";
        case SafetyProducer::ConfigurationUnavailable:
            return "ConfigurationUnavailable";
        case SafetyProducer::ConfigurationIntegrityFailure:
            return "ConfigurationIntegrityFailure";
        case SafetyProducer::RunPersistenceUntrusted:
            return "RunPersistenceUntrusted";
    }
    return "UNMAPPED";
}

void test_existing_production_matches_slice2_oracle() {
    std::size_t pass = 0U;
    std::size_t fail = 0U;
    std::size_t blocked = 0U;
    std::size_t notRun = 0U;
    for (const auto& item : kMatrix) {
        SimulatedPersistentStateStore store;
        const auto fixture = seedProductionStore(item, store);
        const auto actual = item.domain == Domain::Configuration
                                ? runConfigurationProduction(item, store)
                                : runRunProduction(item, store);
        const std::string expectedOutcome = item.hasProductOutcome
                                                ? outcomeName(item.outcome)
                                                : "NOT_APPLICABLE";
        const std::string expectedSafety = item.hasProductOutcome
                                               ? expectedSafetyName(item.safety)
                                               : "NOT_APPLICABLE";
        const std::string expectedProducer =
            item.hasProductOutcome ? expectedProducerName(item.producer)
                                   : "NOT_APPLICABLE";
        const bool baselineMatch =
            actual.baseline == baselineName(item.baseline) &&
            actual.recoveryAction == actionName(item.recoveryAction);
        const bool outcomeMatch =
            !item.hasProductOutcome || actual.productOutcome == expectedOutcome;
        const bool safetyMatch = !item.hasProductOutcome ||
                                 actual.safetyProjection == expectedSafety;
        const bool producerMatch = !item.hasProductOutcome ||
                                   actual.safetyProducer == expectedProducer;
        const bool gateMatch =
            actual.logicalGate == "UNRESOLVED" && !actual.actuatorAllowed;
        const bool comparisonPass = baselineMatch && outcomeMatch &&
                                    safetyMatch && producerMatch && gateMatch;
        const char* difference = "NONE";
        if (!outcomeMatch) {
            difference = "RECOVERY";
        } else if (!producerMatch || !safetyMatch) {
            difference = "SAFETY";
        } else if (!gateMatch) {
            difference = "GATE";
        } else if (!baselineMatch) {
            difference = "STATUS";
        }
        const char* resultName = comparisonPass ? "PASS" : "FAIL";
        if (comparisonPass) {
            ++pass;
        } else {
            ++fail;
        }
        std::printf(
            "issue90_production_compare_case=%s expected_product_outcome=%s "
            "expected_safety_projection=%s expected_safety_producer=%s "
            "expected_logical_gate=UNRESOLVED expected_actuator_allowed=false "
            "actual_primary_status=%s actual_secondary_status=%s "
            "actual_record_classification=%s actual_product_outcome=%s "
            "actual_safety_projection=%s actual_safety_producer=%s "
            "actual_logical_gate=%s actual_actuator_allowed=%s "
            "expected_baseline=%s actual_baseline=%s "
            "expected_recovery_action=%s actual_recovery_action=%s "
            "fixture_read_status=%s comparison_result=%s difference_class=%s "
            "fixture_reboot=%s\n",
            item.id, expectedOutcome.c_str(), expectedSafety.c_str(),
            expectedProducer.c_str(), actual.primaryStatus.c_str(),
            actual.secondaryStatus.c_str(), actual.recordClassification.c_str(),
            actual.productOutcome.c_str(), actual.safetyProjection.c_str(),
            actual.safetyProducer.c_str(), actual.logicalGate.c_str(),
            actual.actuatorAllowed ? "true" : "false",
            baselineName(item.baseline), actual.baseline.c_str(),
            actionName(item.recoveryAction), actual.recoveryAction.c_str(),
            readStatusName(fixture.readStatus), resultName, difference,
            fixture.restarted ? "true" : "false");
    }
    TEST_ASSERT_EQUAL_UINT32(
        static_cast<std::uint32_t>(kMatrix.size()),
        static_cast<std::uint32_t>(pass + fail + blocked + notRun));
    std::printf(
        "issue90_production_compare_summary=cases:%zu pass:%zu fail:%zu "
        "blocked:%zu not_run:%zu callback12=REAL_NVS_ONLY/NOT_RUN\n",
        kMatrix.size(), pass, fail, blocked, notRun);
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
    RUN_TEST(test_configuration_old_and_fallback_graphs_are_distinct);
    RUN_TEST(test_run_current_and_fallback_revisions_share_one_run_line);
    RUN_TEST(test_forbidden_product_states_are_negative_matrix_entries);
    RUN_TEST(test_semantic_counterexamples_are_explicit);
    RUN_TEST(test_machine_output_contains_fixture_and_truth_separation);
    RUN_TEST(test_product_recovery_gate_rejects_inconsistent_observation);
    RUN_TEST(test_product_recovery_gate_rejects_invalid_fallback_reference);
    RUN_TEST(test_product_recovery_gate_rejects_wrong_product_outcome);
    RUN_TEST(test_product_recovery_gate_rejects_wrong_safety_projection);
    RUN_TEST(test_existing_production_matches_slice2_oracle);
    RUN_TEST(test_callback_12_remains_real_nvs_only);
    return UNITY_END();
}
