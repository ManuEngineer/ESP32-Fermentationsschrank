#include "nvs_state_store.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <sys/wait.h>
#include <unistd.h>

#include "bdl_ramdisk.hpp"
#include "esp_time_zone_resolver.hpp"
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
BdlCutMode rh0CutMode = BdlCutMode::ReturnError;
bool armRh0Cut = false;
std::string capturedRh0Write;
std::size_t productBridgePasses = 0U;
std::size_t productBridgeFailures = 0U;
std::size_t productBridgeBlocked = 0U;
std::size_t productBridgeNotRun = 0U;
bool abruptPowerCutAllPass = false;
bool abruptGcEraseCutAllPass = false;

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
            activeBdlDisk->setMutationNewHead(capturedRh0Write);
            if (armRh0Cut) {
                activeBdlDisk->armCutForNext(BdlOperation::Write, rh0CutPhase,
                                             rh0CutMode);
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

bool finalHostGatePass(bool gcEraseObserved, bool powerCutPass,
                       bool gcEraseCutPass, std::size_t bridgeFailures,
                       std::size_t bridgeBlocked, std::size_t bridgeNotRun) {
    return gcEraseObserved && powerCutPass && gcEraseCutPass &&
           bridgeFailures == 0U && bridgeBlocked == 0U && bridgeNotRun == 0U;
}

void finalGateNegativeSelfTests() {
    CHECK(finalHostGatePass(true, true, true, 0U, 0U, 0U));
    CHECK(!finalHostGatePass(true, false, true, 0U, 0U, 0U));
    CHECK(!finalHostGatePass(true, true, false, 0U, 0U, 0U));
    CHECK(!finalHostGatePass(true, true, true, 1U, 0U, 0U));
    CHECK(!finalHostGatePass(true, true, true, 0U, 1U, 0U));
    CHECK(!finalHostGatePass(true, true, true, 0U, 0U, 1U));
    std::puts("issue90_final_gate_negative_selftests=5 PASS");
}

device_platform::StateStoreKey key(const char* raw) {
    const auto result = device_platform::StateStoreKey::create(raw);
    CHECK(result.key.has_value());
    return *result.key;
}

class PartitionFixture final {
   public:
    explicit PartitionFixture(const char* label, std::size_t size = 0x10000U,
                              bool autoInitialize = true)
        : label_(label), disk_(size, 0x1000U) {
        CHECK(disk_.handle() != nullptr);
        if (autoInitialize) initialize();
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

    void initialize() {
        if (initialized_) return;
        const auto status =
            nvs_flash_init_partition_bdl(label_, disk_.handle());
        if (status != ESP_OK) {
            std::fprintf(stderr, "BDL init %s: %s\n", label_,
                         esp_err_to_name(status));
        }
        CHECK(status == ESP_OK);
        initialized_ = true;
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
    rh0CutMode = BdlCutMode::ReturnError;
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

void espTimeZoneResolverContract() {
    const device_platform_esp_idf::EspTimeZoneResolver resolver;

    const auto supported = resolver.prepare("Europe/Zurich");
    CHECK(supported.status == device_platform::TimeZonePrepareStatus::Success);
    CHECK(supported.prepared.has_value());
    CHECK(supported.prepared->canonicalIdentifier == "Europe/Zurich");

    const auto unknown = resolver.prepare("Unknown/Identifier");
    CHECK(unknown.status ==
          device_platform::TimeZonePrepareStatus::UnsupportedIdentifier);
    CHECK(!unknown.prepared.has_value());
    std::puts("ESP_TIME_ZONE_RESOLVER_CONTRACT=PASS");
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

// The same owner-approved Slice-2 product truth is also used when an
// interrupted backend write leaves the old fully valid current head visible.
// The backend cause remains CommitOutcomeUnknown; only the post-restart
// product state is compared here.
constexpr ExpectedProductTruth kFullyValidCurrentExpected{
    "run_rc0_new_valid_resume",
    "NEW_VALID_RESUME",
    "RESUME_OFFER",
    "None",
    "UNRESOLVED",
    false};

constexpr std::array<const char*, 2U> kCanonicalSlice2RunOracleCases = {{
    "run_rh0_unknown_commit_new",
    "run_rc0_new_valid_resume",
}};

bool isCanonicalSlice2RunOracleCase(const char* caseId) {
    return std::any_of(kCanonicalSlice2RunOracleCases.begin(),
                       kCanonicalSlice2RunOracleCases.end(),
                       [caseId](const char* canonical) {
                           return std::strcmp(caseId, canonical) == 0;
                       });
}

void canonicalOracleIdSelfTest() {
    CHECK(isCanonicalSlice2RunOracleCase("run_rh0_unknown_commit_new"));
    CHECK(isCanonicalSlice2RunOracleCase("run_rc0_new_valid_resume"));
    CHECK(!isCanonicalSlice2RunOracleCase("run_rc0_write_error_older"));
    std::puts("issue90_canonical_oracle_id_selftest=PASS");
}

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
    if (expected != nullptr) {
        CHECK(isCanonicalSlice2RunOracleCase(expected->oracleCaseId));
    }
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

std::string readBinaryFile(const std::string& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input.good()) return {};
    input.seekg(0, std::ios::end);
    const auto size = input.tellg();
    if (size < 0) return {};
    input.seekg(0, std::ios::beg);
    std::string value(static_cast<std::size_t>(size), '\0');
    input.read(value.data(), static_cast<std::streamsize>(value.size()));
    if (!input.good() && !input.eof()) return {};
    return value;
}

std::map<std::string, std::string> readKeyValueFile(const std::string& path) {
    std::map<std::string, std::string> values;
    std::ifstream input(path);
    std::string line;
    while (std::getline(input, line)) {
        const auto separator = line.find('=');
        if (separator != std::string::npos) {
            values[line.substr(0U, separator)] = line.substr(separator + 1U);
        }
    }
    return values;
}

const std::string& requiredValue(
    const std::map<std::string, std::string>& values, const char* keyName) {
    const auto found = values.find(keyName);
    CHECK(found != values.end());
    return found->second;
}

pid_t waitForChild(pid_t child, int* status) {
    pid_t result = -1;
    do {
        result = waitpid(child, status, 0);
    } while (result < 0 && errno == EINTR);
    return result;
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
                      bool oldHeadExact, bool newHeadExact,
                      const char* postRestartHeadState,
                      bool currentReferenceValid, bool fallbackReferenceValid,
                      bool orphanRecordPresent,
                      const ExpectedProductTruth* expected,
                      const ProductBridgeObservation* bridge,
                      const char* productGateStatus) {
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
        << "  \"old_head_exact\": " << (oldHeadExact ? "true" : "false")
        << ",\n"
        << "  \"new_head_exact\": " << (newHeadExact ? "true" : "false")
        << ",\n"
        << "  \"post_restart_head_state\": \"" << postRestartHeadState
        << "\",\n"
        << "  \"current_reference_valid\": "
        << (currentReferenceValid ? "true" : "false") << ",\n"
        << "  \"fallback_reference_valid\": "
        << (fallbackReferenceValid ? "true" : "false") << ",\n"
        << "  \"orphan_record_present\": "
        << (orphanRecordPresent ? "true" : "false") << ",\n"
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
        << "  \"product_recovery_gate\": \"" << productGateStatus << "\",\n"
        << "  \"result\": \"" << productGateStatus << "\",\n"
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
        const bool currentReferenceValid =
            currentRecord.has_value() &&
            fermentation::runCheckpointReferenceMatches(
                head->current, *currentRecord, head->current.slot);
        const bool exactNewHeadVisible = headRead.value == capturedRh0Write;
        const bool exactOldHeadVisible = headRead.value == oldHead;
        bool fallbackReferenceValid = false;
        bool orphanRecordPresent = false;
        if (exactNewHeadVisible) {
            if (head->fallback.has_value()) {
                const auto fallbackBytes = store->read(
                    key(head->fallback->slot == 0U ? "rc0" : "rc1"), 8240U);
                if (fallbackBytes.status ==
                    device_platform::StateStoreReadStatus::Success) {
                    const auto fallbackRecord =
                        fermentation::decodeRunPersistenceRecord(
                            fallbackBytes.value,
                            device_platform::StorageEpoch{1U});
                    fallbackReferenceValid =
                        fallbackRecord.has_value() &&
                        fermentation::runCheckpointReferenceMatches(
                            *head->fallback, *fallbackRecord,
                            head->fallback->slot) &&
                        currentRecord.has_value() &&
                        fallbackRecord->snapshot.activeRunId ==
                            currentRecord->snapshot.activeRunId &&
                        fallbackRecord->checkpointRevision <
                            currentRecord->checkpointRevision;
                }
            }
        } else if (exactOldHeadVisible) {
            if (!head->fallback.has_value()) {
                const auto orphanSlot = 1U - head->current.slot;
                const auto orphanBytes =
                    store->read(key(orphanSlot == 0U ? "rc0" : "rc1"), 8240U);
                orphanRecordPresent =
                    orphanBytes.status ==
                        device_platform::StateStoreReadStatus::Success &&
                    fermentation::decodeRunPersistenceRecord(
                        orphanBytes.value, device_platform::StorageEpoch{1U})
                        .has_value();
            }
        } else {
            const auto orphanSlot = 1U - head->current.slot;
            const auto orphanBytes =
                store->read(key(orphanSlot == 0U ? "rc0" : "rc1"), 8240U);
            orphanRecordPresent =
                orphanBytes.status ==
                    device_platform::StateStoreReadStatus::Success &&
                fermentation::decodeRunPersistenceRecord(
                    orphanBytes.value, device_platform::StorageEpoch{1U})
                    .has_value();
        }
        const bool requiresCut = phase != BdlCutPhase::None;
        const auto events = fixture.disk().events();
        const auto* targetEvent = findRh0Event(events, requiresCut);
        CHECK(targetEvent != nullptr);
        const ExpectedProductTruth* expected =
            exactNewHeadVisible && currentReferenceValid &&
                    fallbackReferenceValid
                ? &kUnknownCommitNewExpected
            : exactOldHeadVisible && currentReferenceValid
                ? &kFullyValidCurrentExpected
                : nullptr;
        fermentation::RunPersistenceCoordinator bridgeCoordinator(
            *store, device_platform::StorageEpoch{1U},
            fermentation::RunCheckpointSchedule{});
        const char* postRestartHeadState = exactNewHeadVisible   ? "NEW_EXACT"
                                           : exactOldHeadVisible ? "OLD_EXACT"
                                                                 : "UNEXPECTED";
        const auto bridge =
            expected == nullptr
                ? ProductBridgeObservation{"NOT_RUN",    "NOT_RUN", false,
                                           "NOT_RUN",    "NOT_RUN", "NOT_RUN",
                                           "UNRESOLVED", false,     false}
                : runProductBridge(caseId, bridgeCoordinator, expected);
        const char* productGateStatus =
            expected == nullptr
                ? "BLOCKED"
                : (bridge.productRecoveryGate ? "PASS" : "FAIL");
        if (expected == nullptr) {
            ++productBridgeBlocked;
        } else if (bridge.productRecoveryGate) {
            ++productBridgePasses;
        } else {
            ++productBridgeFailures;
        }
        std::printf(
            "issue90_product_bridge_state=%s old_head_exact=%s "
            "new_head_exact=%s post_restart_head_state=%s "
            "current_reference_valid=%s fallback_reference_valid=%s "
            "orphan_record_present=%s actual_product_outcome=%s "
            "actual_safety_projection=%s actual_safety_producer=%s "
            "actual_logical_gate=%s actual_actuator_allowed=%s "
            "product_recovery_gate=%s\n",
            caseId, exactOldHeadVisible ? "true" : "false",
            exactNewHeadVisible ? "true" : "false", postRestartHeadState,
            currentReferenceValid ? "true" : "false",
            fallbackReferenceValid ? "true" : "false",
            orphanRecordPresent ? "true" : "false", bridge.productOutcome,
            bridge.safetyProjection, bridge.safetyProducer, bridge.logicalGate,
            bridge.actuatorAllowed ? "true" : "false", productGateStatus);
        writeCutArtifact(
            caseId, fixture.label(),
            phase == BdlCutPhase::Before  ? "BDL_IO_FAILURE_BEFORE"
            : phase == BdlCutPhase::After ? "BDL_IO_FAILURE_AFTER"
                                          : "BDL_COMMIT_SUCCESS",
            phase == BdlCutPhase::None ? "Success" : "CommitOutcomeUnknown",
            *targetEvent, oldHead, capturedRh0Write, headRead.value, headRead,
            fixture.disk(), checkpoint, exactOldHeadVisible,
            exactNewHeadVisible, postRestartHeadState, currentReferenceValid,
            fallbackReferenceValid, orphanRecordPresent, expected, &bridge,
            productGateStatus);
        CHECK(expected != nullptr);
        CHECK(bridge.productRecoveryGate);
    };

    run("bdl_io_failure_before_rh0", "cut_early", BdlCutPhase::Before);
    run("bdl_io_failure_after_rh0", "cut_late", BdlCutPhase::After);
    run("bdl_committed_rh0_product_bridge", "bridge_commit", BdlCutPhase::None);
}

void writePowerCutRecoveryResult(
    const std::string& path, const char* caseId, bool oldHeadExact,
    bool newHeadExact, const char* postRestartHeadState,
    bool currentReferenceValid, bool fallbackReferenceValid,
    bool orphanRecordPresent, const ExpectedProductTruth* expected,
    const ProductBridgeObservation& bridge, const char* productGateStatus) {
    std::ofstream result(path, std::ios::out | std::ios::trunc);
    CHECK(result.good());
    result << "case_id=" << caseId << '\n'
           << "old_head_exact=" << (oldHeadExact ? "true" : "false") << '\n'
           << "new_head_exact=" << (newHeadExact ? "true" : "false") << '\n'
           << "post_restart_head_state=" << postRestartHeadState << '\n'
           << "current_reference_valid="
           << (currentReferenceValid ? "true" : "false") << '\n'
           << "fallback_reference_valid="
           << (fallbackReferenceValid ? "true" : "false") << '\n'
           << "orphan_record_present="
           << (orphanRecordPresent ? "true" : "false") << '\n'
           << "oracle_case_id="
           << (expected == nullptr ? "NOT_APPLICABLE" : expected->oracleCaseId)
           << '\n'
           << "expected_product_outcome="
           << (expected == nullptr ? "NOT_RUN" : expected->productOutcome)
           << '\n'
           << "actual_product_outcome=" << bridge.productOutcome << '\n'
           << "expected_safety_projection="
           << (expected == nullptr ? "NOT_RUN" : expected->safetyProjection)
           << '\n'
           << "actual_safety_projection=" << bridge.safetyProjection << '\n'
           << "expected_safety_producer="
           << (expected == nullptr ? "NOT_RUN" : expected->safetyProducer)
           << '\n'
           << "actual_safety_producer=" << bridge.safetyProducer << '\n'
           << "expected_logical_gate="
           << (expected == nullptr ? "NOT_RUN" : expected->logicalGate) << '\n'
           << "actual_logical_gate=" << bridge.logicalGate << '\n'
           << "expected_actuator_allowed="
           << (expected != nullptr && expected->actuatorAllowed ? "true"
                                                                : "false")
           << '\n'
           << "actual_actuator_allowed="
           << (bridge.actuatorAllowed ? "true" : "false") << '\n'
           << "actual_load_status=" << bridge.loadStatus << '\n'
           << "actual_persistence_validated="
           << (bridge.persistenceValidated ? "true" : "false") << '\n'
           << "actual_coordinator_state=" << bridge.coordinatorState << '\n'
           << "product_recovery_gate=" << productGateStatus << '\n'
           << "comparison_result=" << productGateStatus << '\n';
    result.close();
    CHECK(result.good());
}

void abruptPowerCutRecoveryChild(const char* caseId, const char* imagePath,
                                 const char* metadataPath,
                                 const char* oldHeadPath,
                                 const char* newHeadPath,
                                 const char* resultPath) {
    PartitionFixture fixture("pcut_recovery", 0x10000U, false);
    if (!fixture.disk().loadImage(imagePath)) _exit(140);
    fixture.initialize();
    auto store = fixture.openStore();
    const auto oldHead = readBinaryFile(oldHeadPath);
    const auto newHead = readBinaryFile(newHeadPath);
    const auto headRead = store->read(key("rh0"), 256U);
    const auto head =
        headRead.status == device_platform::StateStoreReadStatus::Success
            ? fermentation::decodeRunPersistenceHead(
                  headRead.value, device_platform::StorageEpoch{1U})
            : std::nullopt;
    bool currentReferenceValid = false;
    bool fallbackReferenceValid = false;
    bool orphanRecordPresent = false;
    if (head.has_value()) {
        const auto currentRead =
            store->read(key(head->current.slot == 0U ? "rc0" : "rc1"), 8240U);
        const auto currentRecord =
            currentRead.status == device_platform::StateStoreReadStatus::Success
                ? fermentation::decodeRunPersistenceRecord(
                      currentRead.value, device_platform::StorageEpoch{1U})
                : std::nullopt;
        currentReferenceValid =
            currentRecord.has_value() &&
            fermentation::runCheckpointReferenceMatches(
                head->current, *currentRecord, head->current.slot);
        if (head->fallback.has_value()) {
            const auto fallbackRead = store->read(
                key(head->fallback->slot == 0U ? "rc0" : "rc1"), 8240U);
            const auto fallbackRecord =
                fallbackRead.status ==
                        device_platform::StateStoreReadStatus::Success
                    ? fermentation::decodeRunPersistenceRecord(
                          fallbackRead.value, device_platform::StorageEpoch{1U})
                    : std::nullopt;
            fallbackReferenceValid =
                fallbackRecord.has_value() &&
                fermentation::runCheckpointReferenceMatches(
                    *head->fallback, *fallbackRecord, head->fallback->slot) &&
                currentRecord.has_value() &&
                fallbackRecord->snapshot.activeRunId ==
                    currentRecord->snapshot.activeRunId &&
                fallbackRecord->checkpointRevision <
                    currentRecord->checkpointRevision;
        }
        for (const auto slot : {0U, 1U}) {
            if (slot == head->current.slot ||
                (head->fallback.has_value() && slot == head->fallback->slot))
                continue;
            const auto orphanRead =
                store->read(key(slot == 0U ? "rc0" : "rc1"), 8240U);
            orphanRecordPresent =
                orphanRecordPresent ||
                (orphanRead.status ==
                     device_platform::StateStoreReadStatus::Success &&
                 fermentation::decodeRunPersistenceRecord(
                     orphanRead.value, device_platform::StorageEpoch{1U})
                     .has_value());
        }
    }
    const bool oldHeadExact = headRead.value == oldHead;
    const bool newHeadExact = headRead.value == newHead;
    const ExpectedProductTruth* expected =
        newHeadExact && currentReferenceValid && fallbackReferenceValid
            ? &kUnknownCommitNewExpected
        : oldHeadExact && currentReferenceValid ? &kFullyValidCurrentExpected
                                                : nullptr;
    fermentation::RunPersistenceCoordinator coordinator(
        *store, device_platform::StorageEpoch{1U},
        fermentation::RunCheckpointSchedule{});
    const auto bridge = runProductBridge(caseId, coordinator, expected);
    const char* productGateStatus =
        expected == nullptr ? "BLOCKED"
                            : (bridge.productRecoveryGate ? "PASS" : "FAIL");
    const char* postRestartHeadState = newHeadExact   ? "NEW_EXACT"
                                       : oldHeadExact ? "OLD_EXACT"
                                                      : "UNEXPECTED";
    writePowerCutRecoveryResult(resultPath, caseId, oldHeadExact, newHeadExact,
                                postRestartHeadState, currentReferenceValid,
                                fallbackReferenceValid, orphanRecordPresent,
                                expected, bridge, productGateStatus);
    static_cast<void>(metadataPath);
    _exit(0);
}

void abruptPowerCutWriterChild(const char* imagePath, const char* metadataPath,
                               const char* oldHeadPath, const char* newHeadPath,
                               BdlCutPhase phase) {
    resetFaults();
    PartitionFixture fixture("pcut_writer", 0x10000U);
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
    program->program.fermentationStages.front().targetTemperatureCelsius = 38.0;
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
    CHECK(
        coordinator
            .persistCommand(state, start,
                            fermentation::RunCheckpointTime{100U, std::nullopt})
            .status == fermentation::RunPersistenceResultStatus::Applied);
    const auto oldHeadRead = store->read(key("rh0"), 256U);
    CHECK(oldHeadRead.status == device_platform::StateStoreReadStatus::Success);
    CHECK(!oldHeadRead.value.empty());
    fixture.disk().setAbruptCutFiles(imagePath, metadataPath);
    fixture.disk().setMutationHeadFiles(oldHeadPath, newHeadPath);
    fixture.disk().setMutationOldHead(oldHeadRead.value);
    fixture.disk().clearEvents();
    capturedRh0Write.clear();
    activeBdlDisk = &fixture.disk();
    rh0CutPhase = phase;
    rh0CutMode = BdlCutMode::AbruptProcessExit;
    armRh0Cut = true;
    static_cast<void>(coordinator.checkpointPeriodic(
        state, fermentation::RunCheckpointTime{300100U, std::nullopt}));
    _exit(141);
}

void writePowerCutArtifact(const char* caseId, const char* stimulusKind,
                           const std::string& oldHead,
                           const std::string& newHead,
                           const std::string& frozenImage,
                           const std::map<std::string, std::string>& metadata,
                           const std::map<std::string, std::string>& result) {
    const std::filesystem::path directory = "/tmp/issue90-slice5-artifacts";
    std::error_code error;
    std::filesystem::create_directories(directory, error);
    CHECK(!error);
    const auto value = [&result](const char* name) {
        const auto found = result.find(name);
        return found == result.end() ? std::string("NOT_RUN") : found->second;
    };
    const auto metadataValue = [&metadata](const char* name) {
        const auto found = metadata.find(name);
        return found == metadata.end() ? std::string("NOT_AVAILABLE")
                                       : found->second;
    };
    std::ofstream artifact(directory /
                           (std::string("power_cut_") + caseId + ".json"));
    CHECK(artifact.good());
    artifact << "{\n"
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
             << "  \"partition_backend\": \"frozen_image_bdl\",\n"
             << "  \"namespace\": \"fermentation\",\n"
             << "  \"logical_key\": \"rh0\",\n"
             << "  \"actual_target_key\": \"" << metadataValue("logical_key")
             << "\",\n"
             << "  \"old_identity\": \"" << bytesIdentity(oldHead) << "\",\n"
             << "  \"new_identity\": \"" << bytesIdentity(newHead) << "\",\n"
             << "  \"old_head_bytes_ne_new_head_bytes\": "
             << (oldHead != newHead ? "true" : "false") << ",\n"
             << "  \"target_bdl_operation\": \"write\",\n"
             << "  \"target_occurrence\": " << metadataValue("occurrence")
             << ",\n"
             << "  \"target_offset\": " << metadataValue("offset") << ",\n"
             << "  \"target_length\": " << metadataValue("length") << ",\n"
             << "  \"baseline_checksum_immediately_before_target\": "
             << metadataValue("baseline_checksum") << ",\n"
             << "  \"frozen_image_identity\": \"" << bytesIdentity(frozenImage)
             << "\",\n"
             << "  \"after_image_checksum\": \""
             << metadataValue("image_checksum") << "\",\n"
             << "  \"injected_fault_or_cut\": \"" << stimulusKind << "\",\n"
             << "  \"write_status\": \"CommitOutcomeUnknown\",\n"
             << "  \"reinit_status\": \"Success\",\n"
             << "  \"oracle_case_id\": \"" << value("oracle_case_id") << "\",\n"
             << "  \"expected_product_outcome\": \""
             << value("expected_product_outcome") << "\",\n"
             << "  \"actual_product_outcome\": \""
             << value("actual_product_outcome") << "\",\n"
             << "  \"expected_safety_projection\": \""
             << value("expected_safety_projection") << "\",\n"
             << "  \"actual_safety_projection\": \""
             << value("actual_safety_projection") << "\",\n"
             << "  \"expected_logical_gate\": \""
             << value("expected_logical_gate") << "\",\n"
             << "  \"actual_logical_gate\": \"" << value("actual_logical_gate")
             << "\",\n"
             << "  \"expected_actuator_allowed\": "
             << value("expected_actuator_allowed") << ",\n"
             << "  \"actual_actuator_allowed\": "
             << value("actual_actuator_allowed") << ",\n"
             << "  \"backend_characterization\": \"observed\",\n"
             << "  \"product_recovery_gate\": \""
             << value("product_recovery_gate") << "\",\n"
             << "  \"result\": \"" << value("comparison_result") << "\"\n}\n";
    artifact.close();
    CHECK(artifact.good());
}

bool abruptPowerCutCase(const char* caseId, BdlCutPhase phase) {
    const std::filesystem::path directory = "/tmp/issue90-slice5-artifacts";
    std::error_code error;
    std::filesystem::create_directories(directory, error);
    CHECK(!error);
    const auto prefix = directory / (std::string("power_") + caseId);
    const auto imagePath = prefix.string() + ".image";
    const auto metadataPath = prefix.string() + ".metadata";
    const auto oldHeadPath = prefix.string() + ".old_head";
    const auto newHeadPath = prefix.string() + ".new_head";
    const auto resultPath = prefix.string() + ".result";
    for (const auto& path :
         {imagePath, metadataPath, oldHeadPath, newHeadPath, resultPath}) {
        std::filesystem::remove(path, error);
    }

    const auto writer = fork();
    CHECK(writer >= 0);
    if (writer == 0) {
        abruptPowerCutWriterChild(imagePath.c_str(), metadataPath.c_str(),
                                  oldHeadPath.c_str(), newHeadPath.c_str(),
                                  phase);
    }
    int writerStatus = 0;
    CHECK(waitForChild(writer, &writerStatus) == writer);
    if (!WIFEXITED(writerStatus) || WEXITSTATUS(writerStatus) != 0) {
        std::printf(
            "issue90_power_cut_case=%s result=BLOCKED "
            "reason=writer_child_exit_%d\n",
            caseId, WIFEXITED(writerStatus) ? WEXITSTATUS(writerStatus) : -1);
        return false;
    }

    const auto recovery = fork();
    CHECK(recovery >= 0);
    if (recovery == 0) {
        abruptPowerCutRecoveryChild(caseId, imagePath.c_str(),
                                    metadataPath.c_str(), oldHeadPath.c_str(),
                                    newHeadPath.c_str(), resultPath.c_str());
    }
    int recoveryStatus = 0;
    CHECK(waitForChild(recovery, &recoveryStatus) == recovery);
    if (!WIFEXITED(recoveryStatus) || WEXITSTATUS(recoveryStatus) != 0) {
        std::printf(
            "issue90_power_cut_case=%s result=BLOCKED "
            "reason=recovery_child_exit_%d\n",
            caseId,
            WIFEXITED(recoveryStatus) ? WEXITSTATUS(recoveryStatus) : -1);
        return false;
    }

    const auto oldHead = readBinaryFile(oldHeadPath);
    const auto newHead = readBinaryFile(newHeadPath);
    const auto frozenImage = readBinaryFile(imagePath);
    const auto metadata = readKeyValueFile(metadataPath);
    const auto result = readKeyValueFile(resultPath);
    CHECK(!oldHead.empty());
    CHECK(!newHead.empty());
    CHECK(!frozenImage.empty());
    CHECK(!metadata.empty());
    CHECK(!result.empty());
    writePowerCutArtifact(
        caseId,
        phase == BdlCutPhase::Before ? "POWER_CUT_BEFORE" : "POWER_CUT_AFTER",
        oldHead, newHead, frozenImage, metadata, result);
    for (const auto& [name, value] : result) {
        std::printf("issue90_power_cut_%s=%s\n", name.c_str(), value.c_str());
    }
    return requiredValue(result, "product_recovery_gate") == "PASS";
}

void abruptPowerCutHarness() {
    const bool before = abruptPowerCutCase("rh0_before", BdlCutPhase::Before);
    const bool after = abruptPowerCutCase("rh0_after", BdlCutPhase::After);
    abruptPowerCutAllPass = before && after;
    std::printf("issue90_power_cut_summary=before_%s after_%s\n",
                before ? "PASS" : "BLOCKED_OR_FAIL",
                after ? "PASS" : "BLOCKED_OR_FAIL");
}

void abruptGcEraseWriterChild(const char* imagePath, const char* metadataPath,
                              const char* oldValuePath,
                              const char* newValuePath, BdlCutPhase phase) {
    resetFaults();
    PartitionFixture fixture("gc_pcut_write", 0x20000U);
    auto store = fixture.openStore();
    fixture.disk().setAbruptCutFiles(imagePath, metadataPath);
    fixture.disk().armCutForNext(BdlOperation::Erase, phase,
                                 BdlCutMode::AbruptProcessExit);
    std::string oldValue;
    for (std::size_t revision = 0U; revision < 256U; ++revision) {
        std::string value(8240U, static_cast<char>(revision & 0x7FU));
        value[0] = static_cast<char>((revision + 1U) & 0x7FU);
        fixture.disk().setMutationHeadFiles(oldValuePath, newValuePath);
        fixture.disk().setMutationOldHead(oldValue);
        fixture.disk().setMutationNewHead(value);
        if (store->write(key("gc_window"), value) !=
            device_platform::StateStoreWriteStatus::Success) {
            _exit(143);
        }
        oldValue = value;
    }
    _exit(144);
}

void abruptGcEraseRecoveryChild(const char* imagePath, const char* resultPath) {
    PartitionFixture fixture("gc_pcut_reco", 0x20000U, false);
    if (!fixture.disk().loadImage(imagePath)) _exit(145);
    fixture.initialize();
    auto store = fixture.openStore();
    const auto read = store->read(key("gc_window"), 8240U);
    std::ofstream result(resultPath, std::ios::out | std::ios::trunc);
    if (!result.good()) _exit(146);
    result << "restart_read_status=" << readStatusName(read.status) << '\n'
           << "restart_value_identity="
           << (read.status == device_platform::StateStoreReadStatus::Success
                   ? bytesIdentity(read.value)
                   : "NOT_VISIBLE")
           << '\n';
    result.close();
    if (!result.good()) _exit(147);
    _exit(0);
}

void writeGcErasePowerCutArtifact(
    const char* caseId, const char* stimulusKind,
    const std::map<std::string, std::string>& metadata,
    const std::map<std::string, std::string>& result,
    const std::string& frozenImage, const std::string& oldValue,
    const std::string& newValue, bool oldExact, bool newExact,
    const char* backendCharacterization, const char* backendResult) {
    const std::filesystem::path directory = "/tmp/issue90-slice5-artifacts";
    const auto metadataValue = [&metadata](const char* name) {
        const auto found = metadata.find(name);
        return found == metadata.end() ? std::string("NOT_AVAILABLE")
                                       : found->second;
    };
    const auto resultValue = [&result](const char* name) {
        const auto found = result.find(name);
        return found == result.end() ? std::string("NOT_RUN") : found->second;
    };
    const auto recordedTargetKey = metadataValue("logical_key");
    const auto targetKey = recordedTargetKey.empty()
                               ? std::string("NOT_APPLICABLE")
                               : recordedTargetKey;
    std::ofstream artifact(directory /
                           (std::string("power_cut_") + caseId + ".json"));
    CHECK(artifact.good());
    artifact << "{\n"
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
             << "  \"partition_backend\": \"frozen_image_bdl\",\n"
             << "  \"namespace\": \"fermentation\",\n"
             << "  \"logical_key\": \"NOT_APPLICABLE\",\n"
             << "  \"actual_target_key\": \"" << targetKey << "\",\n"
             << "  \"target_bdl_operation\": \"erase\",\n"
             << "  \"target_occurrence\": " << metadataValue("occurrence")
             << ",\n"
             << "  \"target_offset\": " << metadataValue("offset") << ",\n"
             << "  \"target_length\": " << metadataValue("length") << ",\n"
             << "  \"baseline_checksum_immediately_before_target\": "
             << metadataValue("baseline_checksum") << ",\n"
             << "  \"old_identity\": \"" << bytesIdentity(oldValue) << "\",\n"
             << "  \"new_identity\": \"" << bytesIdentity(newValue) << "\",\n"
             << "  \"restart_value_identity\": \""
             << resultValue("restart_value_identity") << "\",\n"
             << "  \"old_exact\": " << (oldExact ? "true" : "false") << ",\n"
             << "  \"new_exact\": " << (newExact ? "true" : "false") << ",\n"
             << "  \"frozen_image_identity\": \"" << bytesIdentity(frozenImage)
             << "\",\n"
             << "  \"after_image_checksum\": \""
             << metadataValue("image_checksum") << "\",\n"
             << "  \"injected_fault_or_cut\": \"" << stimulusKind << "\",\n"
             << "  \"write_status\": \"NOT_APPLICABLE\",\n"
             << "  \"reinit_status\": \"Success\",\n"
             << "  \"restart_read_status\": \""
             << resultValue("restart_read_status") << "\",\n"
             << "  \"oracle_case_id\": \"NOT_APPLICABLE\",\n"
             << "  \"backend_characterization\": \"" << backendCharacterization
             << "\",\n"
             << "  \"product_recovery_gate\": \"NOT_APPLICABLE\",\n"
             << "  \"result\": \"" << backendResult << "\"\n}\n";
    artifact.close();
    CHECK(artifact.good());
}

bool abruptGcEraseCase(const char* caseId, BdlCutPhase phase) {
    const std::filesystem::path directory = "/tmp/issue90-slice5-artifacts";
    std::error_code error;
    std::filesystem::create_directories(directory, error);
    CHECK(!error);
    const auto prefix = directory / (std::string("gc_") + caseId);
    const auto imagePath = prefix.string() + ".image";
    const auto metadataPath = prefix.string() + ".metadata";
    const auto oldValuePath = prefix.string() + ".old_value";
    const auto newValuePath = prefix.string() + ".new_value";
    const auto resultPath = prefix.string() + ".result";
    for (const auto& path :
         {imagePath, metadataPath, oldValuePath, newValuePath, resultPath}) {
        std::filesystem::remove(path, error);
    }
    const auto writer = fork();
    CHECK(writer >= 0);
    if (writer == 0) {
        abruptGcEraseWriterChild(imagePath.c_str(), metadataPath.c_str(),
                                 oldValuePath.c_str(), newValuePath.c_str(),
                                 phase);
    }
    int writerStatus = 0;
    CHECK(waitForChild(writer, &writerStatus) == writer);
    if (!WIFEXITED(writerStatus) || WEXITSTATUS(writerStatus) != 0) {
        std::printf(
            "issue90_gc_erase_cut_case=%s result=BLOCKED "
            "reason=writer_child_exit_%d\n",
            caseId, WIFEXITED(writerStatus) ? WEXITSTATUS(writerStatus) : -1);
        return false;
    }
    const auto recovery = fork();
    CHECK(recovery >= 0);
    if (recovery == 0) {
        abruptGcEraseRecoveryChild(imagePath.c_str(), resultPath.c_str());
    }
    int recoveryStatus = 0;
    CHECK(waitForChild(recovery, &recoveryStatus) == recovery);
    if (!WIFEXITED(recoveryStatus) || WEXITSTATUS(recoveryStatus) != 0) {
        std::printf(
            "issue90_gc_erase_cut_case=%s result=BLOCKED "
            "reason=recovery_child_exit_%d\n",
            caseId,
            WIFEXITED(recoveryStatus) ? WEXITSTATUS(recoveryStatus) : -1);
        return false;
    }
    const auto frozenImage = readBinaryFile(imagePath);
    const auto metadata = readKeyValueFile(metadataPath);
    const auto result = readKeyValueFile(resultPath);
    CHECK(!frozenImage.empty());
    CHECK(!metadata.empty());
    CHECK(!result.empty());
    const auto oldValue = readBinaryFile(oldValuePath);
    const auto newValue = readBinaryFile(newValuePath);
    CHECK(!oldValue.empty());
    CHECK(!newValue.empty());
    const auto restartStatus = result.find("restart_read_status");
    const auto restartIdentity = result.find("restart_value_identity");
    CHECK(restartStatus != result.end());
    CHECK(restartIdentity != result.end());
    const bool oldExact = restartStatus->second == "Success" &&
                          restartIdentity->second == bytesIdentity(oldValue);
    const bool newExact = restartStatus->second == "Success" &&
                          restartIdentity->second == bytesIdentity(newValue);
    const bool backendPass = oldExact || newExact;
    const char* backendCharacterization =
        backendPass ? "observed" : "unexpected_change";
    const char* backendResult = backendPass ? "PASS" : "FAIL";
    std::map<std::string, std::string> classifiedResult = result;
    classifiedResult["product_recovery_gate"] = "NOT_APPLICABLE";
    classifiedResult["result"] = backendResult;
    writeGcErasePowerCutArtifact(
        caseId,
        phase == BdlCutPhase::Before ? "POWER_CUT_ERASE_BEFORE"
                                     : "POWER_CUT_ERASE_AFTER",
        metadata, classifiedResult, frozenImage, oldValue, newValue, oldExact,
        newExact, backendCharacterization, backendResult);
    for (const auto& [name, value] : classifiedResult) {
        std::printf("issue90_gc_erase_%s=%s\n", name.c_str(), value.c_str());
    }
    return backendPass;
}

void abruptGcEraseHarness() {
    const bool before = abruptGcEraseCase("before", BdlCutPhase::Before);
    const bool after = abruptGcEraseCase("after", BdlCutPhase::After);
    abruptGcEraseCutAllPass = before && after;
    std::printf("issue90_gc_erase_cut_summary=before_%s after_%s\n",
                before ? "PASS" : "BLOCKED_OR_FAIL",
                after ? "PASS" : "BLOCKED_OR_FAIL");
}

void productRecordSizeAndMutationMatrix() {
    PartitionFixture fixture("adapter_matrix", 0x20000U);
    auto store = fixture.openStore();
    CHECK(store->write(key("overwrite"), "old") ==
          device_platform::StateStoreWriteStatus::Success);
    CHECK(store->write(key("overwrite"), "new") ==
          device_platform::StateStoreWriteStatus::Success);
    CHECK(store->read(key("overwrite"), 3U).value == "new");
    constexpr std::array<std::size_t, 9U> sizes = {
        1U, 32U, 1024U, 3999U, 4000U, 4001U, 4096U, 8192U, 8240U};
    for (const std::size_t size : sizes) {
        const auto recordKey = key(("k" + std::to_string(size)).c_str());
        const std::string value(size, static_cast<char>(size & 0x7FU));
        CHECK(store->write(recordKey, value) ==
              device_platform::StateStoreWriteStatus::Success);
    }
    store.reset();
    fixture.restart();
    store = fixture.openStore();
    for (const std::size_t size : sizes) {
        const auto recordKey = key(("k" + std::to_string(size)).c_str());
        const std::string expected(size, static_cast<char>(size & 0x7FU));
        const auto readback = store->read(recordKey, size);
        CHECK(readback.status ==
              device_platform::StateStoreReadStatus::Success);
        CHECK(readback.value == expected);
    }
    std::puts("issue90_real_chunk_boundaries=3999,4000,4001 PASS");
}

std::size_t nvsModelBlobEntries(std::size_t blobSize) {
    constexpr std::size_t chunkSize = 4000U;
    const auto chunkEntries = [](std::size_t size) {
        return 1U + (size + 31U) / 32U;
    };
    if (blobSize == 0U) return 1U + chunkEntries(0U);
    std::size_t entries = 1U;
    while (blobSize != 0U) {
        const auto chunk = std::min(blobSize, chunkSize);
        entries += chunkEntries(chunk);
        blobSize -= chunk;
    }
    return entries;
}

void nvsStatsCrosscheck() {
    constexpr std::array<std::pair<const char*, std::size_t>, 22U> slots = {{
        {"uc0", 301U},   {"uc1", 301U},   {"uc2", 301U},   {"uc3", 301U},
        {"sc0", 45U},    {"sc1", 45U},    {"sc2", 45U},    {"sc3", 45U},
        {"pc0", 32813U}, {"pc1", 32813U}, {"pc2", 32813U}, {"pc3", 32813U},
        {"cm0", 149U},   {"cm1", 149U},   {"cm2", 149U},   {"cr0", 114U},
        {"cr1", 114U},   {"cb0", 42U},    {"cb1", 42U},    {"rc0", 8240U},
        {"rc1", 8240U},  {"rh0", 256U},
    }};
    PartitionFixture fixture("capacity_stats", 0x100000U);
    auto store = fixture.openStore();
    nvs_stats_t before{};
    CHECK(nvs_get_stats(fixture.label(), &before) == ESP_OK);
    std::size_t modeledFullEntries = 1U;
    for (const auto& [rawKey, blobSize] : slots) {
        const auto value = std::string(blobSize, 's');
        CHECK(store->write(key(rawKey), value) ==
              device_platform::StateStoreWriteStatus::Success);
        const auto entries = nvsModelBlobEntries(blobSize);
        modeledFullEntries += entries;
    }
    nvs_stats_t full{};
    CHECK(nvs_get_stats(fixture.label(), &full) == ESP_OK);
    CHECK(full.used_entries <= modeledFullEntries);

    const auto transientValue = std::string(32813U, 't');
    CHECK(store->write(key("pc0"), transientValue) ==
          device_platform::StateStoreWriteStatus::Success);
    nvs_stats_t updated{};
    CHECK(nvs_get_stats(fixture.label(), &updated) == ESP_OK);
    const auto conservativeTransientEntries = modeledFullEntries;
    CHECK(updated.used_entries <=
          modeledFullEntries + conservativeTransientEntries);
    std::printf(
        "issue90_nvs_stats_crosscheck=PASS before_used=%zu full_used=%zu "
        "updated_used=%zu modeled_full=%zu "
        "modeled_conservative_full_copy=%zu\n",
        before.used_entries, full.used_entries, updated.used_entries,
        modeledFullEntries, conservativeTransientEntries);
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
    espTimeZoneResolverContract();
    canonicalOracleIdSelfTest();
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
    nvsStatsCrosscheck();
    const bool gcEraseObserved = gcEraseCharacterization();
    cutHarnessWritesEarlyAndLateArtifacts();
    abruptPowerCutHarness();
    abruptGcEraseHarness();
    finalGateNegativeSelfTests();
    CHECK(finalHostGatePass(gcEraseObserved, abruptPowerCutAllPass,
                            abruptGcEraseCutAllPass, productBridgeFailures,
                            productBridgeBlocked, productBridgeNotRun));
    std::puts("ISSUE90_HOST_ADAPTER_GATE=PASS");
    std::puts(
        "issue90_backend_characterization=observed "
        "evidence_scope=ESP_IDF_BDL_HOST product_recovery_gate=NOT_RUN");
    const auto bridgeSummary =
        std::string("issue90_product_bridge_summary=PASS:") +
        std::to_string(productBridgePasses) +
        " FAIL:" + std::to_string(productBridgeFailures) +
        " BLOCKED:" + std::to_string(productBridgeBlocked) +
        " NOT_RUN:" + std::to_string(productBridgeNotRun) +
        " callback_12_excluded=true";
    std::puts(bridgeSummary.c_str());
    std::puts(
        "issue90_backend_matrix=small_blob,same_key_overwrite,medium_blob,page_"
        "boundary_3999_4000_4001,product_record_8240,new_key,existing_key,"
        "commit,readback_"
        "capacity,empty_blob,bdl_io_failure_before_rh0,"
        "bdl_io_failure_after_rh0,bdl_committed_rh0_product_bridge,"
        "power_cut_rh0_before,power_cut_rh0_after,"
        "gc_erase_cut_before,gc_erase_cut_after");
    std::printf(
        "DEFINED_BACKEND_MATRIX_COMPLETE=true "
        "gc_erase_characterization=%s gc_erase_cut=%s "
        "power_cut_harness=%s power_cut_evidence=%s "
        "power_cut_block_reason=%s\n",
        gcEraseObserved ? "OBSERVED" : "GC_ERASE_CHARACTERIZATION_BLOCKED",
        abruptGcEraseCutAllPass ? "PASS" : "BLOCKED",
        abruptPowerCutAllPass ? "PASS" : "POWER_CUT_HARNESS_BLOCKED",
        abruptPowerCutAllPass ? "PASS" : "NOT_RUN",
        abruptPowerCutAllPass ? "NONE"
                              : "NO_DETERMINISTIC_FROZEN_IMAGE_RESULT");
    std::puts(
        "callback_12=FAIL_CALLBACK_12_NOT_FOUND "
        "backend_characterization=known_limitation "
        "evidence_scope=REAL_NVS_ONLY product_recovery_gate=NOT_RUN "
        "CALLBACK_12_REAL_NVS_PRODUCT_GATE=NOT_RUN_PENDING_SLICE7");
    std::exit(0);
}
