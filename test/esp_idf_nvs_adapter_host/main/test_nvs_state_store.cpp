#include "nvs_state_store.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>

#include "bdl_ramdisk.hpp"
#include "nvs_flash.h"
#include "run_commands.hpp"
#include "run_persistence_codec.hpp"
#include "run_persistence_coordinator.hpp"
#include "safety_core.hpp"
#include "standard_program_catalog.hpp"

namespace {

enum class BlobFault {
    None,
    ErrorOnFirstRead,
    NotFoundOnSecondRead,
    ErrorOnSecondRead,
};
enum class SetFault {
    None,
    SafeWriteError,
    PotentiallyEffectiveError,
};
BlobFault blobFault = BlobFault::None;
SetFault setFault = SetFault::None;
unsigned blobCalls = 0U;
bool failCommit = false;
TestRamDisk* activeBdlDisk = nullptr;
BdlCutPhase rh0CutPhase = BdlCutPhase::None;
bool armRh0Cut = false;
std::string capturedRh0Write;

extern "C" esp_err_t __real_nvs_get_blob(nvs_handle_t, const char*, void*,
                                         size_t*);
extern "C" esp_err_t __real_nvs_set_blob(nvs_handle_t, const char*, const void*,
                                         size_t);
extern "C" esp_err_t __real_nvs_commit(nvs_handle_t);

extern "C" esp_err_t __wrap_nvs_get_blob(nvs_handle_t handle, const char* key,
                                         void* value, size_t* length) {
    ++blobCalls;
    if (value == nullptr && blobFault == BlobFault::ErrorOnFirstRead) {
        return ESP_ERR_NVS_INVALID_LENGTH;
    }
    if (value != nullptr && blobCalls == 2U) {
        if (blobFault == BlobFault::NotFoundOnSecondRead) {
            return ESP_ERR_NVS_NOT_FOUND;
        }
        if (blobFault == BlobFault::ErrorOnSecondRead) {
            return ESP_ERR_NVS_INVALID_LENGTH;
        }
    }
    return __real_nvs_get_blob(handle, key, value, length);
}

extern "C" esp_err_t __wrap_nvs_set_blob(nvs_handle_t handle, const char* key,
                                         const void* value, size_t length) {
    if (setFault == SetFault::SafeWriteError) {
        return ESP_ERR_NVS_INVALID_HANDLE;
    }
    if (setFault == SetFault::PotentiallyEffectiveError) {
        return ESP_ERR_NVS_REMOVE_FAILED;
    }
    if (activeBdlDisk != nullptr) {
        activeBdlDisk->setLogicalKey(key);
        if (std::strcmp(key, "rh0") == 0) {
            capturedRh0Write.assign(static_cast<const char*>(value), length);
            if (armRh0Cut) {
                activeBdlDisk->armCutForNext(BdlOperation::Write, rh0CutPhase);
                armRh0Cut = false;
            }
        }
    }
    const auto result = __real_nvs_set_blob(handle, key, value, length);
    if (activeBdlDisk != nullptr) activeBdlDisk->clearLogicalKey();
    return result;
}

extern "C" esp_err_t __wrap_nvs_commit(nvs_handle_t handle) {
    if (failCommit) {
        return ESP_FAIL;
    }
    return __real_nvs_commit(handle);
}

[[noreturn]] void fail(const char* expression, const char* file, int line) {
    std::fprintf(stderr, "FAIL %s:%d: %s\n", file, line, expression);
    std::exit(1);
}

#define CHECK(expression)                          \
    do {                                           \
        if (!(expression)) {                       \
            fail(#expression, __FILE__, __LINE__); \
        }                                          \
    } while (false)

device_platform::StateStoreKey key(const char* raw) {
    const auto result = device_platform::StateStoreKey::create(raw);
    CHECK(result.key.has_value());
    return *result.key;
}

class PartitionFixture final {
   public:
    explicit PartitionFixture(const char* label, std::size_t size = 0x10000U)
        : label_(label), disk_(size, 0x1000U) {
        CHECK(disk_.handle() != nullptr);
        const auto status =
            nvs_flash_init_partition_bdl(label_, disk_.handle());
        if (status != ESP_OK) {
            std::fprintf(stderr, "BDL init %s: %s\n", label_,
                         esp_err_to_name(status));
        }
        CHECK(status == ESP_OK);
        initialized_ = true;
    }

    ~PartitionFixture() {
        if (initialized_) {
            CHECK(nvs_flash_deinit_partition(label_) == ESP_OK);
        }
    }

    void restart() const {
        CHECK(nvs_flash_deinit_partition(label_) == ESP_OK);
        CHECK(nvs_flash_init_partition_bdl(label_, disk_.handle()) == ESP_OK);
    }

    [[nodiscard]] std::unique_ptr<device_platform_esp_idf::NvsStateStore>
    openStore() const {
        const auto config =
            device_platform_esp_idf::NvsStateStoreConfig::create(
                label_, "fermentation");
        CHECK(config.has_value());
        auto result = device_platform_esp_idf::NvsStateStore::open(*config);
        CHECK(result.status == ESP_OK);
        CHECK(result.store != nullptr);
        return std::move(result.store);
    }

    [[nodiscard]] TestRamDisk& disk() { return disk_; }
    [[nodiscard]] const char* label() const { return label_; }

   private:
    const char* label_;
    TestRamDisk disk_;
    bool initialized_{false};
};

void resetFaults() {
    blobFault = BlobFault::None;
    setFault = SetFault::None;
    blobCalls = 0U;
    failCommit = false;
}

void configValidation() {
    CHECK(device_platform_esp_idf::NvsStateStoreConfig::create("state_store",
                                                               "fermentation")
              .has_value());
    CHECK(device_platform_esp_idf::NvsStateStoreConfig::create("alt-label_01",
                                                               "namespace_01")
              .has_value());
    CHECK(device_platform_esp_idf::NvsStateStoreConfig::create(
              "abcdefghijklmno", "abcdefghijklmno")
              .has_value());
    CHECK(device_platform_esp_idf::NvsStateStoreConfig::create(
              "abcdefghijklmnop", "x")
              .has_value());
    CHECK(!device_platform_esp_idf::NvsStateStoreConfig::create("", "x")
               .has_value());
    CHECK(!device_platform_esp_idf::NvsStateStoreConfig::create(
               std::string(17U, 'p'), "x")
               .has_value());
    CHECK(!device_platform_esp_idf::NvsStateStoreConfig::create(
               "p", std::string(16U, 'n'))
               .has_value());
}

void openFailureIsNotMissingKey() {
    const auto config = device_platform_esp_idf::NvsStateStoreConfig::create(
        "not_initialized", "fermentation");
    CHECK(config.has_value());
    const auto result = device_platform_esp_idf::NvsStateStore::open(*config);
    CHECK(result.status != ESP_OK);
    CHECK(result.status != ESP_ERR_NVS_NOT_FOUND);
}

void binaryWriteReadAndMissingKey() {
    PartitionFixture fixture("adapter_binary");
    auto store = fixture.openStore();
    const auto binary = std::string("a\0b\xFF", 4U);
    CHECK(store->write(key("blob"), binary) ==
          device_platform::StateStoreWriteStatus::Success);
    const auto read = store->read(key("blob"), binary.size());
    CHECK(read.status == device_platform::StateStoreReadStatus::Success);
    CHECK(read.value == binary);
    CHECK(store->read(key("missing"), 32U).status ==
          device_platform::StateStoreReadStatus::NotFound);
}

void readCapacityBoundaries() {
    PartitionFixture fixture("adapt_capacity");
    auto store = fixture.openStore();
    const std::string value(32U, 'x');
    CHECK(store->write(key("blob"), value) ==
          device_platform::StateStoreWriteStatus::Success);
    CHECK(store->read(key("blob"), value.size()).status ==
          device_platform::StateStoreReadStatus::Success);
    CHECK(store->read(key("blob"), value.size() - 1U).status ==
          device_platform::StateStoreReadStatus::CapacityError);
}

void firstReadErrorIsReadError() {
    resetFaults();
    PartitionFixture fixture("adapter_readerr");
    auto store = fixture.openStore();
    const std::string value(32U, 'f');
    CHECK(store->write(key("first_error"), value) ==
          device_platform::StateStoreWriteStatus::Success);
    blobFault = BlobFault::ErrorOnFirstRead;
    const auto result = store->read(key("first_error"), value.size());
    CHECK(result.status == device_platform::StateStoreReadStatus::ReadError);
    CHECK(blobCalls == 1U);
    resetFaults();
}

void secondNotFoundIsReadError() {
    resetFaults();
    PartitionFixture fixture("adapter_race_nf");
    auto store = fixture.openStore();
    const std::string value(32U, 'r');
    CHECK(store->write(key("race"), value) ==
          device_platform::StateStoreWriteStatus::Success);
    blobFault = BlobFault::NotFoundOnSecondRead;
    CHECK(store->read(key("race"), value.size()).status ==
          device_platform::StateStoreReadStatus::ReadError);
    CHECK(blobCalls == 2U);
    resetFaults();
}

void secondOtherErrorIsReadError() {
    resetFaults();
    PartitionFixture fixture("adapt_race_err");
    auto store = fixture.openStore();
    const std::string value(32U, 'e');
    CHECK(store->write(key("race"), value) ==
          device_platform::StateStoreWriteStatus::Success);
    blobFault = BlobFault::ErrorOnSecondRead;
    CHECK(store->read(key("race"), value.size()).status ==
          device_platform::StateStoreReadStatus::ReadError);
    CHECK(blobCalls == 2U);
    resetFaults();
}

void commitErrorIsUnknown() {
    resetFaults();
    PartitionFixture fixture("adapter_commit");
    auto store = fixture.openStore();
    failCommit = true;
    CHECK(store->write(key("commit"), "unknown") ==
          device_platform::StateStoreWriteStatus::CommitOutcomeUnknown);
    resetFaults();
}

void safePreCommitWriteErrorIsWriteError() {
    resetFaults();
    PartitionFixture fixture("adapt_wrerr");
    auto store = fixture.openStore();
    CHECK(store->read(key("missing"), 32U).status ==
          device_platform::StateStoreReadStatus::NotFound);
    setFault = SetFault::SafeWriteError;
    CHECK(store->write(key("writeerr"), "rejected") ==
          device_platform::StateStoreWriteStatus::WriteError);
    store.reset();
    fixture.restart();
    store = fixture.openStore();
    CHECK(store->read(key("writeerr"), 32U).status ==
          device_platform::StateStoreReadStatus::NotFound);

    setFault = SetFault::None;
    const std::string oldValue = "old-persistent-value";
    CHECK(store->write(key("existing"), oldValue) ==
          device_platform::StateStoreWriteStatus::Success);
    setFault = SetFault::SafeWriteError;
    CHECK(store->write(key("existing"), "new-rejected") ==
          device_platform::StateStoreWriteStatus::WriteError);
    store.reset();
    fixture.restart();
    store = fixture.openStore();
    const auto readback = store->read(key("existing"), oldValue.size());
    CHECK(readback.status == device_platform::StateStoreReadStatus::Success);
    CHECK(readback.value == oldValue);
    resetFaults();
}

void potentiallyEffectiveSetErrorIsUnknown() {
    resetFaults();
    PartitionFixture fixture("adapt_setunk");
    auto store = fixture.openStore();
    setFault = SetFault::PotentiallyEffectiveError;
    CHECK(store->write(key("setunknown"), "maybe-written") ==
          device_platform::StateStoreWriteStatus::CommitOutcomeUnknown);
    resetFaults();
}

void preCommitCapacityIsCapacityError() {
    PartitionFixture fixture("adapter_size", 0x20000U);
    auto store = fixture.openStore();
    const std::string tooLarge(130000U, 'c');
    CHECK(store->write(key("large"), tooLarge) ==
          device_platform::StateStoreWriteStatus::CapacityError);
    store.reset();
    fixture.restart();
    store = fixture.openStore();
    CHECK(store->read(key("large"), tooLarge.size()).status ==
          device_platform::StateStoreReadStatus::NotFound);

    const std::string oldValue = "capacity-old";
    CHECK(store->write(key("cap_existing"), oldValue) ==
          device_platform::StateStoreWriteStatus::Success);
    CHECK(store->write(key("cap_existing"), tooLarge) ==
          device_platform::StateStoreWriteStatus::CapacityError);
    store.reset();
    fixture.restart();
    store = fixture.openStore();
    const auto readback = store->read(key("cap_existing"), oldValue.size());
    CHECK(readback.status == device_platform::StateStoreReadStatus::Success);
    CHECK(readback.value == oldValue);
}

void emptyBlobSurvivesRestart() {
    PartitionFixture fixture("adapter_empty");
    auto store = fixture.openStore();
    CHECK(store->write(key("empty"), std::string{}) ==
          device_platform::StateStoreWriteStatus::Success);
    store.reset();
    fixture.restart();
    store = fixture.openStore();
    const auto readback = store->read(key("empty"), 0U);
    CHECK(readback.status == device_platform::StateStoreReadStatus::Success);
    CHECK(readback.value.empty());
}

const char* operationName(BdlOperation operation) {
    switch (operation) {
        case BdlOperation::Read:
            return "read";
        case BdlOperation::Write:
            return "write";
        case BdlOperation::Erase:
            return "erase";
        case BdlOperation::Sync:
            return "sync";
    }
    return "unknown";
}

const char* runLoadStatusName(fermentation::RunPersistenceLoadStatus status) {
    using fermentation::RunPersistenceLoadStatus;
    switch (status) {
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
    return "Unknown";
}

const char* coordinatorStateName(
    fermentation::RunPersistenceCoordinatorState state) {
    using fermentation::RunPersistenceCoordinatorState;
    switch (state) {
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
    return "Unknown";
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
    return "UNKNOWN";
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
    return "Unknown";
}

struct ProductBridgeObservation {
    const char* loadStatus;
    const char* coordinatorState;
    bool persistenceValidated;
    const char* safetyProjection;
    const char* safetyProducer;
    const char* productOutcome;
    const char* logicalGate;
    bool actuatorAllowed;
    bool productRecoveryGate;
};

struct ExpectedProductTruth {
    const char* oracleCaseId;
    const char* productOutcome;
    const char* safetyProjection;
    const char* safetyProducer;
    const char* logicalGate;
    bool actuatorAllowed;
};

// This is an explicit test-only projection of the unchanged, owner-approved
// Slice-2 case `run_rh0_unknown_commit_new`. It is not generated from the
// production result and does not alter the Slice-2 matrix.
constexpr ExpectedProductTruth kUnknownCommitNewExpected{
    "run_rh0_unknown_commit_new",
    "NEW_VALID_RESUME",
    "RESUME_OFFER",
    "None",
    "UNRESOLVED",
    false};

const char* runProductOutcomeName(
    fermentation::RunPersistenceLoadStatus status,
    const fermentation::RunPersistenceSnapshot* snapshot,
    const fermentation::SafetyEvaluation& evaluation) {
    using fermentation::RunPersistenceLoadStatus;
    using fermentation::SafetyBootDisposition;
    if (status == RunPersistenceLoadStatus::Current && snapshot != nullptr) {
        if (evaluation.bootDisposition == SafetyBootDisposition::ResumeOffer)
            return "NEW_VALID_RESUME";
        if (evaluation.bootDisposition == SafetyBootDisposition::Completed)
            return "COMPLETED";
        if (evaluation.bootDisposition == SafetyBootDisposition::TerminalFault)
            return "TERMINAL_FAULT";
        if (evaluation.bootDisposition == SafetyBootDisposition::NoActiveRun)
            return "RUN_ABORT_REQUIRED";
    }
    if (status == RunPersistenceLoadStatus::FallbackRecovered &&
        snapshot != nullptr &&
        evaluation.bootDisposition == SafetyBootDisposition::ResumeOffer)
        return "OLDER_VALID_CHECKPOINT_RESUME";
    if (status == RunPersistenceLoadStatus::NoPersistedRun)
        return "NO_PERSISTED_RUN";
    if (status == RunPersistenceLoadStatus::NoActiveRun) return "NO_ACTIVE_RUN";
    return "RUN_RECOVERY_REQUIRED";
}

ProductBridgeObservation runProductBridge(
    const char* caseId, fermentation::RunPersistenceCoordinator& coordinator,
    const ExpectedProductTruth* expected) {
    const auto loaded = coordinator.loadAndInitialize();
    fermentation::SafetyCore safety;
    safety.beginBoot(device_platform::ResetCause::Unknown);
    fermentation::SafetyCoreInput input;
    input.bootValidationComplete = true;
    input.configurationValidated = true;
    input.configurationRecoveryStatus =
        fermentation::ConfigurationRecoveryStatus::RuntimeReady;
    input.configurationServiceMode =
        fermentation::ConfigurationServiceMode::Operational;
    input.persistenceLoadStatus = loaded.status;
    input.persistenceSnapshot =
        loaded.snapshot.has_value() ? &*loaded.snapshot : nullptr;
    input.persistenceCoordinatorState = coordinator.state();
    input.persistenceValidated =
        loaded.snapshot.has_value() &&
        (loaded.status == fermentation::RunPersistenceLoadStatus::Current ||
         loaded.status ==
             fermentation::RunPersistenceLoadStatus::FallbackRecovered);
    device_platform::SensorQualitySnapshot sensor;
    fermentation::SensorSelectionRuntimeState selection;
    if (loaded.snapshot.has_value()) {
        sensor.quality = device_platform::SensorQuality::Valid;
        selection.permission = fermentation::SensorPeltierPermission::Allowed;
        input.sensorEvidenceValidated = true;
        input.peltierSensor = &sensor;
        input.sensorSelectionRuntime = &selection;
    }
    const auto evaluation = safety.evaluate(input);
    const auto logicalGate =
        evaluation.gate.status ==
                fermentation::ActuatorSafetyGateStatus::Allowed
            ? "ALLOWED"
            : "UNRESOLVED";
    auto observation = ProductBridgeObservation{
        runLoadStatusName(loaded.status),
        coordinatorStateName(coordinator.state()),
        input.persistenceValidated,
        safetyBootDispositionName(evaluation.bootDisposition),
        faultCodeName(evaluation.faultCode),
        runProductOutcomeName(
            loaded.status,
            loaded.snapshot.has_value() ? &*loaded.snapshot : nullptr,
            evaluation),
        logicalGate,
        evaluation.gate.status ==
            fermentation::ActuatorSafetyGateStatus::Allowed,
        false};
    observation.productRecoveryGate =
        expected != nullptr &&
        std::string(observation.productOutcome) == expected->productOutcome &&
        std::string(observation.safetyProjection) ==
            expected->safetyProjection &&
        std::string(observation.safetyProducer) == expected->safetyProducer &&
        std::string(observation.logicalGate) == expected->logicalGate &&
        observation.actuatorAllowed == expected->actuatorAllowed;
    std::printf(
        "issue90_product_bridge_case=%s oracle_case_id=%s "
        "expected_product_outcome=%s expected_safety_projection=%s "
        "expected_safety_producer=%s expected_logical_gate=%s "
        "expected_actuator_allowed=%s actual_load_status=%s "
        "actual_persistence_validated=%s actual_coordinator_state=%s "
        "actual_safety_projection=%s "
        "actual_safety_producer=%s actual_product_outcome=%s "
        "actual_logical_gate=%s actual_actuator_allowed=%s "
        "product_recovery_gate=%s comparison_result=%s\n",
        caseId, expected == nullptr ? "NOT_APPLICABLE" : expected->oracleCaseId,
        expected == nullptr ? "NOT_RUN" : expected->productOutcome,
        expected == nullptr ? "NOT_RUN" : expected->safetyProjection,
        expected == nullptr ? "NOT_RUN" : expected->safetyProducer,
        expected == nullptr ? "NOT_RUN" : expected->logicalGate,
        expected == nullptr ? "NOT_RUN"
                            : (expected->actuatorAllowed ? "true" : "false"),
        observation.loadStatus,
        observation.persistenceValidated ? "true" : "false",
        observation.coordinatorState, observation.safetyProjection,
        observation.safetyProducer, observation.productOutcome,
        observation.logicalGate, observation.actuatorAllowed ? "true" : "false",
        expected == nullptr
            ? "NOT_RUN"
            : (observation.productRecoveryGate ? "PASS" : "FAIL"),
        expected == nullptr
            ? "NOT_RUN"
            : (observation.productRecoveryGate ? "PASS" : "FAIL"));
    return observation;
}

const char* readStatusName(device_platform::StateStoreReadStatus status) {
    using device_platform::StateStoreReadStatus;
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
    return "Unknown";
}

const char* coordinatorResultName(
    fermentation::RunPersistenceResultStatus status) {
    using fermentation::RunPersistenceResultStatus;
    switch (status) {
        case RunPersistenceResultStatus::Applied:
            return "Applied";
        case RunPersistenceResultStatus::CheckpointWritten:
            return "CheckpointWritten";
        case RunPersistenceResultStatus::PersistenceIndeterminate:
            return "PersistenceIndeterminate";
        case RunPersistenceResultStatus::WriteFailed:
            return "WriteFailed";
        case RunPersistenceResultStatus::CapacityExceeded:
            return "CapacityExceeded";
        case RunPersistenceResultStatus::NotDue:
            return "NotDue";
        default:
            return "Other";
    }
}

std::string bytesIdentity(const std::string& bytes) {
    std::uint32_t hash = 2166136261U;
    for (const unsigned char value : bytes) {
        hash ^= value;
        hash *= 16777619U;
    }
    std::ostringstream result;
    result << "fnv1a32:" << std::hex << hash;
    return result.str();
}

const BdlEvent* findRh0Event(const std::vector<BdlEvent>& events,
                             bool requireCut) {
    const auto found = std::find_if(
        events.rbegin(), events.rend(), [requireCut](const BdlEvent& event) {
            return event.operation == BdlOperation::Write &&
                   event.logicalKey == "rh0" &&
                   (!requireCut || event.cutPhase != BdlCutPhase::None);
        });
    if (found == events.rend()) return nullptr;
    return &*found;
}

void writeCutArtifact(const char* caseId, const char* label,
                      const char* stimulusKind, const char* writeStatus,
                      const BdlEvent& targetEvent, const std::string& oldHead,
                      const std::string& newHead, const std::string& afterImage,
                      const device_platform::StateStoreReadResult& headRead,
                      const TestRamDisk& disk,
                      const fermentation::RunPersistenceResult& checkpoint,
                      const ExpectedProductTruth* expected,
                      const ProductBridgeObservation* bridge) {
    const std::filesystem::path directory = "/tmp/issue90-slice5-artifacts";
    std::error_code error;
    std::filesystem::create_directories(directory, error);
    CHECK(!error);
    std::ofstream artifact(directory / (std::string(caseId) + ".json"));
    CHECK(artifact.good());
    artifact
        << "{\n"
        << "  \"source_sha\": \""
        << (std::getenv("ISSUE90_SOURCE_SHA") == nullptr
                ? "UNKNOWN"
                : std::getenv("ISSUE90_SOURCE_SHA"))
        << "\",\n"
        << "  \"esp_idf_tag\": \"v6.0.2\",\n"
        << "  \"esp_idf_commit\": "
           "\"7101770dc6db2667b3c477cc31365dd1acd6db4e\",\n"
        << "  \"case_id\": \"" << caseId << "\",\n"
        << "  \"stimulus_kind\": \"" << stimulusKind << "\",\n"
        << "  \"partition_backend\": \"" << label << "\",\n"
        << "  \"namespace\": \"fermentation\",\n"
        << "  \"logical_operation\": \"run_checkpoint_head_write\",\n"
        << "  \"logical_key\": \"rh0\",\n"
        << "  \"actual_target_key\": \"" << targetEvent.logicalKey << "\",\n"
        << "  \"old_identity\": \"" << bytesIdentity(oldHead) << "\",\n"
        << "  \"new_identity\": \"" << bytesIdentity(newHead) << "\",\n"
        << "  \"old_head_bytes_ne_new_head_bytes\": "
        << (oldHead != newHead ? "true" : "false") << ",\n"
        << "  \"target_bdl_operation\": \"write\",\n"
        << "  \"target_occurrence\": " << targetEvent.occurrence << ",\n"
        << "  \"target_offset\": " << targetEvent.offset << ",\n"
        << "  \"target_length\": " << targetEvent.length << ",\n"
        << "  \"blob_size\": " << newHead.size() << ",\n"
        << "  \"baseline_checksum_immediately_before_target\": "
        << targetEvent.baselineChecksum << ",\n"
        << "  \"after_image_checksum\": \"" << bytesIdentity(afterImage)
        << "\",\n"
        << "  \"injected_fault_or_cut\": \"" << stimulusKind << "\",\n"
        << "  \"write_status\": \"" << writeStatus << "\",\n"
        << "  \"coordinator_result\": \""
        << coordinatorResultName(checkpoint.status) << "\",\n"
        << "  \"reinit_status\": \"Success\",\n"
        << "  \"product_visible_read_status\": \""
        << readStatusName(headRead.status) << "\",\n"
        << "  \"product_visible_bytes_identity\": \""
        << (headRead.status == device_platform::StateStoreReadStatus::Success
                ? bytesIdentity(headRead.value)
                : "NOT_VISIBLE")
        << "\",\n"
        << "  \"actual_persistence_validated\": "
        << (bridge != nullptr && bridge->persistenceValidated ? "true"
                                                              : "false")
        << ",\n"
        << "  \"oracle_case_id\": \""
        << (expected == nullptr ? "NOT_APPLICABLE" : expected->oracleCaseId)
        << "\",\n"
        << "  \"expected_product_outcome\": \""
        << (expected == nullptr ? "NOT_RUN" : expected->productOutcome)
        << "\",\n"
        << "  \"actual_product_outcome\": \""
        << (bridge == nullptr ? "NOT_RUN" : bridge->productOutcome) << "\",\n"
        << "  \"expected_safety_projection\": \""
        << (expected == nullptr ? "NOT_RUN" : expected->safetyProjection)
        << "\",\n"
        << "  \"actual_safety_projection\": \""
        << (bridge == nullptr ? "NOT_RUN" : bridge->safetyProjection) << "\",\n"
        << "  \"expected_safety_producer\": \""
        << (expected == nullptr ? "NOT_RUN" : expected->safetyProducer)
        << "\",\n"
        << "  \"actual_safety_producer\": \""
        << (bridge == nullptr ? "NOT_RUN" : bridge->safetyProducer) << "\",\n"
        << "  \"expected_logical_gate\": \""
        << (expected == nullptr ? "NOT_RUN" : expected->logicalGate) << "\",\n"
        << "  \"actual_logical_gate\": \""
        << (bridge == nullptr ? "NOT_RUN" : bridge->logicalGate) << "\",\n"
        << "  \"expected_actuator_allowed\": "
        << (expected != nullptr && expected->actuatorAllowed ? "true" : "false")
        << ",\n"
        << "  \"actual_actuator_allowed\": "
        << (bridge != nullptr && bridge->actuatorAllowed ? "true" : "false")
        << ",\n"
        << "  \"backend_classification\": \"observed\",\n"
        << "  \"product_recovery_gate\": \""
        << (expected == nullptr
                ? "NOT_RUN"
                : (bridge != nullptr && bridge->productRecoveryGate ? "PASS"
                                                                    : "FAIL"))
        << "\",\n"
        << "  \"result\": \""
        << (expected == nullptr
                ? "NOT_RUN"
                : (bridge != nullptr && bridge->productRecoveryGate ? "PASS"
                                                                    : "FAIL"))
        << "\",\n"
        << "  \"events\": [\n";
    const auto events = disk.events();
    for (std::size_t index = 0U; index < events.size(); ++index) {
        const auto& event = events[index];
        artifact << "    {\"operation\": \"" << operationName(event.operation)
                 << "\", \"offset\": " << event.offset
                 << ", \"length\": " << event.length
                 << ", \"occurrence\": " << event.occurrence
                 << ", \"result\": " << event.result << ", \"logical_key\": \""
                 << event.logicalKey
                 << "\", \"baseline_checksum\": " << event.baselineChecksum
                 << "}" << (index + 1U == events.size() ? "\n" : ",\n");
    }
    artifact << "  ]\n}\n";
    CHECK(artifact.good());
}

void cutHarnessWritesEarlyAndLateArtifacts() {
    const auto run = [](const char* caseId, const char* label,
                        BdlCutPhase phase) {
        resetFaults();
        PartitionFixture fixture(label);
        auto store = fixture.openStore();
        fermentation::RunPersistenceCoordinator coordinator(
            *store, device_platform::StorageEpoch{1U},
            fermentation::RunCheckpointSchedule{});
        CHECK(coordinator.loadAndInitialize().status ==
              fermentation::RunPersistenceLoadStatus::NoPersistedRun);
        fermentation::RunCommandState state;
        state.processState.state = fermentation::ProcessState::Standby;
        auto program = fermentation::FactoryProgramCatalog::find("water-kefir");
        CHECK(program.has_value());
        program->program.productSensorFailure.fallbackDelaySeconds = 30U;
        program->program.fermentationStages.front().targetTemperatureCelsius =
            38.0;
        program->program.fermentationStages.front().durationMinutes = 120U;
        program->program.targetQualification.bandCelsius = 0.5;
        program->program.targetQualification.durationMinutes = 10U;
        program->program.maximumTargetReachMinutes = 180U;
        program->program.preheat = true;
        program->program.maximumProductWaitMinutes = 30U;
        CHECK(fermentation::validateProgram(*program).valid());
        fermentation::ProgramStartRequest request;
        request.envelope = {901U,
                            fermentation::CommandSource::LocalDisplay,
                            100U,
                            state.processState.transitionSequence,
                            state.runRevision,
                            std::nullopt,
                            std::nullopt,
                            true,
                            std::nullopt};
        request.runId = "slice5-run";
        request.program = *program;
        request.sourceKind = fermentation::ProgramSourceKind::FactoryCatalog;
        request.sourceProgramRevision = 1U;
        request.sensorMode = fermentation::RunSensorMode::Product;
        request.safetyAllowsStart = true;
        request.airSensorValid = true;
        request.coolingSensorValid = true;
        request.productSensorValid = true;
        const auto start = fermentation::decideProgramStart(state, request);
        CHECK(start.proposed());
        CHECK(coordinator
                  .persistCommand(
                      state, start,
                      fermentation::RunCheckpointTime{100U, std::nullopt})
                  .status == fermentation::RunPersistenceResultStatus::Applied);
        const auto oldHeadRead = store->read(key("rh0"), 256U);
        CHECK(oldHeadRead.status ==
              device_platform::StateStoreReadStatus::Success);
        const auto oldHead = oldHeadRead.value;
        CHECK(!oldHead.empty());
        fixture.disk().clearEvents();
        capturedRh0Write.clear();
        activeBdlDisk = &fixture.disk();
        rh0CutPhase = phase;
        armRh0Cut = true;
        const auto checkpoint = coordinator.checkpointPeriodic(
            state, fermentation::RunCheckpointTime{300100U, std::nullopt});
        armRh0Cut = false;
        activeBdlDisk = nullptr;
        CHECK(!capturedRh0Write.empty());
        CHECK(oldHead != capturedRh0Write);
        store.reset();
        fixture.disk().clearCutPlan();
        fixture.restart();
        store = fixture.openStore();
        const auto headRead = store->read(key("rh0"), 256U);
        CHECK(headRead.status ==
              device_platform::StateStoreReadStatus::Success);
        const auto head = fermentation::decodeRunPersistenceHead(
            headRead.value, device_platform::StorageEpoch{1U});
        CHECK(head.has_value());
        CHECK(head->state == fermentation::RunPersistenceHeadState::Committed);
        const auto currentBytes =
            store->read(key(head->current.slot == 0U ? "rc0" : "rc1"), 8240U);
        CHECK(currentBytes.status ==
              device_platform::StateStoreReadStatus::Success);
        const auto currentRecord = fermentation::decodeRunPersistenceRecord(
            currentBytes.value, device_platform::StorageEpoch{1U});
        CHECK(currentRecord.has_value());
        CHECK(fermentation::runCheckpointReferenceMatches(
            head->current, *currentRecord, head->current.slot));
        const bool exactNewHeadVisible = headRead.value == capturedRh0Write;
        if (exactNewHeadVisible) {
            CHECK(head->fallback.has_value());
            const auto fallbackBytes = store->read(
                key(head->fallback->slot == 0U ? "rc0" : "rc1"), 8240U);
            CHECK(fallbackBytes.status ==
                  device_platform::StateStoreReadStatus::Success);
            const auto fallbackRecord =
                fermentation::decodeRunPersistenceRecord(
                    fallbackBytes.value, device_platform::StorageEpoch{1U});
            CHECK(fallbackRecord.has_value());
            CHECK(fermentation::runCheckpointReferenceMatches(
                *head->fallback, *fallbackRecord, head->fallback->slot));
            CHECK(fallbackRecord->snapshot.activeRunId ==
                  currentRecord->snapshot.activeRunId);
            CHECK(fallbackRecord->checkpointRevision <
                  currentRecord->checkpointRevision);
        } else {
            CHECK(!head->fallback.has_value());
            const auto orphanSlot = 1U - head->current.slot;
            const auto orphanBytes =
                store->read(key(orphanSlot == 0U ? "rc0" : "rc1"), 8240U);
            CHECK(orphanBytes.status ==
                  device_platform::StateStoreReadStatus::Success);
            CHECK(fermentation::decodeRunPersistenceRecord(
                      orphanBytes.value, device_platform::StorageEpoch{1U})
                      .has_value());
        }
        const bool requiresCut = phase != BdlCutPhase::None;
        const auto events = fixture.disk().events();
        const auto* targetEvent = findRh0Event(events, requiresCut);
        CHECK(targetEvent != nullptr);
        const ExpectedProductTruth* expected =
            exactNewHeadVisible ? &kUnknownCommitNewExpected : nullptr;
        fermentation::RunPersistenceCoordinator bridgeCoordinator(
            *store, device_platform::StorageEpoch{1U},
            fermentation::RunCheckpointSchedule{});
        const auto bridge =
            expected == nullptr
                ? ProductBridgeObservation{"NOT_RUN",    "NOT_RUN", false,
                                           "NOT_RUN",    "NOT_RUN", "NOT_RUN",
                                           "UNRESOLVED", false,     false}
                : runProductBridge(caseId, bridgeCoordinator, expected);
        writeCutArtifact(
            caseId, fixture.label(),
            phase == BdlCutPhase::Before  ? "BDL_IO_FAILURE_BEFORE"
            : phase == BdlCutPhase::After ? "BDL_IO_FAILURE_AFTER"
                                          : "BDL_COMMIT_SUCCESS",
            phase == BdlCutPhase::None ? "Success" : "CommitOutcomeUnknown",
            *targetEvent, oldHead, capturedRh0Write, headRead.value, headRead,
            fixture.disk(), checkpoint, expected, &bridge);
        if (!exactNewHeadVisible) {
            std::printf(
                "issue90_product_bridge_case=%s oracle_case_id=NOT_APPLICABLE "
                "product_recovery_gate=NOT_RUN comparison_result=NOT_RUN "
                "reason=BDL_IO_FAILURE_DID_NOT_EXPOSE_EXACT_NEW_HEAD\n",
                caseId);
        }
        if (expected != nullptr) CHECK(bridge.productRecoveryGate);
    };

    run("bdl_io_failure_before_rh0", "cut_early", BdlCutPhase::Before);
    run("bdl_io_failure_after_rh0", "cut_late", BdlCutPhase::After);
    run("bdl_committed_rh0_product_bridge", "bridge_commit", BdlCutPhase::None);
}

void productRecordSizeAndMutationMatrix() {
    PartitionFixture fixture("adapter_matrix", 0x20000U);
    auto store = fixture.openStore();
    CHECK(store->write(key("overwrite"), "old") ==
          device_platform::StateStoreWriteStatus::Success);
    CHECK(store->write(key("overwrite"), "new") ==
          device_platform::StateStoreWriteStatus::Success);
    CHECK(store->read(key("overwrite"), 3U).value == "new");
    for (const std::size_t size : {1U, 32U, 1024U, 4096U, 8192U, 8240U}) {
        const auto recordKey = key(("k" + std::to_string(size)).c_str());
        const std::string value(size, static_cast<char>(size & 0x7FU));
        CHECK(store->write(recordKey, value) ==
              device_platform::StateStoreWriteStatus::Success);
        CHECK(store->read(recordKey, size).status ==
              device_platform::StateStoreReadStatus::Success);
    }
}

bool gcEraseCharacterization() {
    PartitionFixture fixture("adapter_gc", 0x20000U);
    auto store = fixture.openStore();
    fixture.disk().clearEvents();
    bool writeAttempted = false;
    for (std::size_t revision = 0U; revision < 256U; ++revision) {
        std::string value(8240U, static_cast<char>(revision & 0x7FU));
        value[0] = static_cast<char>((revision + 1U) & 0x7FU);
        writeAttempted = true;
        const auto status = store->write(key("gc_window"), value);
        if (status != device_platform::StateStoreWriteStatus::Success) break;
    }
    CHECK(writeAttempted);
    const auto events = fixture.disk().events();
    const bool eraseObserved =
        std::any_of(events.begin(), events.end(), [](const BdlEvent& event) {
            return event.operation == BdlOperation::Erase;
        });
    std::printf(
        "issue90_gc_erase_events=%s write_events=%zu erase_events=%zu\n",
        eraseObserved ? "OBSERVED" : "NOT_OBSERVED",
        static_cast<std::size_t>(std::count_if(events.begin(), events.end(),
                                               [](const BdlEvent& event) {
                                                   return event.operation ==
                                                          BdlOperation::Write;
                                               })),
        static_cast<std::size_t>(std::count_if(
            events.begin(), events.end(), [](const BdlEvent& event) {
                return event.operation == BdlOperation::Erase;
            })));
    return eraseObserved;
}

}  // namespace

extern "C" void app_main(void) {
    configValidation();
    openFailureIsNotMissingKey();
    binaryWriteReadAndMissingKey();
    readCapacityBoundaries();
    firstReadErrorIsReadError();
    secondNotFoundIsReadError();
    secondOtherErrorIsReadError();
    commitErrorIsUnknown();
    safePreCommitWriteErrorIsWriteError();
    potentiallyEffectiveSetErrorIsUnknown();
    preCommitCapacityIsCapacityError();
    emptyBlobSurvivesRestart();
    productRecordSizeAndMutationMatrix();
    const bool gcEraseObserved = gcEraseCharacterization();
    cutHarnessWritesEarlyAndLateArtifacts();
    std::puts("Issue90 NVS adapter host tests: 14/14 PASS");
    std::puts(
        "issue90_backend_characterization=observed "
        "evidence_scope=ESP_IDF_BDL_HOST product_recovery_gate=NOT_RUN");
    std::puts(
        "issue90_product_bridge_summary=PASS:1 NOT_RUN:2 "
        "callback_12_excluded=true");
    std::puts(
        "issue90_backend_matrix=small_blob,same_key_overwrite,medium_blob,page_"
        "boundary,product_record_8240,new_key,existing_key,commit,readback_"
        "capacity,empty_blob,bdl_io_failure_before_rh0,"
        "bdl_io_failure_after_rh0,bdl_committed_rh0_product_bridge,"
        "erase_window,gc_window");
    std::printf(
        "issue90_backend_matrix_completeness=PARTIAL exhaustive=false "
        "gc_erase_characterization=%s gc_erase_cut=NOT_RUN "
        "power_cut_harness=POWER_CUT_HARNESS_BLOCKED "
        "power_cut_evidence=NOT_RUN "
        "power_cut_block_reason=BDL_IO_FAILURE_RETURNS_TO_ESP_IDF_PROCESS\n",
        gcEraseObserved ? "OBSERVED" : "GC_ERASE_CHARACTERIZATION_BLOCKED");
    std::puts(
        "callback_12=FAIL_CALLBACK_12_NOT_FOUND "
        "backend_characterization=known_limitation "
        "evidence_scope=REAL_NVS_ONLY product_recovery_gate=NOT_RUN");
    std::exit(0);
}
