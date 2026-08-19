#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "nvs.h"
#include "nvs_flash.h"
#include "spi_flash_mmap.h"
#include "nvs_state_store.hpp"
#include "nvs_api_fault_seam.hpp"
#include "stateful_block_device.hpp"
#include "state_store_key.hpp"

namespace {

class TestFailure final : public std::runtime_error {
   public:
    explicit TestFailure(const char* expression)
        : std::runtime_error(expression) {}
};

#define REQUIRE(expression)                 \
    do {                                    \
        if (!(expression)) {                \
            throw TestFailure(#expression); \
        }                                   \
    } while (false)

using device_platform::StateStoreReadStatus;
using device_platform::StateStoreWriteStatus;
using device_platform_esp_idf::NvsStateStore;
using issue90_host::NvsApiFaultSeam;
using issue90_host::StatefulBlockDevice;

constexpr char kPartition[] = "state_store";
constexpr std::size_t kPartitionSize = 69U * 4096U;
constexpr std::size_t kRotationLimit = 2048U;
constexpr std::size_t kMaximumProgramRecordBytes = 32813U;
constexpr char kIdfSha[] = "7101770dc6db2667b3c477cc31365dd1acd6db4e";
bool g_exhaustive{false};

struct TestRecord final {
    const char* key;
    std::size_t maximumBytes;
};

constexpr std::array<TestRecord, 22U> kRecordInventory = {
    {{"uc0", 301U},   {"uc1", 301U},   {"uc2", 301U},   {"uc3", 301U},
     {"sc0", 45U},    {"sc1", 45U},    {"sc2", 45U},    {"sc3", 45U},
     {"pc0", 32813U}, {"pc1", 32813U}, {"pc2", 32813U}, {"pc3", 32813U},
     {"cm0", 149U},   {"cm1", 149U},   {"cm2", 149U},   {"cr0", 114U},
     {"cr1", 114U},   {"cb0", 42U},    {"cb1", 42U},    {"rc0", 8240U},
     {"rc1", 8240U},  {"rh0", 256U}}};

const TestRecord& recordFor(const char* name) {
    for (const auto& record : kRecordInventory) {
        if (std::string(record.key) == name) return record;
    }
    throw TestFailure("unknown test inventory key");
}

device_platform::StateStoreKey key(const char* value) {
    const auto result = device_platform::StateStoreKey::create(value);
    REQUIRE(result.status == device_platform::StateStoreKeyStatus::Success);
    REQUIRE(result.key.has_value());
    return *result.key;
}

std::string bytes(std::size_t size, std::uint8_t seed) {
    std::string value(size, '\0');
    for (std::size_t index = 0U; index < value.size(); ++index) {
        value[index] = static_cast<char>(seed + (index % 251U));
    }
    return value;
}

class HostStorage final {
   public:
    HostStorage() : device(kPartitionSize, 4096U) {
        NvsApiFaultSeam::reset();
        REQUIRE(nvs_flash_init_partition_bdl(kPartition, device.handle()) ==
                ESP_OK);
        initialized_ = true;
    }

    ~HostStorage() {
        if (initialized_) {
            static_cast<void>(nvs_flash_deinit_partition(kPartition));
        }
        NvsApiFaultSeam::reset();
    }

    HostStorage(const HostStorage&) = delete;
    HostStorage& operator=(const HostStorage&) = delete;

    void reinitialize() {
        REQUIRE(nvs_flash_deinit_partition(kPartition) == ESP_OK);
        initialized_ = false;
        NvsApiFaultSeam::reset();
        REQUIRE(nvs_flash_init_partition_bdl(kPartition, device.handle()) ==
                ESP_OK);
        initialized_ = true;
    }

    StatefulBlockDevice device;

   private:
    bool initialized_{false};
};

void requireOldOrNew(const NvsStateStore& store,
                     const device_platform::StateStoreKey& stateKey,
                     const std::string& oldValue, const std::string& newValue) {
    const auto read =
        store.read(stateKey, std::max(oldValue.size(), newValue.size()));
    REQUIRE(read.status == StateStoreReadStatus::Success);
    REQUIRE((read.value == oldValue || read.value == newValue));
}

void requireExact(const NvsStateStore& store,
                  const device_platform::StateStoreKey& stateKey,
                  const std::string& expected) {
    const auto read = store.read(stateKey, expected.size());
    REQUIRE(read.status == StateStoreReadStatus::Success);
    REQUIRE(read.value == expected);
}

std::vector<std::size_t> relevantMutationCallbacks(
    const std::vector<issue90_host::BlockTraceEvent>& trace) {
    std::vector<std::size_t> result;
    std::size_t lastWideWrite = 0U;
    for (std::size_t index = 0U; index < trace.size(); ++index) {
        const auto& event = trace[index];
        if (event.operation == issue90_host::BlockOperation::Write &&
            event.length > 4U) {
            lastWideWrite = index;
        }
    }
    std::size_t mutation = 0U;
    for (std::size_t index = 0U; index < trace.size(); ++index) {
        const auto& event = trace[index];
        const bool isMutation =
            event.operation == issue90_host::BlockOperation::Write ||
            event.operation == issue90_host::BlockOperation::Erase;
        if (!isMutation) continue;
        ++mutation;
        const bool isDataOrIndexWrite =
            event.operation == issue90_host::BlockOperation::Write &&
            event.length > 4U;
        const bool isTrailingRemoval =
            event.operation == issue90_host::BlockOperation::Write &&
            event.length == 4U && index >= lastWideWrite;
        if (event.operation == issue90_host::BlockOperation::Erase ||
            isDataOrIndexWrite || isTrailingRemoval) {
            result.push_back(mutation);
        }
    }
    REQUIRE(!result.empty());
    return result;
}

}  // namespace

void testBinaryAndEmptyValues() {
    HostStorage storage;
    NvsStateStore store;
    const auto stateKey = key("empty");

    REQUIRE(store.write(stateKey, {}) == StateStoreWriteStatus::Success);
    const auto empty = store.read(stateKey, 0U);
    REQUIRE(empty.status == StateStoreReadStatus::Success);
    REQUIRE(empty.value.empty());

    const std::string binary("\0\xff\x01\0\xa5", 5U);
    REQUIRE(store.write(stateKey, binary) == StateStoreWriteStatus::Success);
    const auto read = store.read(stateKey, binary.size());
    REQUIRE(read.status == StateStoreReadStatus::Success);
    REQUIRE(read.value == binary);
}

void testBoundedTwoStageReadContract() {
    HostStorage storage;
    NvsStateStore store;
    const auto stateKey = key("race");
    const std::string oldValue = "old";
    const std::string replacement(64U, 'R');

    REQUIRE(store.write(stateKey, oldValue) == StateStoreWriteStatus::Success);
    NvsApiFaultSeam::raceAfterSizeQuery(replacement);
    const auto raceOverLimit = store.read(stateKey, 8U);
    REQUIRE(raceOverLimit.status == StateStoreReadStatus::CapacityError);

    REQUIRE(store.write(stateKey, oldValue) == StateStoreWriteStatus::Success);
    NvsApiFaultSeam::raceAfterSizeQuery(replacement);
    const auto raceWithinLimit = store.read(stateKey, replacement.size());
    REQUIRE(raceWithinLimit.status == StateStoreReadStatus::ReadError);
}

void testBlobBoundariesAndPatterns() {
    HostStorage storage;
    NvsStateStore store;
    const std::array<std::pair<const char*, std::size_t>, 4U> cases{
        std::pair{"b32", 32U}, std::pair{"b33", 33U}, std::pair{"c4000", 4000U},
        std::pair{"c4001", 4001U}};
    const std::array<std::uint8_t, 4U> patterns{0x00U, 0xa5U, 0xffU, 0x5aU};
    for (std::size_t index = 0U; index < cases.size(); ++index) {
        const auto stateKey = key(cases[index].first);
        const std::string value(cases[index].second,
                                static_cast<char>(patterns[index]));
        REQUIRE(store.write(stateKey, value) == StateStoreWriteStatus::Success);
        const auto read = store.read(stateKey, value.size());
        REQUIRE(read.status == StateStoreReadStatus::Success);
        REQUIRE(read.value == value);
    }
}

void testMaximumRecordReadLimit() {
    HostStorage storage;
    NvsStateStore store;
    const auto stateKey = key("max");
    const auto maximum = bytes(kMaximumProgramRecordBytes, 0x11U);

    REQUIRE(store.write(stateKey, maximum) == StateStoreWriteStatus::Success);
    REQUIRE(store.read(stateKey, maximum.size() - 1U).status ==
            StateStoreReadStatus::CapacityError);
    const auto read = store.read(stateKey, maximum.size());
    REQUIRE(read.status == StateStoreReadStatus::Success);
    REQUIRE(read.value == maximum);

    const auto replacement = bytes(kMaximumProgramRecordBytes, 0xeeU);
    REQUIRE(store.write(stateKey, replacement) ==
            StateStoreWriteStatus::Success);
    const auto replacementRead = store.read(stateKey, replacement.size());
    REQUIRE(replacementRead.status == StateStoreReadStatus::Success);
    REQUIRE(replacementRead.value == replacement);
}

void testErrorMapping() {
    HostStorage storage;
    NvsStateStore store;
    const auto stateKey = key("errors");
    const std::string oldValue = "old-value";
    const std::string newValue = "new-value";

    const std::array<esp_err_t, 5U> openWriteErrors{
        ESP_ERR_NVS_PART_NOT_FOUND, ESP_ERR_NVS_NO_FREE_PAGES,
        ESP_ERR_NVS_INVALID_STATE, ESP_ERR_NO_MEM, ESP_ERR_NOT_SUPPORTED};
    for (const auto error : openWriteErrors) {
        NvsApiFaultSeam::failOpen(error);
        REQUIRE(store.write(stateKey, newValue) ==
                StateStoreWriteStatus::WriteError);
        NvsApiFaultSeam::reset();
    }

    NvsApiFaultSeam::failOpen(ESP_ERR_NVS_NOT_FOUND);
    REQUIRE(store.read(stateKey, 32U).status == StateStoreReadStatus::NotFound);
    NvsApiFaultSeam::reset();
    for (const auto error : openWriteErrors) {
        NvsApiFaultSeam::failOpen(error);
        REQUIRE(store.read(stateKey, 32U).status ==
                StateStoreReadStatus::ReadError);
        NvsApiFaultSeam::reset();
    }

    REQUIRE(store.write(stateKey, oldValue) == StateStoreWriteStatus::Success);
    NvsApiFaultSeam::failSizeQuery(ESP_ERR_NVS_NOT_FOUND);
    REQUIRE(store.read(stateKey, 32U).status == StateStoreReadStatus::NotFound);
    NvsApiFaultSeam::reset();
    const std::array<esp_err_t, 5U> readErrors{
        ESP_ERR_NVS_INVALID_LENGTH, ESP_ERR_NVS_INVALID_STATE, ESP_ERR_NO_MEM,
        ESP_ERR_NVS_NO_FREE_PAGES, ESP_ERR_NOT_SUPPORTED};
    for (const auto error : readErrors) {
        NvsApiFaultSeam::failSizeQuery(error);
        REQUIRE(store.read(stateKey, 32U).status ==
                StateStoreReadStatus::ReadError);
        NvsApiFaultSeam::reset();
    }
    NvsApiFaultSeam::failRead(ESP_ERR_NVS_NOT_FOUND);
    REQUIRE(store.read(stateKey, 32U).status == StateStoreReadStatus::NotFound);
    NvsApiFaultSeam::reset();
    NvsApiFaultSeam::failRead(ESP_ERR_NVS_INVALID_LENGTH, 64U);
    REQUIRE(store.read(stateKey, 32U).status ==
            StateStoreReadStatus::CapacityError);
    NvsApiFaultSeam::reset();
    for (const auto error : readErrors) {
        NvsApiFaultSeam::failRead(error);
        REQUIRE(store.read(stateKey, 32U).status ==
                StateStoreReadStatus::ReadError);
        NvsApiFaultSeam::reset();
    }

    NvsApiFaultSeam::failSet(ESP_ERR_NVS_VALUE_TOO_LONG);
    REQUIRE(store.write(stateKey, newValue) ==
            StateStoreWriteStatus::CapacityError);
    NvsApiFaultSeam::reset();
    requireExact(store, stateKey, oldValue);

    const std::array<esp_err_t, 4U> preMutationWriteErrors{
        ESP_ERR_NVS_INVALID_HANDLE, ESP_ERR_NVS_READ_ONLY,
        ESP_ERR_NVS_INVALID_NAME, ESP_ERR_NVS_KEY_TOO_LONG};
    for (const auto error : preMutationWriteErrors) {
        NvsApiFaultSeam::failSet(error);
        REQUIRE(store.write(stateKey, newValue) ==
                StateStoreWriteStatus::WriteError);
        NvsApiFaultSeam::reset();
        requireExact(store, stateKey, oldValue);
    }

    const std::array<esp_err_t, 6U> unknownWriteErrors{
        ESP_ERR_NVS_NOT_ENOUGH_SPACE, ESP_ERR_NVS_NO_FREE_PAGES,
        ESP_ERR_NVS_INVALID_STATE,    ESP_ERR_NO_MEM,
        ESP_ERR_NOT_SUPPORTED,        ESP_ERR_NVS_REMOVE_FAILED};
    for (const auto error : unknownWriteErrors) {
        NvsApiFaultSeam::failSet(error);
        REQUIRE(store.write(stateKey, newValue) ==
                StateStoreWriteStatus::CommitOutcomeUnknown);
        NvsApiFaultSeam::reset();
        requireOldOrNew(store, stateKey, oldValue, newValue);
    }

    NvsApiFaultSeam::failSet(ESP_ERR_FLASH_OP_FAIL, true);
    REQUIRE(store.write(stateKey, newValue) ==
            StateStoreWriteStatus::CommitOutcomeUnknown);
    NvsApiFaultSeam::reset();
    requireOldOrNew(store, stateKey, oldValue, newValue);

    NvsApiFaultSeam::failCommit(ESP_FAIL);
    REQUIRE(store.write(stateKey, newValue) ==
            StateStoreWriteStatus::CommitOutcomeUnknown);
    NvsApiFaultSeam::reset();
    requireOldOrNew(store, stateKey, oldValue, newValue);
}

void testOldOrNewAfterBdlCuts() {
    const std::array<std::uint8_t, 3U> patterns{0x00U, 0xa5U, 0xffU};
    const std::size_t repetitions = g_exhaustive ? patterns.size() : 1U;
    for (std::size_t repetition = 0U; repetition < repetitions; ++repetition) {
        const auto stateKey = key("cut");
        const std::string oldValue = bytes(8240U, patterns[repetition]);
        const std::string newValue =
            bytes(8240U, patterns[(repetition + 1U) % patterns.size()]);

        std::vector<std::size_t> mutationPoints;
        {
            HostStorage storage;
            NvsStateStore store;
            REQUIRE(store.write(stateKey, oldValue) ==
                    StateStoreWriteStatus::Success);
            storage.device.clearTrace();
            REQUIRE(store.write(stateKey, newValue) ==
                    StateStoreWriteStatus::Success);
            mutationPoints = relevantMutationCallbacks(storage.device.trace());
        }

        std::vector<std::size_t> cutCallbacks;
        if (g_exhaustive) {
            cutCallbacks = mutationPoints;
        } else {
            cutCallbacks = {mutationPoints.front(), mutationPoints.back()};
            cutCallbacks.push_back(mutationPoints[mutationPoints.size() / 2U]);
            std::sort(cutCallbacks.begin(), cutCallbacks.end());
            cutCallbacks.erase(
                std::unique(cutCallbacks.begin(), cutCallbacks.end()),
                cutCallbacks.end());
        }
        for (const std::size_t callback : cutCallbacks) {
            HostStorage storage;
            NvsStateStore store;
            REQUIRE(store.write(stateKey, oldValue) ==
                    StateStoreWriteStatus::Success);
            storage.device.clearTrace();
            storage.device.failFromCallback(callback);
            const auto result = store.write(stateKey, newValue);
            REQUIRE(result == StateStoreWriteStatus::CommitOutcomeUnknown);
            storage.device.clearFailure();
            storage.reinitialize();
            requireOldOrNew(store, stateKey, oldValue, newValue);
        }

        HostStorage storage;
        NvsStateStore store;
        REQUIRE(store.write(stateKey, oldValue) ==
                StateStoreWriteStatus::Success);
        NvsApiFaultSeam::failCommit(ESP_ERR_FLASH_OP_FAIL);
        REQUIRE(store.write(stateKey, newValue) ==
                StateStoreWriteStatus::CommitOutcomeUnknown);
        NvsApiFaultSeam::reset();
        requireOldOrNew(store, stateKey, oldValue, newValue);
    }
}

using TestValues = std::array<std::string, 22U>;

std::size_t recordIndex(const TestRecord& wanted) {
    for (std::size_t index = 0U; index < kRecordInventory.size(); ++index) {
        if (&kRecordInventory[index] == &wanted) return index;
    }
    throw TestFailure("record is not in canonical test inventory");
}

TestValues prefillInventory(NvsStateStore& store) {
    TestValues values;
    for (std::size_t index = 0U; index < kRecordInventory.size(); ++index) {
        const auto& record = kRecordInventory[index];
        values[index] =
            bytes(record.maximumBytes, static_cast<std::uint8_t>(index));
        REQUIRE(store.write(key(record.key), values[index]) ==
                StateStoreWriteStatus::Success);
    }
    return values;
}

const TestRecord& rotatingRecord(std::size_t rotation) {
    constexpr std::array<const char*, 5U> rotatingKeys{"pc0", "pc1", "rc0",
                                                       "rc1", "rh0"};
    return recordFor(rotatingKeys[rotation % rotatingKeys.size()]);
}

std::string rotateValue(const TestRecord& record, std::size_t rotation) {
    return bytes(record.maximumBytes, static_cast<std::uint8_t>(rotation + 3U));
}

void replayRotations(NvsStateStore& store, std::size_t count) {
    for (std::size_t rotation = 0U; rotation < count; ++rotation) {
        const auto& record = rotatingRecord(rotation);
        REQUIRE(store.write(key(record.key), rotateValue(record, rotation)) ==
                StateStoreWriteStatus::Success);
    }
}

void testPrefilledGcAndEraseWorkload() {
    struct Scenario final {
        std::size_t rotation;
        const TestRecord* record;
        TestValues oldValues;
        TestValues newValues;
        std::vector<std::size_t> mutationPoints;
    };

    std::optional<Scenario> scenario;
    {
        HostStorage storage;
        NvsStateStore store;
        TestValues currentValues = prefillInventory(store);
        for (std::size_t rotation = 0U;
             rotation < kRotationLimit && !scenario.has_value(); ++rotation) {
            const auto& record = rotatingRecord(rotation);
            const auto oldValues = currentValues;
            storage.device.clearTrace();
            const auto replacement = rotateValue(record, rotation);
            const auto result = store.write(key(record.key), replacement);
            REQUIRE((result == StateStoreWriteStatus::Success ||
                     result == StateStoreWriteStatus::CommitOutcomeUnknown));
            currentValues[recordIndex(record)] = replacement;
            const bool eraseObserved = std::any_of(
                storage.device.trace().begin(), storage.device.trace().end(),
                [](const auto& event) {
                    return event.operation ==
                           issue90_host::BlockOperation::Erase;
                });
            if (eraseObserved) {
                scenario =
                    Scenario{rotation, &record, oldValues, currentValues,
                             relevantMutationCallbacks(storage.device.trace())};
            }
        }
    }
    REQUIRE(scenario.has_value());

    std::vector<std::size_t> cutCallbacks = scenario->mutationPoints;
    if (!g_exhaustive) {
        cutCallbacks = {cutCallbacks.front(), cutCallbacks.back(),
                        cutCallbacks[cutCallbacks.size() / 2U]};
        std::sort(cutCallbacks.begin(), cutCallbacks.end());
        cutCallbacks.erase(
            std::unique(cutCallbacks.begin(), cutCallbacks.end()),
            cutCallbacks.end());
    }
    for (const std::size_t callback : cutCallbacks) {
        HostStorage cutStorage;
        NvsStateStore cutStore;
        static_cast<void>(prefillInventory(cutStore));
        replayRotations(cutStore, scenario->rotation);
        cutStorage.device.clearTrace();
        cutStorage.device.failFromCallback(callback);
        const auto result =
            cutStore.write(key(scenario->record->key),
                           scenario->newValues[recordIndex(*scenario->record)]);
        REQUIRE(result == StateStoreWriteStatus::CommitOutcomeUnknown);
        cutStorage.device.clearFailure();
        cutStorage.reinitialize();
        for (std::size_t index = 0U; index < kRecordInventory.size(); ++index) {
            requireOldOrNew(cutStore, key(kRecordInventory[index].key),
                            scenario->oldValues[index],
                            scenario->newValues[index]);
        }
    }

    HostStorage commitStorage;
    NvsStateStore commitStore;
    static_cast<void>(prefillInventory(commitStore));
    replayRotations(commitStore, scenario->rotation);
    NvsApiFaultSeam::failCommit(ESP_ERR_FLASH_OP_FAIL);
    REQUIRE(commitStore.write(
                key(scenario->record->key),
                scenario->newValues[recordIndex(*scenario->record)]) ==
            StateStoreWriteStatus::CommitOutcomeUnknown);
    NvsApiFaultSeam::reset();
    commitStorage.reinitialize();
    for (std::size_t index = 0U; index < kRecordInventory.size(); ++index) {
        requireOldOrNew(commitStore, key(kRecordInventory[index].key),
                        scenario->oldValues[index], scenario->newValues[index]);
    }

    nvs_stats_t stats{};
    REQUIRE(nvs_get_stats(kPartition, &stats) == ESP_OK);
    REQUIRE(stats.total_entries > 0U);
    REQUIRE(stats.available_entries > 0U);
}

int runHostTests(bool exhaustive, bool ciRegression, std::uint32_t seed,
                 const char* reportPath) {
    g_exhaustive = exhaustive;

    struct TestCase {
        const char* name;
        void (*run)();
    };
    const TestCase tests[]{
        {"binary-empty", testBinaryAndEmptyValues},
        {"bounded-read", testBoundedTwoStageReadContract},
        {"blob-boundaries", testBlobBoundariesAndPatterns},
        {"maximum-record", testMaximumRecordReadLimit},
        {"error-mapping", testErrorMapping},
        {"old-or-new-bdl-cut", testOldOrNewAfterBdlCuts},
        {"prefilled-gc-erase", testPrefilledGcAndEraseWorkload}};

    std::cout << "ISSUE90_HOST_BEGIN mode="
              << (exhaustive ? "exhaustive" : (ciRegression ? "ci" : "default"))
              << " seed=" << seed << '\n';
    for (const auto& test : tests) {
        try {
            test.run();
            std::cout << "PASS " << test.name << '\n';
        } catch (const std::exception& error) {
            std::cerr << "FAIL " << test.name << ": " << error.what() << '\n';
            return 1;
        }
    }
    std::cout << "ISSUE90_HOST_PASS\n";
    if (reportPath != nullptr) {
        const char* sourceSha = std::getenv("SOURCE_GIT_SHA");
        std::ofstream report(reportPath);
        if (!report) {
            std::cerr << "cannot open report: " << reportPath << '\n';
            return 1;
        }
        report << "{\n"
               << "  \"idf_sha\": \"" << kIdfSha << "\",\n"
               << "  \"source_git_sha\": \""
               << (sourceSha == nullptr ? "working-tree" : sourceSha) << "\",\n"
               << "  \"mode\": \""
               << (exhaustive ? "exhaustive"
                              : (ciRegression ? "ci-regression" : "default"))
               << "\",\n"
               << "  \"seed\": " << seed << ",\n"
               << "  \"status\": \"PASS\",\n"
               << "  \"tests\": [\"binary-empty\", \"bounded-read\", "
                  "\"blob-boundaries\", \"maximum-record\", "
                  "\"error-mapping\", \"old-or-new-bdl-cut\", "
                  "\"prefilled-gc-erase\"]\n"
               << "}\n";
    }
    return 0;
}

extern "C" void app_main(void) {
    const char* mode = std::getenv("ISSUE90_HOST_MODE");
    const bool exhaustive =
        mode != nullptr && std::string(mode) == "exhaustive";
    const bool ciRegression =
        mode != nullptr && std::string(mode) == "ci-regression";
    const char* reportPath = std::getenv("ISSUE90_HOST_REPORT");
    std::exit(runHostTests(exhaustive, ciRegression, 0U, reportPath));
}
