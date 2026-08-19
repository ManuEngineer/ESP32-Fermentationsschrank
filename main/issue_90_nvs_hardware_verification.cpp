#include "issue_90_nvs_hardware_verification.hpp"

#if defined(APP_ISSUE_90_NVS_HARDWARE_TEST)

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "esp_heap_caps.h"
#include "esp_flash.h"
#include "esp_log.h"
#include "esp_partition.h"
#include "esp_rom_crc.h"
#include "esp_system.h"
#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "psa/crypto.h"

#include "nvs_state_store.hpp"
#include "state_store.hpp"
#include "state_store_key.hpp"

namespace device_platform_esp_idf {
namespace {

constexpr char kTag[] = "issue90_hw";
constexpr char kPartition[] = "state_store";
constexpr char kNamespace[] = "fermentation";
constexpr std::size_t kPageSize = 4096U;
constexpr std::size_t kPageCount = 69U;
constexpr std::size_t kRotationLimit = 2048U;
constexpr std::size_t kPageReadChunk = kPageSize;
constexpr std::size_t kHarnessScratchBudgetBytes = 16U * 1024U;
constexpr TickType_t kUartWait = pdMS_TO_TICKS(1000U);
constexpr std::uint32_t kPartitionOffset = 0x10000U;
constexpr std::uint32_t kExpectedPartitionType = ESP_PARTITION_TYPE_DATA;
constexpr std::uint32_t kExpectedPartitionSubtype =
    ESP_PARTITION_SUBTYPE_DATA_NVS;

#ifndef APP_ISSUE_90_SOURCE_GIT_SHA
#error "Issue-90 harness requires APP_ISSUE_90_SOURCE_GIT_SHA"
#endif
#ifndef APP_ISSUE_90_PLAN_SHA
#error "Issue-90 harness requires APP_ISSUE_90_PLAN_SHA"
#endif

constexpr char kSourceGitSha[] = APP_ISSUE_90_SOURCE_GIT_SHA;
constexpr char kPlanSha[] = APP_ISSUE_90_PLAN_SHA;
constexpr char kIdfSha[] = "7101770dc6db2667b3c477cc31365dd1acd6db4e";

constexpr std::array<const char*, 22U> kKeys{
    "uc0", "uc1", "uc2", "uc3", "sc0", "sc1", "sc2", "sc3",
    "pc0", "pc1", "pc2", "pc3", "cm0", "cm1", "cm2", "cr0",
    "cr1", "cb0", "cb1", "rc0", "rc1", "rh0"};
constexpr std::array<std::size_t, 22U> kSizes{
    301U,   301U, 301U, 301U, 45U,  45U,  45U, 45U, 32813U, 32813U, 32813U,
    32813U, 149U, 149U, 149U, 114U, 114U, 42U, 42U, 8240U,  8240U,  256U};
constexpr std::array<const char*, 5U> kRotatingKeys{"pc0", "pc1", "rc0", "rc1",
                                                    "rh0"};
constexpr std::array<std::size_t, 5U> kRotatingSizes{32813U, 32813U, 8240U,
                                                     8240U, 256U};

struct RotateCommand final {
    std::size_t maxWrites;
    std::size_t targetRotation;
};

using Sha256 = std::array<std::uint8_t, 32U>;

const char* writeStatusName(device_platform::StateStoreWriteStatus status) {
    using device_platform::StateStoreWriteStatus;
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
    return "Unknown";
}

const char* commitStatusName(device_platform::StateStoreWriteStatus status) {
    return status == device_platform::StateStoreWriteStatus::Success ? "Success"
           : status ==
                   device_platform::StateStoreWriteStatus::CommitOutcomeUnknown
               ? "Unknown"
               : "NotCommitted";
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

struct PageEvidence final {
    bool valid{false};
    bool allErased{true};
    bool headerCrcValid{false};
    bool entriesCrcValid{true};
    std::uint32_t state{0xffffffffU};
    std::uint32_t sequence{0xffffffffU};
    std::uint16_t writtenEntries{0U};
    std::uint16_t erasedEntries{0U};
    std::uint32_t liveKeyMask{0U};
    std::uint32_t blobDataMask{0U};
    std::uint32_t blobIndexMask{0U};
    std::uint32_t removedKeyMask{0U};
    std::array<std::uint8_t, 22U> liveEntryCounts{};
    Sha256 digest{};
};

std::string entryCounts(const PageEvidence& evidence) {
    std::string result;
    for (std::size_t index = 0U; index < evidence.liveEntryCounts.size();
         ++index) {
        if (index != 0U) result.push_back(',');
        result += std::to_string(evidence.liveEntryCounts[index]);
    }
    return result;
}

struct HarnessScratch final {
    std::array<PageEvidence, kPageCount> previousPages{};
    std::array<PageEvidence, kPageCount> currentPages{};
    std::array<std::uint8_t, kPageReadChunk> readBuffer{};
};

static_assert(
    sizeof(HarnessScratch) <= kHarnessScratchBudgetBytes,
    "Issue-90 page evidence exceeds the fixed no-PSRAM scratch budget");

// Deliberately static internal BSS: these arrays must never be automatic
// objects on the ESP-IDF main-task stack.
HarnessScratch gHarnessScratch{};

constexpr std::uint32_t kPageStateActive = 0xfffffffeU;
constexpr std::uint32_t kPageStateFull = 0xfffffffcU;
constexpr std::uint32_t kPageStateFreeing = 0xfffffff8U;
constexpr std::uint8_t kEntryStateWritten = 0x02U;
constexpr std::uint8_t kEntryStateErased = 0x00U;

std::uint32_t littleEndian32(const std::uint8_t* bytes) {
    return static_cast<std::uint32_t>(bytes[0]) |
           (static_cast<std::uint32_t>(bytes[1]) << 8U) |
           (static_cast<std::uint32_t>(bytes[2]) << 16U) |
           (static_cast<std::uint32_t>(bytes[3]) << 24U);
}

std::optional<std::size_t> inventoryKeyIndex(const std::uint8_t* keyBytes) {
    for (std::size_t index = 0U; index < kKeys.size(); ++index) {
        const auto length = std::strlen(kKeys[index]);
        if (std::memcmp(keyBytes, kKeys[index], length) == 0 &&
            keyBytes[length] == 0U) {
            return index;
        }
    }
    return std::nullopt;
}

bool newerSequence(std::uint32_t current, std::uint32_t previous) {
    return current != previous &&
           static_cast<std::int32_t>(current - previous) > 0;
}

bool entryHeaderCrcValid(const std::uint8_t* entry) {
    const auto stored = littleEndian32(entry + 4U);
    auto calculated = esp_rom_crc32_le(0xffffffffU, entry, 4U);
    calculated = esp_rom_crc32_le(calculated, entry + 8U, 16U);
    calculated = esp_rom_crc32_le(calculated, entry + 24U, 8U);
    return calculated == stored;
}

void parsePage(PageEvidence& evidence, const std::uint8_t* bytes) {
    evidence.state = littleEndian32(bytes);
    evidence.sequence = littleEndian32(bytes + 4U);
    const auto expected = littleEndian32(bytes + 28U);
    evidence.headerCrcValid =
        esp_rom_crc32_le(0xffffffffU, bytes + 4U, 24U) == expected;
    for (std::size_t entry = 0U; entry < 126U; ++entry) {
        const auto stateOffset = 32U + entry / 4U;
        const auto stateByte = bytes[stateOffset];
        const auto state = static_cast<std::uint8_t>(
            (stateByte >> ((entry % 4U) * 2U)) & 0x03U);
        const auto dataOffset = 64U + entry * 32U;
        const auto crcValid = entryHeaderCrcValid(bytes + dataOffset);
        if ((state == kEntryStateWritten || state == kEntryStateErased) &&
            !crcValid) {
            evidence.entriesCrcValid = false;
        }
        const auto keyIndex = crcValid
                                  ? inventoryKeyIndex(bytes + dataOffset + 8U)
                                  : std::nullopt;
        if (state == kEntryStateWritten) {
            ++evidence.writtenEntries;
            if (keyIndex.has_value()) {
                const auto mask = 1UL << *keyIndex;
                evidence.liveKeyMask |= mask;
                ++evidence.liveEntryCounts[*keyIndex];
                const auto type = bytes[dataOffset + 1U];
                if (type == 0x42U) evidence.blobDataMask |= mask;
                if (type == 0x48U) evidence.blobIndexMask |= mask;
            }
        } else if (state == kEntryStateErased) {
            ++evidence.erasedEntries;
            if (keyIndex.has_value()) {
                evidence.removedKeyMask |= 1UL << *keyIndex;
            }
        }
    }
}

std::string makeValue(std::size_t size, std::uint8_t seed) {
    std::string value(size, '\0');
    for (std::size_t index = 0U; index < size; ++index) {
        value[index] = static_cast<char>(seed + (index % 251U));
    }
    return value;
}

bool sha256(const void* data, std::size_t length, Sha256& output) {
    const auto status = psa_hash_compute(
        PSA_ALG_SHA_256, static_cast<const std::uint8_t*>(data), length,
        output.data(), output.size(), nullptr);
    return status == PSA_SUCCESS;
}

std::string hex(const Sha256& digest) {
    constexpr char digits[] = "0123456789abcdef";
    std::string result;
    result.reserve(digest.size() * 2U);
    for (const auto byte : digest) {
        result.push_back(digits[byte >> 4U]);
        result.push_back(digits[byte & 0x0fU]);
    }
    return result;
}

std::optional<device_platform::StateStoreKey> makeKey(const char* value) {
    const auto created = device_platform::StateStoreKey::create(value);
    if (created.status != device_platform::StateStoreKeyStatus::Success ||
        !created.key.has_value()) {
        ESP_LOGE(kTag, "invalid compile-time test key: %s", value);
        return std::nullopt;
    }
    return *created.key;
}

struct ResourceSnapshot final {
    std::size_t freeHeapBytes;
    std::size_t largestFreeBlockBytes;
    UBaseType_t stackHighWaterMarkWords;
};

ResourceSnapshot captureResources() {
    return {esp_get_free_heap_size(),
            heap_caps_get_largest_free_block(MALLOC_CAP_8BIT),
            uxTaskGetStackHighWaterMark(nullptr)};
}

void logResourceSnapshot(const char* marker, const ResourceSnapshot& snapshot,
                         const ResourceSnapshot* baseline) {
    if (baseline == nullptr) {
        ESP_LOGI(kTag,
                 "%s free_heap_bytes=%u largest_free_block_bytes=%u "
                 "stack_hwm_words=%u",
                 marker, snapshot.freeHeapBytes, snapshot.largestFreeBlockBytes,
                 static_cast<unsigned>(snapshot.stackHighWaterMarkWords));
        return;
    }
    ESP_LOGI(kTag,
             "%s free_heap_bytes=%u largest_free_block_bytes=%u "
             "stack_hwm_words=%u delta_heap_bytes=%d "
             "delta_largest_block_bytes=%d delta_stack_hwm_words=%d",
             marker, snapshot.freeHeapBytes, snapshot.largestFreeBlockBytes,
             static_cast<unsigned>(snapshot.stackHighWaterMarkWords),
             static_cast<int>(snapshot.freeHeapBytes) -
                 static_cast<int>(baseline->freeHeapBytes),
             static_cast<int>(snapshot.largestFreeBlockBytes) -
                 static_cast<int>(baseline->largestFreeBlockBytes),
             static_cast<int>(snapshot.stackHighWaterMarkWords) -
                 static_cast<int>(baseline->stackHighWaterMarkWords));
}

class HardwareHarness final {
   public:
    HardwareHarness(const esp_partition_t* partition,
                    NvsStateStoreConfig config, HarnessScratch& scratch)
        : partition_(partition), store_(std::move(config)), scratch_(scratch) {}

    bool prefill(std::uint8_t seed) {
        for (std::size_t index = 0U; index < kKeys.size(); ++index) {
            const auto value = makeValue(
                kSizes[index], static_cast<std::uint8_t>(seed + index));
            const auto status = write(kKeys[index], value);
            Sha256 digest{};
            const bool hashed = sha256(value.data(), value.size(), digest);
            ESP_LOGI(kTag,
                     "ISSUE90 PREFILL_WRITE protocol=1 key=%s status=%s "
                     "length=%u sha256=%s",
                     kKeys[index], writeStatusName(status),
                     static_cast<unsigned>(value.size()),
                     hashed ? hex(digest).c_str() : "-");
            if (status != device_platform::StateStoreWriteStatus::Success ||
                !hashed) {
                return false;
            }
        }
        if (!snapshotPages(scratch_.previousPages)) return false;
        operationSequence_ = 0U;
        ESP_LOGI(kTag,
                 "ISSUE90 PREFILL_DONE protocol=1 status=PASS keys=%u seed=%u",
                 kKeys.size(), seed);
        logStats("PREFILL_STATS");
        logResources("PREFILL_RESOURCE");
        return true;
    }

    bool rotate(RotateCommand command) {
        const auto maximumWrites = command.maxWrites;
        const auto targetRotation = command.targetRotation;
        const auto limit = maximumWrites == 0U ? kRotationLimit : maximumWrites;
        if (targetRotation >= limit) return false;
        for (std::size_t rotation = 0U; rotation < limit; ++rotation) {
            const auto slot = rotation % kRotatingKeys.size();
            const auto replacement = makeValue(
                kRotatingSizes[slot], static_cast<std::uint8_t>(rotation + 3U));
            Sha256 oldHash{};
            std::size_t oldLength = 0U;
            if (!readHash(kRotatingKeys[slot], kRotatingSizes[slot], oldHash,
                          oldLength)) {
                ESP_LOGE(
                    kTag,
                    "ISSUE90 ROTATE_RESULT protocol=1 status=FAIL sequence=%u "
                    "reason=old_readback",
                    static_cast<unsigned>(operationSequence_));
                return false;
            }
            Sha256 replacementHash{};
            if (!sha256(replacement.data(), replacement.size(),
                        replacementHash)) {
                ESP_LOGE(
                    kTag,
                    "ISSUE90 ROTATE_RESULT protocol=1 status=FAIL rotation=%u "
                    "reason=hash",
                    rotation);
                return false;
            }
            if (rotation < targetRotation) {
                const auto preparationStatus =
                    write(kRotatingKeys[slot], replacement);
                if (preparationStatus !=
                    device_platform::StateStoreWriteStatus::Success) {
                    ESP_LOGE(kTag,
                             "ISSUE90 ROTATE_RESULT protocol=1 status=FAIL "
                             "reason=target_preparation rotation=%u status=%s",
                             static_cast<unsigned>(rotation),
                             writeStatusName(preparationStatus));
                    return false;
                }
                if (detectGcErase()) {
                    ESP_LOGI(kTag,
                             "ISSUE90 ROTATE_PREPARATION_GC protocol=1 "
                             "rotation=%u",
                             static_cast<unsigned>(rotation));
                }
                continue;
            }
            ESP_LOGI(kTag,
                     "ISSUE90 ROTATE_BEGIN protocol=1 sequence=%u key=%s "
                     "rotation=%u target_rotation=%u max_writes=%u "
                     "old_length=%u old_sha256=%s new_length=%u new_sha256=%s",
                     static_cast<unsigned>(operationSequence_),
                     kRotatingKeys[slot], rotation,
                     static_cast<unsigned>(targetRotation), limit,
                     static_cast<unsigned>(oldLength), hex(oldHash).c_str(),
                     static_cast<unsigned>(replacement.size()),
                     hex(replacementHash).c_str());
            const auto sequence = operationSequence_++;
            const auto status = write(kRotatingKeys[slot], replacement);
            if (status != device_platform::StateStoreWriteStatus::Success) {
                ESP_LOGE(
                    kTag,
                    "ISSUE90 ROTATE_RESULT protocol=1 status=FAIL sequence=%u "
                    "key=%s set_status=%s commit_status=%s",
                    static_cast<unsigned>(sequence), kRotatingKeys[slot],
                    writeStatusName(status), commitStatusName(status));
                return false;
            }
            if (detectGcErase()) {
                ESP_LOGI(
                    kTag,
                    "ISSUE90 ROTATE_RESULT protocol=1 status=PASS sequence=%u "
                    "key=%s set_status=Success commit_status=Success "
                    "rotation=%u writes=%u",
                    static_cast<unsigned>(sequence), kRotatingKeys[slot],
                    static_cast<unsigned>(rotation),
                    static_cast<unsigned>(rotation + 1U));
                logStats("GC_STATS");
                logResources("GC_RESOURCE");
                return true;
            }
            ESP_LOGI(kTag,
                     "ISSUE90 ROTATE_RESULT protocol=1 status=PASS sequence=%u "
                     "key=%s "
                     "set_status=Success commit_status=Success rotation=%u",
                     static_cast<unsigned>(sequence), kRotatingKeys[slot],
                     static_cast<unsigned>(rotation));
        }
        ESP_LOGE(kTag,
                 "ISSUE90 ROTATE_RESULT protocol=1 status=FAIL "
                 "reason=GC_ERASE_NOT_OBSERVED");
        return false;
    }

    bool readbackAll() const {
        bool pass = true;
        for (std::size_t index = 0U; index < kKeys.size(); ++index) {
            const auto stateKey = makeKey(kKeys[index]);
            if (!stateKey.has_value()) return false;
            const auto result = store_.read(*stateKey, kSizes[index]);
            Sha256 digest{};
            const bool hashed =
                result.status ==
                    device_platform::StateStoreReadStatus::Success &&
                sha256(result.value.data(), result.value.size(), digest);
            ESP_LOGI(kTag,
                     "ISSUE90 READBACK protocol=1 key=%s status=%s length=%u "
                     "sha256=%s",
                     kKeys[index], readStatusName(result.status),
                     static_cast<unsigned>(result.value.size()),
                     hashed ? hex(digest).c_str() : "-");
            pass = pass && hashed && result.value.size() == kSizes[index];
        }
        logStats("READBACK_STATS");
        logResources("READBACK_RESOURCE");
        ESP_LOGI(kTag, "ISSUE90 READBACK_RESULT protocol=1 status=%s count=%u",
                 pass ? "PASS" : "FAIL", kKeys.size());
        return pass;
    }

    bool pageEvidence() {
        if (!snapshotPages(scratch_.currentPages)) return false;
        for (std::size_t page = 0U; page < kPageCount; ++page) {
            const auto& evidence = scratch_.currentPages[page];
            ESP_LOGI(
                kTag,
                "ISSUE90 PAGE_EVIDENCE protocol=1 page=%u valid=%u "
                "all_erased=%u "
                "state=%08x sequence=%u written_entries=%u erased_entries=%u "
                "live_mask=%08x blob_data_mask=%08x blob_index_mask=%08x "
                "removed_mask=%08x header_crc_valid=%u entries_crc_valid=%u "
                "live_counts=%s sha256=%s",
                static_cast<unsigned>(page), evidence.valid ? 1U : 0U,
                evidence.allErased ? 1U : 0U,
                static_cast<unsigned>(evidence.state),
                static_cast<unsigned>(evidence.sequence),
                static_cast<unsigned>(evidence.writtenEntries),
                static_cast<unsigned>(evidence.erasedEntries),
                static_cast<unsigned>(evidence.liveKeyMask),
                static_cast<unsigned>(evidence.blobDataMask),
                static_cast<unsigned>(evidence.blobIndexMask),
                static_cast<unsigned>(evidence.removedKeyMask),
                evidence.headerCrcValid ? 1U : 0U,
                evidence.entriesCrcValid ? 1U : 0U,
                entryCounts(evidence).c_str(), hex(evidence.digest).c_str());
            ESP_LOGI(
                kTag, "ISSUE90 PAGE_LIVE_COUNTS protocol=1 page=%u counts=%s",
                static_cast<unsigned>(page), entryCounts(evidence).c_str());
        }
        ESP_LOGI(kTag,
                 "ISSUE90 PAGE_EVIDENCE_RESULT protocol=1 status=PASS pages=%u",
                 kPageCount);
        return true;
    }

    bool cutArmed(const char* token) const {
        ESP_LOGI(kTag, "ISSUE90 CUT_ARMED protocol=1 token=%s",
                 token == nullptr ? "-" : token);
        return true;
    }

    bool resetPartition() const {
        const auto deinitResult = nvs_flash_deinit_partition(kPartition);
        if (deinitResult != ESP_OK) {
            ESP_LOGE(kTag,
                     "ISSUE90 TEST_RESET protocol=1 status=FAIL phase=deinit "
                     "err=%s",
                     esp_err_to_name(deinitResult));
            return false;
        }
        const auto eraseResult =
            esp_partition_erase_range(partition_, 0U, partition_->size);
        const auto initResult = nvs_flash_init_partition(kPartition);
        if (eraseResult != ESP_OK || initResult != ESP_OK) {
            ESP_LOGE(kTag,
                     "ISSUE90 TEST_RESET protocol=1 status=FAIL erase=%s "
                     "init=%s",
                     esp_err_to_name(eraseResult), esp_err_to_name(initResult));
            return false;
        }
        ESP_LOGI(kTag,
                 "ISSUE90 TEST_RESET protocol=1 status=PASS label=%s "
                 "offset=%u size=%u",
                 partition_->label, static_cast<unsigned>(partition_->address),
                 static_cast<unsigned>(partition_->size));
        return true;
    }

   private:
    device_platform::StateStoreWriteStatus write(const char* name,
                                                 const std::string& value) {
        const auto stateKey = makeKey(name);
        if (!stateKey.has_value()) {
            return device_platform::StateStoreWriteStatus::WriteError;
        }
        const auto result = store_.write(*stateKey, value);
        return result;
    }

    bool readHash(const char* name, std::size_t maximumBytes, Sha256& digest,
                  std::size_t& length) const {
        const auto stateKey = makeKey(name);
        if (!stateKey.has_value()) return false;
        const auto result = store_.read(*stateKey, maximumBytes);
        if (result.status != device_platform::StateStoreReadStatus::Success) {
            return false;
        }
        length = result.value.size();
        return sha256(result.value.data(), result.value.size(), digest);
    }

    bool snapshotPages(std::array<PageEvidence, kPageCount>& target) const {
        for (std::size_t page = 0U; page < kPageCount; ++page) {
            PageEvidence evidence{};
            psa_hash_operation_t hashOperation = PSA_HASH_OPERATION_INIT;
            if (psa_hash_setup(&hashOperation, PSA_ALG_SHA_256) !=
                PSA_SUCCESS) {
                return false;
            }
            const auto result =
                esp_partition_read(partition_, page * kPageSize,
                                   scratch_.readBuffer.data(), kPageSize);
            const bool readPass =
                result == ESP_OK &&
                psa_hash_update(&hashOperation, scratch_.readBuffer.data(),
                                kPageSize) == PSA_SUCCESS;
            if (!readPass) {
                ESP_LOGE(kTag, "PARTITION_READ_FAIL page=%u err=%s", page,
                         esp_err_to_name(result));
            }
            if (readPass) {
                evidence.allErased = std::all_of(
                    scratch_.readBuffer.begin(), scratch_.readBuffer.end(),
                    [](std::uint8_t byte) { return byte == 0xffU; });
                parsePage(evidence, scratch_.readBuffer.data());
            }
            if (!readPass ||
                psa_hash_finish(&hashOperation, evidence.digest.data(),
                                evidence.digest.size(),
                                nullptr) != PSA_SUCCESS) {
                static_cast<void>(psa_hash_abort(&hashOperation));
                return false;
            }
            evidence.valid = !evidence.allErased &&
                             (evidence.state == kPageStateActive ||
                              evidence.state == kPageStateFull ||
                              evidence.state == kPageStateFreeing) &&
                             evidence.sequence != 0xffffffffU &&
                             evidence.headerCrcValid &&
                             evidence.entriesCrcValid;
            target[page] = evidence;
        }
        return true;
    }

    bool detectGcErase() {
        if (!snapshotPages(scratch_.currentPages)) return false;
        nvs_stats_t stats{};
        if (nvs_get_stats(kPartition, &stats) != ESP_OK) return false;
        bool detected = false;
        for (std::size_t page = 0U; page < kPageCount; ++page) {
            const auto& before = scratch_.previousPages[page];
            const auto& after = scratch_.currentPages[page];
            if (!before.valid || before.writtenEntries == 0U ||
                before.allErased || !after.allErased ||
                std::memcmp(before.digest.data(), after.digest.data(),
                            before.digest.size()) == 0) {
                continue;
            }
            for (std::size_t newPage = 0U; newPage < kPageCount; ++newPage) {
                const auto& candidate = scratch_.currentPages[newPage];
                if (!candidate.valid || candidate.allErased ||
                    candidate.writtenEntries == 0U ||
                    !newerSequence(candidate.sequence, before.sequence) ||
                    (candidate.liveKeyMask & before.liveKeyMask) !=
                        before.liveKeyMask ||
                    (candidate.blobDataMask & before.blobDataMask) !=
                        before.blobDataMask ||
                    (candidate.blobIndexMask & before.blobIndexMask) !=
                        before.blobIndexMask) {
                    continue;
                }
                bool countsMatch = true;
                for (std::size_t key = 0U; key < kKeys.size(); ++key) {
                    if (before.liveEntryCounts[key] != 0U &&
                        candidate.liveEntryCounts[key] <
                            before.liveEntryCounts[key]) {
                        countsMatch = false;
                        break;
                    }
                }
                if (!countsMatch ||
                    candidate.writtenEntries < before.writtenEntries) {
                    continue;
                }
                if (!readbackAll()) return false;
                ESP_LOGI(kTag,
                         "ISSUE90 GC_ERASE_DETECTED protocol=1 old_page=%u "
                         "old_state=%08x old_seq=%u old_written=%u "
                         "old_live_mask=%08x old_blob_data_mask=%08x "
                         "old_blob_index_mask=%08x old_sha256=%s new_page=%u "
                         "new_state=%08x new_seq=%u new_written=%u "
                         "new_live_mask=%08x new_blob_data_mask=%08x "
                         "new_blob_index_mask=%08x new_removed_mask=%08x "
                         "old_live_counts=%s new_live_counts=%s "
                         "new_sha256=%s stats_total=%u stats_used=%u "
                         "stats_free=%u stats_available=%u stats_namespaces=%u",
                         page, before.state, before.sequence,
                         before.writtenEntries, before.liveKeyMask,
                         before.blobDataMask, before.blobIndexMask,
                         hex(before.digest).c_str(), newPage, candidate.state,
                         candidate.sequence, candidate.writtenEntries,
                         candidate.liveKeyMask, candidate.blobDataMask,
                         candidate.blobIndexMask, candidate.removedKeyMask,
                         entryCounts(before).c_str(),
                         entryCounts(candidate).c_str(),
                         hex(candidate.digest).c_str(), stats.total_entries,
                         stats.used_entries, stats.free_entries,
                         stats.available_entries, stats.namespace_count);
                detected = true;
                break;
            }
            if (detected) break;
        }
        scratch_.previousPages.swap(scratch_.currentPages);
        return detected;
    }

    void logStats(const char* marker) const {
        nvs_stats_t stats{};
        const auto result = nvs_get_stats(kPartition, &stats);
        if (result != ESP_OK) {
            ESP_LOGE(kTag, "%s status=FAIL err=%s", marker,
                     esp_err_to_name(result));
            return;
        }
        ESP_LOGI(kTag,
                 "%s status=PASS used_entries=%u free_entries=%u "
                 "available_entries=%u total_entries=%u namespaces=%u",
                 marker, stats.used_entries, stats.free_entries,
                 stats.available_entries, stats.total_entries,
                 stats.namespace_count);
    }

    void logResources(const char* marker) const {
        const auto snapshot = captureResources();
        logResourceSnapshot(marker, snapshot, nullptr);
    }

    const esp_partition_t* partition_;
    NvsStateStore store_;
    HarnessScratch& scratch_;
    std::uint32_t operationSequence_{0U};
};

bool readCommand(std::string& line) {
    char byte = '\0';
    const int read = uart_read_bytes(UART_NUM_0, &byte, 1U, kUartWait);
    if (read != 1) return false;
    if (byte == '\r') return true;
    if (byte == '\n') return true;
    if (line.size() < 127U) line.push_back(byte);
    return false;
}

std::optional<RotateCommand> parseRotateCommand(std::string_view line) {
    constexpr std::string_view prefix{"ROTATE max_writes="};
    if (line.rfind(prefix, 0U) != 0U || line.size() == prefix.size()) {
        return std::nullopt;
    }
    const auto separator = line.find(" target_rotation=", prefix.size());
    const auto maxText =
        line.substr(prefix.size(), separator == std::string_view::npos
                                       ? line.size() - prefix.size()
                                       : separator - prefix.size());
    const auto targetText = separator == std::string_view::npos
                                ? std::string_view{"0"}
                                : line.substr(separator + 17U);
    if (maxText.empty() || targetText.empty()) return std::nullopt;
    std::size_t maximum = 0U;
    for (const char byte : maxText) {
        if (byte < '0' || byte > '9' ||
            maximum > (SIZE_MAX - static_cast<std::size_t>(byte - '0')) / 10U) {
            return std::nullopt;
        }
        maximum = maximum * 10U + static_cast<std::size_t>(byte - '0');
    }
    std::size_t target = 0U;
    for (const char byte : targetText) {
        if (byte < '0' || byte > '9' ||
            target > (SIZE_MAX - static_cast<std::size_t>(byte - '0')) / 10U) {
            return std::nullopt;
        }
        target = target * 10U + static_cast<std::size_t>(byte - '0');
    }
    return RotateCommand{maximum == 0U ? kRotationLimit : maximum, target};
}

std::optional<std::string_view> parseCutArmCommand(std::string_view line) {
    constexpr std::string_view prefix{"CUT_ARM token="};
    if (line.rfind(prefix, 0U) != 0U || line.size() == prefix.size()) {
        return std::nullopt;
    }
    const auto token = line.substr(prefix.size());
    if (token.find('=') != std::string_view::npos ||
        token.find_first_of(" \t\r\n") != std::string_view::npos) {
        return std::nullopt;
    }
    return token;
}

bool parserSelfTest() {
    const auto rotate = parseRotateCommand("ROTATE max_writes=1");
    const auto rotateLarge =
        parseRotateCommand("ROTATE max_writes=2048 target_rotation=7");
    const auto token = parseCutArmCommand("CUT_ARM token=issue90-0");
    return rotate.has_value() && rotate->maxWrites == 1U &&
           rotate->targetRotation == 0U && rotateLarge.has_value() &&
           rotateLarge->maxWrites == 2048U &&
           rotateLarge->targetRotation == 7U && token.has_value() &&
           *token == "issue90-0" &&
           !parseRotateCommand("ROTATE max_writes=").has_value() &&
           !parseRotateCommand("ROTATE max_writes=1x").has_value() &&
           !parseRotateCommand("ROTATE max_writes=1 target_rotation=x")
                .has_value() &&
           !parseCutArmCommand("CUT_ARM token==issue90-0").has_value();
}

}  // namespace

bool runIssue90NvsHardwareVerification() {
    if (!parserSelfTest()) {
        ESP_LOGE(kTag, "FAIL phase=parser_self_test; fail-closed");
        return false;
    }
    const auto* partition = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_NVS, kPartition);
    if (partition == nullptr) {
        ESP_LOGE(kTag, "FAIL phase=partition_lookup");
        return false;
    }
    const auto actualPages = partition->size / kPageSize;
    std::uint32_t flashSize = 0U;
    const auto flashSizeResult = esp_flash_get_size(nullptr, &flashSize);
    const bool partitionMatches =
        std::strncmp(partition->label, kPartition, sizeof(partition->label)) ==
            0 &&
        partition->type == kExpectedPartitionType &&
        partition->subtype == kExpectedPartitionSubtype &&
        partition->address == kPartitionOffset &&
        partition->size == kPageCount * kPageSize &&
        actualPages == kPageCount && flashSizeResult == ESP_OK &&
        static_cast<std::uint64_t>(partition->address) + partition->size <=
            flashSize;
    ESP_LOGI(kTag,
             "ISSUE90 PARTITION protocol=1 label=%s type=%u subtype=%u "
             "offset=%u size=%u pages=%u flash_size_bytes=%u status=%s",
             partition->label, static_cast<unsigned>(partition->type),
             static_cast<unsigned>(partition->subtype),
             static_cast<unsigned>(partition->address),
             static_cast<unsigned>(partition->size),
             static_cast<unsigned>(actualPages),
             static_cast<unsigned>(flashSize),
             partitionMatches ? "PASS" : "FAIL");
    if (!partitionMatches) {
        ESP_LOGE(kTag, "FAIL phase=partition_contract; fail-closed");
        return false;
    }
    const auto initResult = nvs_flash_init_partition(kPartition);
    if (initResult != ESP_OK) {
        ESP_LOGE(kTag, "FAIL phase=partition_init err=%s; fail-closed",
                 esp_err_to_name(initResult));
        return false;
    }
    if (psa_crypto_init() != PSA_SUCCESS) {
        ESP_LOGE(kTag, "FAIL phase=crypto_init; fail-closed");
        static_cast<void>(nvs_flash_deinit_partition(kPartition));
        return false;
    }

