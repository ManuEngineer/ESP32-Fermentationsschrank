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

#include "esp_heap_caps.h"
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
constexpr std::size_t kPageReadChunk = 256U;
constexpr TickType_t kUartWait = pdMS_TO_TICKS(1000U);

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

using Sha256 = std::array<std::uint8_t, 32U>;

struct PageEvidence final {
    bool valid{false};
    bool allErased{true};
    bool headerCrcValid{false};
    std::uint32_t state{0xffffffffU};
    std::uint32_t sequence{0xffffffffU};
    std::uint16_t writtenEntries{0U};
    std::uint16_t erasedEntries{0U};
    std::uint32_t keyMask{0U};
    Sha256 digest{};
};

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

void parsePageChunk(PageEvidence& evidence, std::size_t pageOffset,
                    const std::uint8_t* bytes, std::size_t length) {
    const auto contains = [pageOffset, length](std::size_t offset,
                                               std::size_t size) {
        return offset >= pageOffset && offset + size <= pageOffset + length;
    };
    if (contains(0U, 32U)) {
        evidence.state = littleEndian32(bytes);
        evidence.sequence = littleEndian32(bytes + 4U);
        const auto expected = littleEndian32(bytes + 28U);
        evidence.headerCrcValid =
            esp_rom_crc32_le(0xffffffffU, bytes + 4U, 24U) == expected;
    }
    for (std::size_t entry = 0U; entry < 126U; ++entry) {
        const auto stateOffset = 32U + entry / 4U;
        if (contains(stateOffset, 1U)) {
            const auto stateByte = bytes[stateOffset - pageOffset];
            const auto state = static_cast<std::uint8_t>(
                (stateByte >> ((entry % 4U) * 2U)) & 0x03U);
            if (state == kEntryStateWritten) {
                ++evidence.writtenEntries;
            } else if (state == kEntryStateErased) {
                ++evidence.erasedEntries;
            }
        }
        const auto dataOffset = 64U + entry * 32U;
        if (contains(dataOffset, 24U)) {
            const auto keyIndex = inventoryKeyIndex(bytes + dataOffset + 8U);
            if (keyIndex.has_value()) {
                evidence.keyMask |= 1UL << *keyIndex;
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
    explicit HardwareHarness(const esp_partition_t* partition)
        : partition_(partition) {}

    bool prefill(std::uint8_t seed) {
        for (std::size_t index = 0U; index < kKeys.size(); ++index) {
            if (!write(kKeys[index],
                       makeValue(kSizes[index],
                                 static_cast<std::uint8_t>(seed + index)))) {
                return false;
            }
        }
        if (!snapshotPages(previousPages_)) return false;
        ESP_LOGI(kTag, "PREFILL_DONE keys=%u seed=%u", kKeys.size(), seed);
        logStats("PREFILL_STATS");
        logResources("PREFILL_RESOURCE");
        return true;
    }

    bool rotate(std::size_t maximumWrites) {
        const auto limit = maximumWrites == 0U ? kRotationLimit : maximumWrites;
        for (std::size_t rotation = 0U; rotation < limit; ++rotation) {
            const auto slot = rotation % kRotatingKeys.size();
            const auto replacement = makeValue(
                kRotatingSizes[slot], static_cast<std::uint8_t>(rotation + 3U));
            Sha256 replacementHash{};
            if (!sha256(replacement.data(), replacement.size(),
                        replacementHash)) {
                ESP_LOGE(kTag, "ROTATE_WRITE_FAIL rotation=%u reason=hash",
                         rotation);
                return false;
            }
            ESP_LOGI(kTag,
                     "ROTATE_BEGIN key=%s rotation=%u max_writes=%u "
                     "new_sha256=%s",
                     kRotatingKeys[slot], rotation, limit,
                     hex(replacementHash).c_str());
            if (!write(kRotatingKeys[slot], replacement)) {
                ESP_LOGE(kTag, "ROTATE_WRITE_FAIL rotation=%u slot=%u",
                         rotation, slot);
                return false;
            }
            if (detectGcErase()) {
                ESP_LOGI(kTag, "GC_ERASE_DETECTED rotation=%u", rotation);
                logStats("GC_STATS");
                logResources("GC_RESOURCE");
                ESP_LOGI(kTag, "ROTATE_RESULT status=PASS writes=%u",
                         rotation + 1U);
                return true;
            }
        }
        ESP_LOGE(kTag,
                 "ROTATE_RESULT status=FAIL reason=GC_ERASE_NOT_OBSERVED");
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
            ESP_LOGI(kTag, "READBACK key=%s status=%u bytes=%u sha256=%s",
                     kKeys[index], static_cast<unsigned>(result.status),
                     result.value.size(), hashed ? hex(digest).c_str() : "-");
            pass = pass && hashed && result.value.size() == kSizes[index];
        }
        logStats("READBACK_STATS");
        logResources("READBACK_RESOURCE");
        ESP_LOGI(kTag, "READBACK_RESULT status=%s", pass ? "PASS" : "FAIL");
        return pass;
    }

    bool cutArmed(const char* token) const {
        ESP_LOGI(kTag, "CUT_ARMED token=%s", token == nullptr ? "-" : token);
        return true;
    }

   private:
    bool write(const char* name, const std::string& value) {
        const auto stateKey = makeKey(name);
        if (!stateKey.has_value()) return false;
        const auto result = store_.write(*stateKey, value);
        return result == device_platform::StateStoreWriteStatus::Success ||
               result ==
                   device_platform::StateStoreWriteStatus::CommitOutcomeUnknown;
    }

    bool snapshotPages(std::array<PageEvidence, kPageCount>& target) const {
        for (std::size_t page = 0U; page < kPageCount; ++page) {
            PageEvidence evidence{};
            psa_hash_operation_t hashOperation = PSA_HASH_OPERATION_INIT;
            if (psa_hash_setup(&hashOperation, PSA_ALG_SHA_256) !=
                PSA_SUCCESS) {
                return false;
            }
            bool readPass = true;
            for (std::size_t offset = 0U; offset < kPageSize;
                 offset += kPageReadChunk) {
                const auto length =
                    std::min(kPageReadChunk, kPageSize - offset);
                const auto result =
                    esp_partition_read(partition_, page * kPageSize + offset,
                                       readBuffer_.data(), length);
                if (result != ESP_OK ||
                    psa_hash_update(&hashOperation, readBuffer_.data(),
                                    length) != PSA_SUCCESS) {
                    ESP_LOGE(kTag,
                             "PARTITION_READ_FAIL page=%u offset=%u err=%s",
                             page, offset, esp_err_to_name(result));
                    readPass = false;
                    break;
                }
                for (std::size_t byte = 0U; byte < length; ++byte) {
                    evidence.allErased =
                        evidence.allErased && readBuffer_[byte] == 0xffU;
                }
                parsePageChunk(evidence, offset, readBuffer_.data(), length);
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
                             evidence.headerCrcValid;
            target[page] = evidence;
        }
        return true;
    }

    bool detectGcErase() {
        if (!snapshotPages(currentPages_)) return false;
        nvs_stats_t stats{};
        if (nvs_get_stats(kPartition, &stats) != ESP_OK) return false;
        bool detected = false;
        for (std::size_t page = 0U; page < kPageCount; ++page) {
            const auto& before = previousPages_[page];
            const auto& after = currentPages_[page];
            if (!before.valid || before.writtenEntries == 0U ||
                before.allErased || !after.allErased ||
                std::memcmp(before.digest.data(), after.digest.data(),
                            before.digest.size()) == 0) {
                continue;
            }
            for (std::size_t newPage = 0U; newPage < kPageCount; ++newPage) {
                const auto& candidate = currentPages_[newPage];
                if (!candidate.valid || candidate.allErased ||
                    candidate.writtenEntries == 0U ||
                    !newerSequence(candidate.sequence, before.sequence) ||
                    (candidate.keyMask & before.keyMask) == 0U) {
                    continue;
                }
                if (!readbackAll()) return false;
                ESP_LOGI(kTag,
                         "GC_ERASE_DETECTED old_page=%u old_seq=%u "
                         "old_written=%u old_sha256=%s new_page=%u "
                         "new_seq=%u new_written=%u new_key_mask=0x%08x "
                         "new_sha256=%s stats_total=%u stats_free=%u",
                         page, before.sequence, before.writtenEntries,
                         hex(before.digest).c_str(), newPage,
                         candidate.sequence, candidate.writtenEntries,
                         candidate.keyMask, hex(candidate.digest).c_str(),
                         stats.total_entries, stats.free_entries);
                detected = true;
                break;
            }
            if (detected) break;
        }
        previousPages_.swap(currentPages_);
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
    std::array<PageEvidence, kPageCount> previousPages_{};
    std::array<PageEvidence, kPageCount> currentPages_{};
    mutable std::array<std::uint8_t, kPageReadChunk> readBuffer_{};
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

std::optional<std::size_t> parseRotateCommand(std::string_view line) {
    constexpr std::string_view prefix{"ROTATE max_writes="};
    if (line.rfind(prefix, 0U) != 0U || line.size() == prefix.size()) {
        return std::nullopt;
    }
    std::size_t result = 0U;
    for (std::size_t index = prefix.size(); index < line.size(); ++index) {
        const char byte = line[index];
        if (byte < '0' || byte > '9' ||
            result > (SIZE_MAX - static_cast<std::size_t>(byte - '0')) / 10U) {
            return std::nullopt;
        }
        result = result * 10U + static_cast<std::size_t>(byte - '0');
    }
    return result == 0U ? kRotationLimit : result;
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
    const auto rotateLarge = parseRotateCommand("ROTATE max_writes=2048");
    const auto token = parseCutArmCommand("CUT_ARM token=issue90-0");
    return rotate.has_value() && *rotate == 1U && rotateLarge.has_value() &&
           *rotateLarge == 2048U && token.has_value() &&
           *token == "issue90-0" &&
           !parseRotateCommand("ROTATE max_writes=").has_value() &&
           !parseRotateCommand("ROTATE max_writes=1x").has_value() &&
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
    HardwareHarness harness(partition);
    const auto harnessActive = captureResources();
    logResourceSnapshot("HARNESS_RESOURCE_ACTIVE", harnessActive,
                        &harnessBaseline);
    ESP_LOGI(kTag,
             "READY partition=%s namespace=%s pages=%u page_bytes=%u "
             "profile=bringup idf=6.0.2 "
             "idf_sha=7101770dc6db2667b3c477cc31365dd1acd6db4e",
             kPartition, kNamespace, kPageCount, kPageSize);
    bool pass = true;
    std::string line;
    for (;;) {
        if (!readCommand(line)) continue;
        if (line == "PREFILL seed=0") {
            pass = harness.prefill(0U) && pass;
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
            ESP_LOGI(kTag, "REBOOTING");
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
    ESP_LOGI(kTag, "%s", pass ? "PASS" : "FAIL");
    return pass;
}

}  // namespace device_platform_esp_idf

#endif
