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
constexpr std::size_t kMaximumProgramRecordBytes = 32813U;
constexpr char kIdfSha[] = "7101770dc6db2667b3c477cc31365dd1acd6db4e";
bool g_exhaustive{false};

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

    NvsApiFaultSeam::failOpen(ESP_ERR_NVS_PART_NOT_FOUND);
    REQUIRE(store.write(stateKey, newValue) ==
            StateStoreWriteStatus::WriteError);
    NvsApiFaultSeam::reset();
    NvsApiFaultSeam::failOpen(ESP_ERR_NVS_NOT_FOUND);
    REQUIRE(store.read(stateKey, 32U).status == StateStoreReadStatus::NotFound);
    NvsApiFaultSeam::reset();
    NvsApiFaultSeam::failOpen(ESP_ERR_NVS_PART_NOT_FOUND);
    REQUIRE(store.read(stateKey, 32U).status ==
            StateStoreReadStatus::ReadError);
    NvsApiFaultSeam::reset();

    REQUIRE(store.write(stateKey, oldValue) == StateStoreWriteStatus::Success);
    NvsApiFaultSeam::failSizeQuery(ESP_ERR_NVS_INVALID_LENGTH);
    REQUIRE(store.read(stateKey, 32U).status ==
            StateStoreReadStatus::ReadError);
    NvsApiFaultSeam::reset();
    NvsApiFaultSeam::failRead(ESP_ERR_NVS_INVALID_LENGTH);
    REQUIRE(store.read(stateKey, 32U).status ==
            StateStoreReadStatus::ReadError);
    NvsApiFaultSeam::reset();

    NvsApiFaultSeam::failSet(ESP_ERR_NVS_VALUE_TOO_LONG);
    REQUIRE(store.write(stateKey, newValue) ==
            StateStoreWriteStatus::CapacityError);
    NvsApiFaultSeam::reset();
    NvsApiFaultSeam::failSet(ESP_ERR_NVS_REMOVE_FAILED);
    REQUIRE(store.write(stateKey, newValue) ==
            StateStoreWriteStatus::CommitOutcomeUnknown);
    NvsApiFaultSeam::reset();
    requireOldOrNew(store, stateKey, oldValue, newValue);

    NvsApiFaultSeam::failSet(ESP_ERR_FLASH_OP_FAIL, true);
    REQUIRE(store.write(stateKey, newValue) ==
            StateStoreWriteStatus::CommitOutcomeUnknown);
    NvsApiFaultSeam::reset();
    requireOldOrNew(store, stateKey, oldValue, newValue);

    NvsApiFaultSeam::failCommit(ESP_FAIL);
    REQUIRE(store.write(stateKey, newValue) ==
            StateStoreWriteStatus::CommitOutcomeUnknown);
}