    const auto harnessBaseline = captureResources();
    logResourceSnapshot("HARNESS_RESOURCE_BASELINE", harnessBaseline, nullptr);
    HardwareHarness harness(partition,
                            NvsStateStoreConfig{kPartition, kNamespace},
                            gHarnessScratch);
    const auto harnessActive = captureResources();
    logResourceSnapshot("HARNESS_RESOURCE_ACTIVE", harnessActive,
                        &harnessBaseline);
    ESP_LOGI(kTag,
             "ISSUE90 READY protocol=1 idf_sha=%s source_sha=%s plan_sha=%s "
             "profile=bringup partition_label=%s namespace=%s "
             "partition_type=%u partition_subtype=%u offset=%u size=%u "
             "pages=%u page_bytes=%u flash_size_bytes=%u "
             "scratch_budget_bytes=%u",
             kIdfSha, kSourceGitSha, kPlanSha, partition->label, kNamespace,
             static_cast<unsigned>(partition->type),
             static_cast<unsigned>(partition->subtype),
             static_cast<unsigned>(partition->address),
             static_cast<unsigned>(partition->size),
             static_cast<unsigned>(actualPages),
             static_cast<unsigned>(kPageSize), static_cast<unsigned>(flashSize),
             static_cast<unsigned>(kHarnessScratchBudgetBytes));
    bool pass = true;
    std::string line;
    for (;;) {
        if (!readCommand(line)) continue;
        if (line == "PREFILL seed=0") {
            pass = harness.prefill(0U) && pass;
        } else if (line == "PAGE_EVIDENCE") {
            pass = harness.pageEvidence() && pass;
        } else if (line == "RESET_PARTITION") {
            pass = harness.resetPartition() && pass;
        } else if (const auto value = parseRotateCommand(line);
                   value.has_value()) {
            pass = harness.rotate(*value) && pass;
        } else if (line == "READBACK_ALL") {
            pass = harness.readbackAll() && pass;
        } else if (const auto token = parseCutArmCommand(line);
                   token.has_value()) {
            const std::string tokenText(*token);
            pass = harness.cutArmed(tokenText.c_str()) && pass;
        } else if (line == "REBOOT") {
            ESP_LOGI(kTag, "ISSUE90 REBOOT protocol=1 status=REQUESTED");
            esp_restart();
        } else if (line == "STOP") {
            break;
        } else if (!line.empty()) {
            ESP_LOGE(kTag, "FAIL phase=command command=%s", line.c_str());
            pass = false;
        }
        line.clear();
    }

    const auto deinitResult = nvs_flash_deinit_partition(kPartition);
    if (deinitResult != ESP_OK) {
        ESP_LOGE(kTag, "FAIL phase=partition_deinit err=%s",
                 esp_err_to_name(deinitResult));
        pass = false;
    }
    ESP_LOGI(kTag, "ISSUE90 COMPLETE protocol=1 status=%s",
             pass ? "PASS" : "FAIL");
    return pass;
}

}  // namespace device_platform_esp_idf

#endif
