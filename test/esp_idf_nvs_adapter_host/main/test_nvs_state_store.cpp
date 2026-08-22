#include "nvs_state_store.hpp"

#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>

#include "bdl_ramdisk.hpp"
#include "nvs_flash.h"

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
    CHECK(!device_platform_esp_idf::NvsStateStoreConfig::create(
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
    setFault = SetFault::SafeWriteError;
    CHECK(store->write(key("writeerr"), "rejected") ==
          device_platform::StateStoreWriteStatus::WriteError);
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
    PartitionFixture fixture("adapter_small", 0x3000U);
    auto store = fixture.openStore();
    const std::string tooLarge(8240U, 'c');
    CHECK(store->write(key("large"), tooLarge) ==
          device_platform::StateStoreWriteStatus::CapacityError);
}

void productRecordSizeAndMutationMatrix() {
    PartitionFixture fixture("adapter_matrix", 0x20000U);
    auto store = fixture.openStore();
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
    productRecordSizeAndMutationMatrix();
    std::puts("Issue90 NVS adapter host tests: 12/12 PASS");
}