void testOldOrNewAfterBdlCuts() {
    const std::array<std::uint8_t, 3U> patterns{0x00U, 0xa5U, 0xffU};
    const std::size_t repetitions = g_exhaustive ? patterns.size() : 1U;
    for (std::size_t repetition = 0U; repetition < repetitions; ++repetition) {
        const auto stateKey = key("cut");
        const std::string oldValue = bytes(8240U, patterns[repetition]);
        const std::string newValue =
            bytes(8240U, patterns[(repetition + 1U) % patterns.size()]);

        std::size_t mutationCallbacks = 0U;
        {
            HostStorage storage;
            NvsStateStore store;
            REQUIRE(store.write(stateKey, oldValue) ==
                    StateStoreWriteStatus::Success);
            storage.device.clearTrace();
            REQUIRE(store.write(stateKey, newValue) ==
                    StateStoreWriteStatus::Success);
            mutationCallbacks = storage.device.trace().size();
            REQUIRE(mutationCallbacks > 0U);
        }

        // These are logical end points in the successful 8,240-byte update
        // trace of the pinned v6.0.2 BDL implementation: early/later
        // BLOB_DATA chunks, BLOB_IDX publication, and old-version removal.
        // The CI set is a small deterministic sample; --exhaustive covers
        // all seven at all three fixed byte patterns.
        const std::array<std::size_t, 7U> exhaustiveCutCallbacks{
            6U, 15U, 19U, 21U, 22U, 28U, 35U};
        const std::array<std::size_t, 3U> ciCutCallbacks{15U, 22U, 35U};
        const std::vector<std::size_t> cutCallbacks =
            g_exhaustive
                ? std::vector<std::size_t>(exhaustiveCutCallbacks.begin(),
                                           exhaustiveCutCallbacks.end())
                : std::vector<std::size_t>(ciCutCallbacks.begin(),
                                           ciCutCallbacks.end());
        for (const std::size_t callback : cutCallbacks) {
            REQUIRE(callback <= mutationCallbacks);
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
    }
}

void testPrefilledGcAndEraseWorkload() {
    HostStorage storage;
    NvsStateStore store;
    const std::array<const char*, 22U> keys{
        "uc0", "uc1", "uc2", "uc3", "sc0", "sc1", "sc2", "sc3",
        "pc0", "pc1", "pc2", "pc3", "cm0", "cm1", "cm2", "cr0",
        "cr1", "cb0", "cb1", "rc0", "rc1", "rh0"};
    const std::array<std::size_t, 22U> sizes{
        301U,   301U, 301U, 301U, 45U,  45U,  45U, 45U, 32813U, 32813U, 32813U,
        32813U, 149U, 149U, 149U, 114U, 114U, 42U, 42U, 8240U,  8240U,  256U};

    for (std::size_t index = 0U; index < keys.size(); ++index) {
        REQUIRE(store.write(
                    key(keys[index]),
                    bytes(sizes[index], static_cast<std::uint8_t>(index))) ==
                StateStoreWriteStatus::Success);
    }

    bool eraseObserved = false;
    for (std::size_t rotation = 0U; rotation < 2048U && !eraseObserved;
         ++rotation) {
        const auto slot = rotation % 5U;
        const std::array<const char*, 5U> rotating{"pc0", "pc1", "rc0", "rc1",
                                                   "rh0"};
        const auto result =
            store.write(key(rotating[slot]),
                        bytes(sizes[8U + (slot < 4U ? slot : 13U)],
                              static_cast<std::uint8_t>(rotation + 3U)));
        REQUIRE((result == StateStoreWriteStatus::Success ||
                 result == StateStoreWriteStatus::CommitOutcomeUnknown));
        for (const auto& event : storage.device.trace()) {
            if (event.operation == issue90_host::BlockOperation::Erase) {
                eraseObserved = true;
                break;
            }
        }
        storage.device.clearTrace();
    }
    REQUIRE(eraseObserved);

    nvs_stats_t stats{};
    REQUIRE(nvs_get_stats(kPartition, &stats) == ESP_OK);
    REQUIRE(stats.total_entries > 0U);
    REQUIRE(stats.available_entries > 0U);
}

int main(int argc, char** argv) {
    bool exhaustive = false;
    bool ciRegression = false;
    std::uint32_t seed = 0U;
    std::string reportPath;
    for (int index = 1; index < argc; ++index) {
        const std::string argument(argv[index]);
        if (argument == "--exhaustive") {
            exhaustive = true;
        } else if (argument == "--ci-regression") {
            ciRegression = true;
        } else if (argument == "--seed" && index + 1 < argc) {
            seed = static_cast<std::uint32_t>(std::stoul(argv[++index]));
        } else if (argument == "--report" && index + 1 < argc) {
            reportPath = argv[++index];
        } else if (argument != "--help") {
            std::cerr << "unknown argument: " << argument << '\n';
            return 2;
        }
    }

    if (argc == 2 && std::string(argv[1]) == "--help") {
        std::cout << "usage: issue_90_nvs_adapter_host [--ci-regression] "
                     "[--exhaustive --seed N] [--report PATH]\n";
        return 0;
    }

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
    if (!reportPath.empty()) {
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
