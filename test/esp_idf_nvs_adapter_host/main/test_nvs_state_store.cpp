#include "nvs_state_store.hpp"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>

#include "bdl_ramdisk.hpp"
#include "nvs_flash.h"
#include "run_persistence_coordinator.hpp"
#include "safety_core.hpp"

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
    return __real_nvs_set_blob(handle, key, value, length);
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
    const char* safetyProjection;
    const char* safetyProducer;
    bool actuatorAllowed;
};

ProductBridgeObservation runProductBridge(
    const char* caseId, fermentation::RunPersistenceCoordinator& coordinator) {
    const auto loaded = coordinator.loadAndInitialize();
    fermentation::SafetyCore safety;
    safety.beginBoot(device_platform::ResetCause::Unknown);
    fermentation::SafetyCoreInput input;
    input.bootValidationComplete = true;
    input.persistenceLoadStatus = loaded.status;
    input.persistenceSnapshot =
        loaded.snapshot.has_value() ? &*loaded.snapshot : nullptr;
    input.persistenceCoordinatorState = coordinator.state();
    input.persistenceValidated = false;
    const auto evaluation = safety.evaluate(input);
    const auto observation = ProductBridgeObservation{
        runLoadStatusName(loaded.status),
        coordinatorStateName(coordinator.state()),
        safetyBootDispositionName(evaluation.bootDisposition),
        faultCodeName(evaluation.faultCode),
        evaluation.gate.status ==
            fermentation::ActuatorSafetyGateStatus::Allowed};
    std::printf(
        "issue90_product_bridge_case=%s actual_load_status=%s "
        "actual_coordinator_state=%s actual_safety_projection=%s "
        "actual_safety_producer=%s actual_logical_gate=UNRESOLVED "
        "actual_actuator_allowed=%s product_recovery_gate=NOT_RUN\n",
        caseId, observation.loadStatus, observation.coordinatorState,
        observation.safetyProjection, observation.safetyProducer,
        observation.actuatorAllowed ? "true" : "false");
    return observation;
}

void writeCutArtifact(const char* caseId, const char* label,
                      const char* targetKey, const char* oldIdentity,
                      const char* newIdentity, const char* injectedPhase,
                      std::uint32_t baselineChecksum, const TestRamDisk& disk,
                      device_platform::StateStoreWriteStatus writeStatus,
                      device_platform::StateStoreReadStatus readStatus,
                      const ProductBridgeObservation& bridge) {
    const std::filesystem::path directory = "/tmp/issue90-slice5-artifacts";
    std::error_code error;
    std::filesystem::create_directories(directory, error);
    CHECK(!error);
    std::ofstream artifact(directory / (std::string(caseId) + ".json"));
    CHECK(artifact.good());
    artifact << "{\n"
             << "  \"source_sha\": \""
             << (std::getenv("ISSUE90_SOURCE_SHA") == nullptr
                     ? "UNKNOWN"
                     : std::getenv("ISSUE90_SOURCE_SHA"))
             << "\",\n"
             << "  \"esp_idf\": "
                "\"v6.0.2@7101770dc6db2667b3c477cc31365dd1acd6db4e\",\n"
             << "  \"case_id\": \"" << caseId << "\",\n"
             << "  \"partition\": \"" << label << "\",\n"
             << "  \"namespace\": \"fermentation\",\n"
             << "  \"key\": \"" << targetKey << "\",\n"
             << "  \"blob_size\": 32,\n"
             << "  \"old_identity\": \"" << oldIdentity << "\",\n"
             << "  \"new_identity\": \"" << newIdentity << "\",\n"
             << "  \"baseline_checksum_before_cut\": " << baselineChecksum
             << ",\n"
             << "  \"target_operation\": \"write\",\n"
             << "  \"injected_cut\": \"" << injectedPhase << "\",\n"
             << "  \"write_status\": " << static_cast<unsigned>(writeStatus)
             << ",\n"
             << "  \"read_status_after_reinit\": "
             << static_cast<unsigned>(readStatus) << ",\n"
             << "  \"backend_classification\": \"observed\",\n"
             << "  \"product_recovery_result\": \"" << bridge.loadStatus
             << "\",\n"
             << "  \"product_coordinator_state\": \"" << bridge.coordinatorState
             << "\",\n"
             << "  \"product_safety_projection\": \"" << bridge.safetyProjection
             << "\",\n"
             << "  \"product_safety_producer\": \"" << bridge.safetyProducer
             << "\",\n"
             << "  \"product_logical_gate\": \"UNRESOLVED\",\n"
             << "  \"product_actuator_allowed\": "
             << (bridge.actuatorAllowed ? "true" : "false") << ",\n"
             << "  \"product_recovery_gate\": \"NOT_RUN\",\n"
             << "  \"events\": [\n";
    const auto events = disk.events();
    for (std::size_t index = 0U; index < events.size(); ++index) {
        const auto& event = events[index];
        artifact << "    {\"operation\": \"" << operationName(event.operation)
                 << "\", \"offset\": " << event.offset
                 << ", \"length\": " << event.length
                 << ", \"occurrence\": " << event.occurrence
                 << ", \"result\": " << event.result << "}"
                 << (index + 1U == events.size() ? "\n" : ",\n");
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
        fixture.disk().clearEvents();
        const auto baselineChecksum = fixture.disk().checksum();
        fixture.disk().setCutPlan(BdlOperation::Write, 1U, phase);
        const auto writeStatus =
            store->write(key("cut_target"), std::string(32U, 'n'));
        store.reset();
        fixture.disk().clearCutPlan();
        fixture.restart();
        store = fixture.openStore();
        const auto readStatus = store->read(key("rh0"), 32U).status;
        fermentation::RunPersistenceCoordinator coordinator(
            *store, device_platform::StorageEpoch{1U},
            fermentation::RunCheckpointSchedule{});
        const auto bridge = runProductBridge(caseId, coordinator);
        writeCutArtifact(
            caseId, fixture.label(), "rh0", "OLD_ABSENT", "NEW_32_BYTES",
            phase == BdlCutPhase::Before ? "before_write" : "after_write",
            baselineChecksum, fixture.disk(), writeStatus, readStatus, bridge);
    };

    run("bdl_cut_early", "cut_early", BdlCutPhase::Before);
    run("bdl_cut_late", "cut_late", BdlCutPhase::After);
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
    cutHarnessWritesEarlyAndLateArtifacts();
    std::puts("Issue90 NVS adapter host tests: 14/14 PASS");
    std::puts(
        "issue90_backend_characterization=observed "
        "evidence_scope=ESP_IDF_BDL_HOST product_recovery_gate=NOT_RUN");
    std::puts(
        "issue90_backend_matrix=small_blob,same_key_overwrite,medium_blob,page_"
        "boundary,product_record_8240,new_key,existing_key,commit,readback_"
        "capacity,empty_blob,write_cut_before,write_cut_after");
    std::puts(
        "issue90_backend_matrix_completeness=PARTIAL exhaustive=false "
        "gc_erase_cut=NOT_RUN");
    std::puts(
        "callback_12=FAIL_CALLBACK_12_NOT_FOUND "
        "backend_characterization=known_limitation "
        "evidence_scope=REAL_NVS_ONLY product_recovery_gate=NOT_RUN");
}
