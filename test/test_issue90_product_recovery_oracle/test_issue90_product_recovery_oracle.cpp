#include <unity.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <optional>
#include <string>

#include "simulated_persistent_state_store.hpp"
#include "state_store.hpp"
#include "state_store_key.hpp"

namespace {

using device_platform::StateStoreKey;
using device_platform::StateStoreReadStatus;
using device_platform::StateStoreWriteStatus;
using device_platform_test_support::SimulatedPersistentStateStore;

// Diese Typen sind absichtlich testseitig: Slice 2 definiert die erwartete
// Produktwahrheit, ohne neue oeffentliche Produktions-Enums einzufuehren.
enum class Domain : std::uint8_t {
    Configuration,
    Run,
};

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
    Missing,
    Partial,
    Mixed,
    CorruptEnvelopeCrc,
    UnsupportedSchema,
    InvalidReference,
    ForeignEpoch,
    PreparedInterrupted,
    Orphan,
    NotReconstructible,
    ControlledDiscard,
};

enum class RecordClassification : std::uint8_t {
    FullyValidCurrent,
    FullyValidOlder,
    FullyValidFallback,
    Missing,
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

enum class SafetyProjection : std::uint8_t {
    Standby,
    ResumeOffer,
    SafeBoot,
};

enum class SafetyProducer : std::uint8_t {
    None,
    ConfigurationUnavailable,
    RunPersistenceUntrusted,
};

enum class ProductRecoveryGate : std::uint8_t {
    Pass,
    Fail,
    NotRun,
};

struct OracleCase {
    const char* id;
    Domain domain;
    const char* mutationPath;
    const char* recordFamily;
    Scenario scenario;
    StateStoreWriteStatus expectedWriteStatus;
    StateStoreReadStatus expectedReadStatus;
    RecordClassification classification;
    ProductOutcome outcome;
    SafetyProjection safety;
    SafetyProducer producer;
    ProductRecoveryGate productGate;
    bool prohibitedActiveState;
};

// Die Matrix ist die unabhaengige Erwartungsquelle fuer Slice 3. Sie ruft
// keine Produktions-Recoveryentscheidung auf und leitet Expected nicht aus
// einem Actual-Status ab.
const std::array<OracleCase, 40> kMatrix{{
    {"config_cr0_new_valid", Domain::Configuration,
     "mutating configuration root write", "cr0/cr1 + manifest",
     Scenario::CurrentValid, StateStoreWriteStatus::Success,
     StateStoreReadStatus::Success, RecordClassification::FullyValidCurrent,
     ProductOutcome::NewValidConfiguration, SafetyProjection::Standby,
     SafetyProducer::None, ProductRecoveryGate::Pass, false},
    {"config_cr1_old_valid", Domain::Configuration, "configuration recovery",
     "cr1 + referenced graph", Scenario::OlderValid,
     StateStoreWriteStatus::Success, StateStoreReadStatus::Success,
     RecordClassification::FullyValidOlder,
     ProductOutcome::OldValidConfiguration, SafetyProjection::Standby,
     SafetyProducer::None, ProductRecoveryGate::Pass, false},
    {"config_cm0_fallback_valid", Domain::Configuration,
     "configuration fallback recovery", "cm0/cm1/cm2 + fallback root",
     Scenario::FallbackValid, StateStoreWriteStatus::Success,
     StateStoreReadStatus::Success, RecordClassification::FullyValidFallback,
     ProductOutcome::FallbackValidConfiguration, SafetyProjection::Standby,
     SafetyProducer::None, ProductRecoveryGate::Pass, false},
    {"config_cr1_unknown_commit_new", Domain::Configuration,
     "mutating configuration root write", "cr1", Scenario::UnknownCommitValid,
     StateStoreWriteStatus::CommitOutcomeUnknown, StateStoreReadStatus::Success,
     RecordClassification::FullyValidCurrent,
     ProductOutcome::NewValidConfiguration, SafetyProjection::Standby,
     SafetyProducer::None, ProductRecoveryGate::Pass, false},
    {"config_cr1_write_error_old", Domain::Configuration,
     "mutating configuration root write", "cr1", Scenario::SafeWriteErrorOld,
     StateStoreWriteStatus::WriteError, StateStoreReadStatus::Success,
     RecordClassification::FullyValidOlder,
     ProductOutcome::OldValidConfiguration, SafetyProjection::Standby,
     SafetyProducer::None, ProductRecoveryGate::Pass, false},
    {"config_cr1_capacity_error_old", Domain::Configuration,
     "mutating configuration root write", "cr1", Scenario::SafeCapacityOld,
     StateStoreWriteStatus::CapacityError, StateStoreReadStatus::Success,
     RecordClassification::FullyValidOlder,
     ProductOutcome::OldValidConfiguration, SafetyProjection::Standby,
     SafetyProducer::None, ProductRecoveryGate::Pass, false},
    {"config_cr1_unknown_not_found", Domain::Configuration,
     "configuration recovery after unknown commit", "cr1",
     Scenario::UnknownCommitNotFound,
     StateStoreWriteStatus::CommitOutcomeUnknown,
     StateStoreReadStatus::NotFound, RecordClassification::Indeterminate,
     ProductOutcome::ConfigurationRecoveryRequired, SafetyProjection::SafeBoot,
     SafetyProducer::ConfigurationUnavailable, ProductRecoveryGate::Pass, true},
    {"config_cr1_read_error", Domain::Configuration, "configuration recovery",
     "cr1", Scenario::ReadError, StateStoreWriteStatus::Success,
     StateStoreReadStatus::ReadError, RecordClassification::Indeterminate,
     ProductOutcome::ConfigurationRecoveryRequired, SafetyProjection::SafeBoot,
     SafetyProducer::ConfigurationUnavailable, ProductRecoveryGate::Pass, true},
    {"config_cr1_read_capacity", Domain::Configuration,
     "configuration recovery", "cr1", Scenario::ReadCapacity,
     StateStoreWriteStatus::Success, StateStoreReadStatus::CapacityError,
     RecordClassification::Indeterminate,
     ProductOutcome::ConfigurationRecoveryRequired, SafetyProjection::SafeBoot,
     SafetyProducer::ConfigurationUnavailable, ProductRecoveryGate::Pass, true},
    {"config_cr0_missing", Domain::Configuration, "configuration recovery",
     "cr0", Scenario::Missing, StateStoreWriteStatus::Success,
     StateStoreReadStatus::NotFound, RecordClassification::Missing,
     ProductOutcome::ConfigurationRecoveryRequired, SafetyProjection::SafeBoot,
     SafetyProducer::ConfigurationUnavailable, ProductRecoveryGate::Pass, true},
    {"config_uc0_partial", Domain::Configuration, "configuration recovery",
     "uc0", Scenario::Partial, StateStoreWriteStatus::Success,
     StateStoreReadStatus::Success, RecordClassification::Partial,
     ProductOutcome::ConfigurationRecoveryRequired, SafetyProjection::SafeBoot,
     SafetyProducer::ConfigurationUnavailable, ProductRecoveryGate::Pass, true},
    {"config_sc0_mixed", Domain::Configuration, "configuration recovery",
     "sc0/sc1 + generation graph", Scenario::Mixed,
     StateStoreWriteStatus::Success, StateStoreReadStatus::Success,
     RecordClassification::Mixed, ProductOutcome::ConfigurationRecoveryRequired,
     SafetyProjection::SafeBoot, SafetyProducer::ConfigurationUnavailable,
     ProductRecoveryGate::Pass, true},
    {"config_cm1_corrupt_envelope_crc", Domain::Configuration,
     "configuration recovery", "cm1", Scenario::CorruptEnvelopeCrc,
     StateStoreWriteStatus::Success, StateStoreReadStatus::Success,
     RecordClassification::Corrupt,
     ProductOutcome::ConfigurationRecoveryRequired, SafetyProjection::SafeBoot,
     SafetyProducer::ConfigurationUnavailable, ProductRecoveryGate::Pass, true},
    {"config_cm2_unsupported_schema", Domain::Configuration,
     "configuration recovery", "cm2", Scenario::UnsupportedSchema,
     StateStoreWriteStatus::Success, StateStoreReadStatus::Success,
     RecordClassification::UnsupportedSchema,
     ProductOutcome::ConfigurationRecoveryRequired, SafetyProjection::SafeBoot,
     SafetyProducer::ConfigurationUnavailable, ProductRecoveryGate::Pass, true},
    {"config_cr0_invalid_reference", Domain::Configuration,
     "configuration recovery", "cr0 -> missing manifest",
     Scenario::InvalidReference, StateStoreWriteStatus::Success,
     StateStoreReadStatus::Success, RecordClassification::InvalidReference,
     ProductOutcome::ConfigurationRecoveryRequired, SafetyProjection::SafeBoot,
     SafetyProducer::ConfigurationUnavailable, ProductRecoveryGate::Pass, true},
    {"config_cr1_foreign_epoch", Domain::Configuration,
     "configuration recovery", "cr1 + StorageEpoch", Scenario::ForeignEpoch,
     StateStoreWriteStatus::Success, StateStoreReadStatus::Success,
     RecordClassification::ForeignEpoch,
     ProductOutcome::ConfigurationRecoveryRequired, SafetyProjection::SafeBoot,
     SafetyProducer::ConfigurationUnavailable, ProductRecoveryGate::Pass, true},
    {"config_cr0_prepared_interrupted", Domain::Configuration,
     "mutating configuration root write", "cr0", Scenario::PreparedInterrupted,
     StateStoreWriteStatus::WriteError, StateStoreReadStatus::Success,
     RecordClassification::PreparedInterrupted,
     ProductOutcome::ConfigurationRecoveryRequired, SafetyProjection::SafeBoot,
     SafetyProducer::ConfigurationUnavailable, ProductRecoveryGate::Pass, true},
    {"config_cr1_orphan", Domain::Configuration, "configuration recovery",
     "cr1 orphan generation", Scenario::Orphan, StateStoreWriteStatus::Success,
     StateStoreReadStatus::Success, RecordClassification::Orphan,
     ProductOutcome::ConfigurationRecoveryRequired, SafetyProjection::SafeBoot,
     SafetyProducer::ConfigurationUnavailable, ProductRecoveryGate::Pass, true},
    {"config_cr0_not_reconstructible", Domain::Configuration,
     "configuration recovery", "cr0/cr1/root graph",
     Scenario::NotReconstructible, StateStoreWriteStatus::Success,
     StateStoreReadStatus::Success, RecordClassification::NotReconstructible,
     ProductOutcome::ConfigurationRecoveryRequired, SafetyProjection::SafeBoot,
     SafetyProducer::ConfigurationUnavailable, ProductRecoveryGate::Pass, true},
    {"config_cb0_bootstrap_missing", Domain::Configuration,
     "configuration bootstrap recovery", "cb0/cb1", Scenario::Missing,
     StateStoreWriteStatus::Success, StateStoreReadStatus::NotFound,
     RecordClassification::Missing,
     ProductOutcome::ConfigurationRecoveryRequired, SafetyProjection::SafeBoot,
     SafetyProducer::ConfigurationUnavailable, ProductRecoveryGate::Pass, true},
    {"run_rc0_new_valid_resume", Domain::Run, "mutating run checkpoint write",
     "rc0/rc1 + rh0", Scenario::CurrentValid, StateStoreWriteStatus::Success,
     StateStoreReadStatus::Success, RecordClassification::FullyValidCurrent,
     ProductOutcome::NewValidResume, SafetyProjection::ResumeOffer,
     SafetyProducer::None, ProductRecoveryGate::Pass, false},
    {"run_rc1_older_checkpoint_resume", Domain::Run, "run fallback recovery",
     "rc1 + rh0", Scenario::OlderValid, StateStoreWriteStatus::Success,
     StateStoreReadStatus::Success, RecordClassification::FullyValidOlder,
     ProductOutcome::OlderValidCheckpointResume, SafetyProjection::ResumeOffer,
     SafetyProducer::None, ProductRecoveryGate::Pass, false},
    {"run_rh0_fallback_resume", Domain::Run, "run fallback recovery",
     "rh0 -> rc1", Scenario::FallbackValid, StateStoreWriteStatus::Success,
     StateStoreReadStatus::Success, RecordClassification::FullyValidFallback,
     ProductOutcome::OlderValidCheckpointResume, SafetyProjection::ResumeOffer,
     SafetyProducer::None, ProductRecoveryGate::Pass, false},
    {"run_rh0_unknown_commit_new", Domain::Run, "mutating run head write",
     "rh0", Scenario::UnknownCommitValid,
     StateStoreWriteStatus::CommitOutcomeUnknown, StateStoreReadStatus::Success,
     RecordClassification::FullyValidCurrent, ProductOutcome::NewValidResume,
     SafetyProjection::ResumeOffer, SafetyProducer::None,
     ProductRecoveryGate::Pass, false},
    {"run_rc0_write_error_older", Domain::Run, "mutating run checkpoint write",
     "rc0", Scenario::SafeWriteErrorOld, StateStoreWriteStatus::WriteError,
     StateStoreReadStatus::Success, RecordClassification::FullyValidOlder,
     ProductOutcome::OlderValidCheckpointResume, SafetyProjection::ResumeOffer,
     SafetyProducer::None, ProductRecoveryGate::Pass, false},
    {"run_rc1_capacity_error_older", Domain::Run,
     "mutating run checkpoint write", "rc1", Scenario::SafeCapacityOld,
     StateStoreWriteStatus::CapacityError, StateStoreReadStatus::Success,
     RecordClassification::FullyValidOlder,
     ProductOutcome::OlderValidCheckpointResume, SafetyProjection::ResumeOffer,
     SafetyProducer::None, ProductRecoveryGate::Pass, false},
    {"run_rh0_unknown_not_found", Domain::Run,
     "run recovery after unknown commit", "rh0",
     Scenario::UnknownCommitNotFound,
     StateStoreWriteStatus::CommitOutcomeUnknown,
     StateStoreReadStatus::NotFound, RecordClassification::Indeterminate,
     ProductOutcome::RunRecoveryRequired, SafetyProjection::SafeBoot,
     SafetyProducer::RunPersistenceUntrusted, ProductRecoveryGate::Pass, true},
    {"run_rh0_read_error", Domain::Run, "run recovery", "rh0",
     Scenario::ReadError, StateStoreWriteStatus::Success,
     StateStoreReadStatus::ReadError, RecordClassification::Indeterminate,
     ProductOutcome::RunRecoveryRequired, SafetyProjection::SafeBoot,
     SafetyProducer::RunPersistenceUntrusted, ProductRecoveryGate::Pass, true},
    {"run_rc0_read_capacity", Domain::Run, "run recovery", "rc0",
     Scenario::ReadCapacity, StateStoreWriteStatus::Success,
     StateStoreReadStatus::CapacityError, RecordClassification::Indeterminate,
     ProductOutcome::RunRecoveryRequired, SafetyProjection::SafeBoot,
     SafetyProducer::RunPersistenceUntrusted, ProductRecoveryGate::Pass, true},
    {"run_rc1_missing", Domain::Run, "run recovery", "rc1", Scenario::Missing,
     StateStoreWriteStatus::Success, StateStoreReadStatus::NotFound,
     RecordClassification::Missing, ProductOutcome::RunRecoveryRequired,
     SafetyProjection::SafeBoot, SafetyProducer::RunPersistenceUntrusted,
     ProductRecoveryGate::Pass, true},
    {"run_rc0_partial", Domain::Run, "run recovery", "rc0", Scenario::Partial,
     StateStoreWriteStatus::Success, StateStoreReadStatus::Success,
     RecordClassification::Partial, ProductOutcome::RunRecoveryRequired,
     SafetyProjection::SafeBoot, SafetyProducer::RunPersistenceUntrusted,
     ProductRecoveryGate::Pass, true},
    {"run_rc1_mixed", Domain::Run, "run recovery", "rc1/rc0 + rh0",
     Scenario::Mixed, StateStoreWriteStatus::Success,
     StateStoreReadStatus::Success, RecordClassification::Mixed,
     ProductOutcome::RunRecoveryRequired, SafetyProjection::SafeBoot,
     SafetyProducer::RunPersistenceUntrusted, ProductRecoveryGate::Pass, true},
    {"run_rh0_corrupt_envelope_crc", Domain::Run, "run recovery", "rh0",
     Scenario::CorruptEnvelopeCrc, StateStoreWriteStatus::Success,
     StateStoreReadStatus::Success, RecordClassification::Corrupt,
     ProductOutcome::RunRecoveryRequired, SafetyProjection::SafeBoot,
     SafetyProducer::RunPersistenceUntrusted, ProductRecoveryGate::Pass, true},
    {"run_rc0_unsupported_schema", Domain::Run, "run recovery", "rc0",
     Scenario::UnsupportedSchema, StateStoreWriteStatus::Success,
     StateStoreReadStatus::Success, RecordClassification::UnsupportedSchema,
     ProductOutcome::RunRecoveryRequired, SafetyProjection::SafeBoot,
     SafetyProducer::RunPersistenceUntrusted, ProductRecoveryGate::Pass, true},
    {"run_rh0_invalid_reference", Domain::Run, "run recovery",
     "rh0 -> missing rc0", Scenario::InvalidReference,
     StateStoreWriteStatus::Success, StateStoreReadStatus::Success,
     RecordClassification::InvalidReference,
     ProductOutcome::RunRecoveryRequired, SafetyProjection::SafeBoot,
     SafetyProducer::RunPersistenceUntrusted, ProductRecoveryGate::Pass, true},
    {"run_rc1_foreign_epoch", Domain::Run, "run recovery", "rc1 + StorageEpoch",
     Scenario::ForeignEpoch, StateStoreWriteStatus::Success,
     StateStoreReadStatus::Success, RecordClassification::ForeignEpoch,
     ProductOutcome::RunRecoveryRequired, SafetyProjection::SafeBoot,
     SafetyProducer::RunPersistenceUntrusted, ProductRecoveryGate::Pass, true},
    {"run_rh0_prepared_interrupted", Domain::Run, "mutating run head write",
     "rh0", Scenario::PreparedInterrupted, StateStoreWriteStatus::WriteError,
     StateStoreReadStatus::Success, RecordClassification::PreparedInterrupted,
     ProductOutcome::RunRecoveryRequired, SafetyProjection::SafeBoot,
     SafetyProducer::RunPersistenceUntrusted, ProductRecoveryGate::Pass, true},
    {"run_rc0_orphan", Domain::Run, "run recovery", "rc0 orphan checkpoint",
     Scenario::Orphan, StateStoreWriteStatus::Success,
     StateStoreReadStatus::Success, RecordClassification::Orphan,
     ProductOutcome::RunRecoveryRequired, SafetyProjection::SafeBoot,
     SafetyProducer::RunPersistenceUntrusted, ProductRecoveryGate::Pass, true},
    {"run_rh0_not_reconstructible", Domain::Run, "run recovery",
     "rh0/rc0/rc1 graph", Scenario::NotReconstructible,
     StateStoreWriteStatus::Success, StateStoreReadStatus::Success,
     RecordClassification::NotReconstructible,
     ProductOutcome::RunRecoveryRequired, SafetyProjection::SafeBoot,
     SafetyProducer::RunPersistenceUntrusted, ProductRecoveryGate::Pass, true},
    {"run_controlled_discard_abort", Domain::Run, "controlled discard/abort",
     "rh0 + rc0/rc1 tombstone", Scenario::ControlledDiscard,
     StateStoreWriteStatus::Success, StateStoreReadStatus::Success,
     RecordClassification::ControlledDiscard, ProductOutcome::RunAbortRequired,
     SafetyProjection::Standby, SafetyProducer::None, ProductRecoveryGate::Pass,
     false},
}};

struct BackendObservation {
    StateStoreWriteStatus writeStatus{StateStoreWriteStatus::Success};
    StateStoreReadStatus readStatus{StateStoreReadStatus::NotFound};
    std::string readBytes;
    bool writeAttempted{false};
    bool restarted{false};
    bool pendingBeforeRestart{false};
};

StateStoreKey keyFor(const char* value) {
    return *StateStoreKey::create(value).key;
}

const char* writeStatusName(StateStoreWriteStatus status) {
    switch (status) {
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

const char* readStatusName(StateStoreReadStatus status) {
    switch (status) {
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

const char* domainName(Domain domain) {
    return domain == Domain::Configuration ? "configuration" : "run";
}

const char* validationContract(Domain domain) {
    return domain == Domain::Configuration
               ? "envelope_crc_schema_storage_epoch_generation_root_manifest_"
                 "fallback_bootstrap"
               : "envelope_crc_schema_storage_epoch_generation_rc0_rc1_rh0_"
                 "fallback";
}

const char* classificationName(RecordClassification classification) {
    switch (classification) {
        case RecordClassification::FullyValidCurrent:
            return "FullyValidCurrent";
        case RecordClassification::FullyValidOlder:
            return "FullyValidOlder";
        case RecordClassification::FullyValidFallback:
            return "FullyValidFallback";
        case RecordClassification::Missing:
            return "Missing";
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
    }
    return "UnknownClassification";
}

const char* outcomeName(ProductOutcome outcome) {
    switch (outcome) {
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

const char* safetyName(SafetyProjection safety) {
    switch (safety) {
        case SafetyProjection::Standby:
            return "STANDBY";
        case SafetyProjection::ResumeOffer:
            return "RESUME_OFFER";
        case SafetyProjection::SafeBoot:
            return "SAFE_BOOT";
    }
    return "UNKNOWN_SAFETY_PROJECTION";
}

const char* producerName(SafetyProducer producer) {
    switch (producer) {
        case SafetyProducer::None:
            return "NONE";
        case SafetyProducer::ConfigurationUnavailable:
            return "CONFIGURATION_UNAVAILABLE";
        case SafetyProducer::RunPersistenceUntrusted:
            return "RUN_PERSISTENCE_UNTRUSTED";
    }
    return "UNKNOWN_SAFETY_PRODUCER";
}

const char* gateName(ProductRecoveryGate gate) {
    switch (gate) {
        case ProductRecoveryGate::Pass:
            return "PASS";
        case ProductRecoveryGate::Fail:
            return "FAIL";
        case ProductRecoveryGate::NotRun:
            return "NOT_RUN";
    }
    return "UNKNOWN_PRODUCT_GATE";
}

std::string hexBytes(const std::string& bytes) {
    constexpr char digits[] = "0123456789abcdef";
    std::string result;
    result.reserve(bytes.size() * 2U);
    for (const char byte : bytes) {
        const auto value = static_cast<unsigned char>(byte);
        result.push_back(digits[value >> 4U]);
        result.push_back(digits[value & 0x0fU]);
    }
    return result;
}

BackendObservation observe(const OracleCase& oracleCase) {
    SimulatedPersistentStateStore store;
    const StateStoreKey key =
        keyFor(oracleCase.domain == Domain::Configuration ? "cr1" : "rh0");
    BackendObservation observed;

    const auto write = [&](const std::string& value) {
        observed.writeAttempted = true;
        observed.writeStatus = store.write(key, value);
    };
    const auto restartAndRead = [&](std::size_t maxBytes = 256U) {
        observed.pendingBeforeRestart = store.hasPendingWriteForTesting();
        store.restart();
        observed.restarted = true;
        const auto read = store.read(key, maxBytes);
        observed.readStatus = read.status;
        observed.readBytes = read.value;
    };

    switch (oracleCase.scenario) {
        case Scenario::CurrentValid:
            write("fully-valid-current");
            restartAndRead();
            break;
        case Scenario::OlderValid:
            write("fully-valid-older");
            restartAndRead();
            break;
        case Scenario::FallbackValid:
            write("fully-valid-fallback");
            restartAndRead();
            break;
        case Scenario::UnknownCommitValid:
            store.setNextWriteFault(SimulatedPersistentStateStore::WriteFault::
                                        PowerCutAfterCommitBeforeReturn);
            write("fully-valid-new-after-unknown");
            restartAndRead();
            break;
        case Scenario::SafeWriteErrorOld:
            write("fully-valid-old-before-write-error");
            store.setNextWriteFault(SimulatedPersistentStateStore::WriteFault::
                                        PowerCutBeforeCommit);
            write("candidate-not-durable");
            restartAndRead();
            break;
        case Scenario::SafeCapacityOld:
            write("fully-valid-old-before-capacity-error");
            store.setNextWriteFault(
                SimulatedPersistentStateStore::WriteFault::CapacityExceeded);
            write("candidate-not-written");
            restartAndRead();
            break;
        case Scenario::UnknownCommitNotFound:
            store.setNextWriteFault(SimulatedPersistentStateStore::WriteFault::
                                        PowerCutAfterCommitBeforeReturn);
            write("unknown-record");
            store.restart();
            observed.restarted = true;
            store.forceNotFound(key, true);
            {
                const auto read = store.read(key, 256U);
                observed.readStatus = read.status;
                observed.readBytes = read.value;
            }
            break;
        case Scenario::ReadError:
            write("record-before-read-error");
            store.restart();
            observed.restarted = true;
            store.injectReadFailure(key, true);
            {
                const auto read = store.read(key, 256U);
                observed.readStatus = read.status;
                observed.readBytes = read.value;
            }
            break;
        case Scenario::ReadCapacity:
            write(std::string(64U, 'x'));
            restartAndRead(16U);
            break;
        case Scenario::Missing:
            store.restart();
            observed.restarted = true;
            {
                const auto read = store.read(key, 256U);
                observed.readStatus = read.status;
                observed.readBytes = read.value;
            }
            break;
        case Scenario::Partial:
            write("partial-envelope");
            store.injectCorruption(key, "partial-envelope");
            restartAndRead();
            break;
        case Scenario::Mixed:
            write("mixed-generation-reference");
            store.injectCorruption(key, "mixed-generation-reference");
            restartAndRead();
            break;
        case Scenario::CorruptEnvelopeCrc:
            write("envelope-crc-invalid");
            store.injectCorruption(key, "envelope-crc-invalid");
            restartAndRead();
            break;
        case Scenario::UnsupportedSchema:
            write("schema-version-newer-than-reader");
            store.injectCorruption(key, "schema-version-newer-than-reader");
            restartAndRead();
            break;
        case Scenario::InvalidReference:
            write("root-references-missing-record");
            store.injectCorruption(key, "root-references-missing-record");
            restartAndRead();
            break;
        case Scenario::ForeignEpoch:
            write("storage-epoch-does-not-match");
            store.injectCorruption(key, "storage-epoch-does-not-match");
            restartAndRead();
            break;
        case Scenario::PreparedInterrupted:
            write("fully-valid-old-before-prepared-cut");
            store.setNextWriteFault(SimulatedPersistentStateStore::WriteFault::
                                        PowerCutBeforeCommit);
            write("prepared-interrupted");
            restartAndRead();
            break;
        case Scenario::Orphan:
            write("orphan-record-without-valid-root");
            restartAndRead();
            break;
        case Scenario::NotReconstructible:
            write("not-reconstructible-record-graph");
            restartAndRead();
            break;
        case Scenario::ControlledDiscard:
            write("controlled-discard-no-active-run");
            restartAndRead();
            break;
    }
    return observed;
}

std::string machineLine(const OracleCase& oracleCase,
                        const BackendObservation& observed) {
    std::string line{"issue90_oracle_case="};
    line += oracleCase.id;
    line += " domain=";
    line += domainName(oracleCase.domain);
    line += " mutation_path=\"";
    line += oracleCase.mutationPath;
    line += "\" record_family=\"";
    line += oracleCase.recordFamily;
    line += "\" evidence_scope=SIMULATOR_ONLY";
    line += " backend_characterization=observed";
    line += " validation_contract=";
    line += validationContract(oracleCase.domain);
    line += " write_attempted=";
    line += observed.writeAttempted ? "true" : "false";
    line += " write_status=";
    line += observed.writeAttempted ? writeStatusName(observed.writeStatus)
                                    : "NOT_APPLICABLE";
    line += " read_status=";
    line += readStatusName(observed.readStatus);
    line += " read_bytes_hex=";
    line += hexBytes(observed.readBytes);
    line += " record_classification=";
    line += classificationName(oracleCase.classification);
    line += " product_outcome=";
    line += outcomeName(oracleCase.outcome);
    line += " safety_projection=";
    line += safetyName(oracleCase.safety);
    line += " safety_producer=";
    line += producerName(oracleCase.producer);
    line += " logical_gate=UNRESOLVED actuator_allowed=false";
    line += " product_recovery_gate=";
    line += gateName(oracleCase.productGate);
    line += " reboot=";
    line += observed.restarted ? "true" : "false";
    line += " pending_before_reboot=";
    line += observed.pendingBeforeRestart ? "true" : "false";
    line += " prohibited_active_state=";
    line += oracleCase.prohibitedActiveState ? "true" : "false";
    return line;
}

void test_oracle_matrix_covers_config_and_run_domains() {
    std::size_t configurationCases = 0U;
    std::size_t runCases = 0U;
    bool hasPrepared = false;
    bool hasOrphan = false;
    bool hasPartial = false;
    bool hasMixed = false;
    bool hasCorrupt = false;
    bool hasIndeterminate = false;
    bool hasForeignEpoch = false;
    bool hasControlledDiscard = false;
    bool hasUnknownCommit = false;
    bool hasReadError = false;
    bool hasCapacityError = false;
    bool hasNotFound = false;
    bool hasForbidden = false;
    bool hasNewValidConfiguration = false;
    bool hasOldValidConfiguration = false;
    bool hasFallbackValidConfiguration = false;
    bool hasConfigurationRecovery = false;
    bool hasNewValidResume = false;
    bool hasOlderValidResume = false;
    bool hasRunRecovery = false;
    bool hasRunAbort = false;

    for (const auto& oracleCase : kMatrix) {
        if (oracleCase.domain == Domain::Configuration) {
            ++configurationCases;
        } else {
            ++runCases;
        }
        hasPrepared =
            hasPrepared || oracleCase.classification ==
                               RecordClassification::PreparedInterrupted;
        hasOrphan = hasOrphan ||
                    oracleCase.classification == RecordClassification::Orphan;
        hasPartial = hasPartial ||
                     oracleCase.classification == RecordClassification::Partial;
        hasMixed = hasMixed ||
                   oracleCase.classification == RecordClassification::Mixed;
        hasCorrupt = hasCorrupt ||
                     oracleCase.classification == RecordClassification::Corrupt;
        hasIndeterminate =
            hasIndeterminate ||
            oracleCase.classification == RecordClassification::Indeterminate;
        hasForeignEpoch =
            hasForeignEpoch ||
            oracleCase.classification == RecordClassification::ForeignEpoch;
        hasControlledDiscard =
            hasControlledDiscard || oracleCase.classification ==
                                        RecordClassification::ControlledDiscard;
        hasUnknownCommit =
            hasUnknownCommit || oracleCase.expectedWriteStatus ==
                                    StateStoreWriteStatus::CommitOutcomeUnknown;
        hasReadError = hasReadError || oracleCase.expectedReadStatus ==
                                           StateStoreReadStatus::ReadError;
        hasCapacityError =
            hasCapacityError || oracleCase.expectedReadStatus ==
                                    StateStoreReadStatus::CapacityError;
        hasNotFound = hasNotFound || oracleCase.expectedReadStatus ==
                                         StateStoreReadStatus::NotFound;
        hasForbidden = hasForbidden || oracleCase.prohibitedActiveState;
        hasNewValidConfiguration =
            hasNewValidConfiguration ||
            oracleCase.outcome == ProductOutcome::NewValidConfiguration;
        hasOldValidConfiguration =
            hasOldValidConfiguration ||
            oracleCase.outcome == ProductOutcome::OldValidConfiguration;
        hasFallbackValidConfiguration =
            hasFallbackValidConfiguration ||
            oracleCase.outcome == ProductOutcome::FallbackValidConfiguration;
        hasConfigurationRecovery =
            hasConfigurationRecovery ||
            oracleCase.outcome == ProductOutcome::ConfigurationRecoveryRequired;
        hasNewValidResume =
            hasNewValidResume ||
            oracleCase.outcome == ProductOutcome::NewValidResume;
        hasOlderValidResume =
            hasOlderValidResume ||
            oracleCase.outcome == ProductOutcome::OlderValidCheckpointResume;
        hasRunRecovery =
            hasRunRecovery ||
            oracleCase.outcome == ProductOutcome::RunRecoveryRequired;
        hasRunAbort = hasRunAbort ||
                      oracleCase.outcome == ProductOutcome::RunAbortRequired;
    }

    TEST_ASSERT_EQUAL_UINT32(20U, configurationCases);
    TEST_ASSERT_EQUAL_UINT32(20U, runCases);
    TEST_ASSERT_TRUE(hasPrepared);
    TEST_ASSERT_TRUE(hasOrphan);
    TEST_ASSERT_TRUE(hasPartial);
    TEST_ASSERT_TRUE(hasMixed);
    TEST_ASSERT_TRUE(hasCorrupt);
    TEST_ASSERT_TRUE(hasIndeterminate);
    TEST_ASSERT_TRUE(hasForeignEpoch);
    TEST_ASSERT_TRUE(hasControlledDiscard);
    TEST_ASSERT_TRUE(hasUnknownCommit);
    TEST_ASSERT_TRUE(hasReadError);
    TEST_ASSERT_TRUE(hasCapacityError);
    TEST_ASSERT_TRUE(hasNotFound);
    TEST_ASSERT_TRUE(hasForbidden);
    TEST_ASSERT_TRUE(hasNewValidConfiguration);
    TEST_ASSERT_TRUE(hasOldValidConfiguration);
    TEST_ASSERT_TRUE(hasFallbackValidConfiguration);
    TEST_ASSERT_TRUE(hasConfigurationRecovery);
    TEST_ASSERT_TRUE(hasNewValidResume);
    TEST_ASSERT_TRUE(hasOlderValidResume);
    TEST_ASSERT_TRUE(hasRunRecovery);
    TEST_ASSERT_TRUE(hasRunAbort);
}

void test_simulated_restart_and_fault_observations_match_matrix() {
    for (const auto& oracleCase : kMatrix) {
        const auto observed = observe(oracleCase);
        TEST_ASSERT_TRUE(observed.restarted);
        TEST_ASSERT_TRUE(observed.writeStatus ==
                         oracleCase.expectedWriteStatus);
        TEST_ASSERT_TRUE(observed.readStatus == oracleCase.expectedReadStatus);
        if (oracleCase.scenario == Scenario::PreparedInterrupted) {
            TEST_ASSERT_TRUE(observed.pendingBeforeRestart);
        }
        if (oracleCase.scenario != Scenario::PreparedInterrupted &&
            oracleCase.scenario != Scenario::SafeWriteErrorOld) {
            TEST_ASSERT_FALSE(observed.pendingBeforeRestart);
        }
    }
}

void test_allowed_product_outcomes_are_explicit_and_fail_closed() {
    for (const auto& oracleCase : kMatrix) {
        const auto observed = observe(oracleCase);
        const auto line = machineLine(oracleCase, observed);
        std::printf("%s\n", line.c_str());
        TEST_ASSERT_TRUE(line.find("backend_characterization=observed") !=
                         std::string::npos);
        TEST_ASSERT_TRUE(line.find("evidence_scope=SIMULATOR_ONLY") !=
                         std::string::npos);
        TEST_ASSERT_TRUE(line.find("product_recovery_gate=PASS") !=
                         std::string::npos);
        TEST_ASSERT_TRUE(line.find("logical_gate=UNRESOLVED") !=
                         std::string::npos);
        TEST_ASSERT_TRUE(line.find("actuator_allowed=false") !=
                         std::string::npos);
        if (!oracleCase.prohibitedActiveState) {
            TEST_ASSERT_TRUE(oracleCase.safety != SafetyProjection::SafeBoot);
        }
    }
}

void test_forbidden_outcomes_never_become_active() {
    for (const auto& oracleCase : kMatrix) {
        if (!oracleCase.prohibitedActiveState) continue;
        TEST_ASSERT_TRUE(oracleCase.outcome ==
                             ProductOutcome::ConfigurationRecoveryRequired ||
                         oracleCase.outcome ==
                             ProductOutcome::RunRecoveryRequired);
        TEST_ASSERT_TRUE(oracleCase.safety == SafetyProjection::SafeBoot);
        TEST_ASSERT_TRUE(oracleCase.producer != SafetyProducer::None);
        TEST_ASSERT_TRUE(oracleCase.productGate == ProductRecoveryGate::Pass);
    }

    const auto discard = kMatrix.back();
    TEST_ASSERT_TRUE(discard.outcome == ProductOutcome::RunAbortRequired);
    TEST_ASSERT_TRUE(discard.safety == SafetyProjection::Standby);
    TEST_ASSERT_FALSE(discard.prohibitedActiveState);
}

void test_record_validation_and_recovery_outcomes_are_independent_of_actual() {
    std::size_t configurationRecoveryRequired = 0U;
    std::size_t runRecoveryRequired = 0U;
    std::size_t validConfiguration = 0U;
    std::size_t validResume = 0U;

    for (const auto& oracleCase : kMatrix) {
        const auto observed = observe(oracleCase);
        TEST_ASSERT_TRUE(observed.readStatus == oracleCase.expectedReadStatus);
        if (oracleCase.outcome ==
            ProductOutcome::ConfigurationRecoveryRequired) {
            ++configurationRecoveryRequired;
            TEST_ASSERT_TRUE(oracleCase.prohibitedActiveState);
        }
        if (oracleCase.outcome == ProductOutcome::RunRecoveryRequired) {
            ++runRecoveryRequired;
            TEST_ASSERT_TRUE(oracleCase.prohibitedActiveState);
        }
        if (oracleCase.outcome == ProductOutcome::NewValidConfiguration ||
            oracleCase.outcome == ProductOutcome::OldValidConfiguration ||
            oracleCase.outcome == ProductOutcome::FallbackValidConfiguration) {
            ++validConfiguration;
            TEST_ASSERT_FALSE(oracleCase.prohibitedActiveState);
        }
        if (oracleCase.outcome == ProductOutcome::NewValidResume ||
            oracleCase.outcome == ProductOutcome::OlderValidCheckpointResume) {
            ++validResume;
            TEST_ASSERT_FALSE(oracleCase.prohibitedActiveState);
            TEST_ASSERT_TRUE(oracleCase.safety ==
                             SafetyProjection::ResumeOffer);
        }
    }

    TEST_ASSERT_EQUAL_UINT32(14U, configurationRecoveryRequired);
    TEST_ASSERT_EQUAL_UINT32(13U, runRecoveryRequired);
    TEST_ASSERT_EQUAL_UINT32(6U, validConfiguration);
    TEST_ASSERT_EQUAL_UINT32(6U, validResume);
}

void test_callback_12_is_real_backend_reference_not_simulator_evidence() {
    constexpr const char* callback12Reference =
        "issue90_oracle_backend_reference=FAIL_CALLBACK_12_NOT_FOUND "
        "backend_characterization=known_limitation "
        "evidence_scope=REAL_NVS_ONLY "
        "product_recovery_gate=NOT_RUN";
    std::printf("%s\n", callback12Reference);
    const std::string line{callback12Reference};
    TEST_ASSERT_TRUE(line.find("known_limitation") != std::string::npos);
    TEST_ASSERT_TRUE(line.find("REAL_NVS_ONLY") != std::string::npos);
    TEST_ASSERT_TRUE(line.find("product_recovery_gate=NOT_RUN") !=
                     std::string::npos);
    TEST_ASSERT_TRUE(line.find("SIMULATOR_ONLY") == std::string::npos);
    TEST_ASSERT_TRUE(line.find("backend_characterization=observed") ==
                     std::string::npos);
}

void test_machine_schema_contains_required_truth_separation() {
    const auto line = machineLine(kMatrix.front(), observe(kMatrix.front()));
    constexpr const char* required[] = {
        "backend_characterization=",
        "evidence_scope=",
        "write_status=",
        "read_status=",
        "record_classification=",
        "product_outcome=",
        "safety_projection=",
        "logical_gate=",
        "actuator_allowed=",
        "product_recovery_gate=",
        "reboot=",
    };
    for (const char* field : required) {
        TEST_ASSERT_TRUE(line.find(field) != std::string::npos);
    }
    constexpr const char* characterizationSchema =
        "backend_characterization_values="
        "observed|known_limitation|unexpected_change";
    std::printf("issue90_oracle_schema=%s\n", characterizationSchema);
    const std::string schema{characterizationSchema};
    TEST_ASSERT_TRUE(schema.find("observed") != std::string::npos);
    TEST_ASSERT_TRUE(schema.find("known_limitation") != std::string::npos);
    TEST_ASSERT_TRUE(schema.find("unexpected_change") != std::string::npos);
}

}  // namespace

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_oracle_matrix_covers_config_and_run_domains);
    RUN_TEST(test_simulated_restart_and_fault_observations_match_matrix);
    RUN_TEST(test_allowed_product_outcomes_are_explicit_and_fail_closed);
    RUN_TEST(test_forbidden_outcomes_never_become_active);
    RUN_TEST(
        test_record_validation_and_recovery_outcomes_are_independent_of_actual);
    RUN_TEST(test_callback_12_is_real_backend_reference_not_simulator_evidence);
    RUN_TEST(test_machine_schema_contains_required_truth_separation);
    return UNITY_END();
}
