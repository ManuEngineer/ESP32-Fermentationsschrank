#include "issue_90_nvs_hardware_verification.hpp"

#if defined(APP_ISSUE_90_NVS_HARDWARE_TEST)

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <string>
#include <vector>

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_partition.h"
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

bool allErased(const std::uint8_t* page) {
    for (std::size_t index = 0U; index < kPageSize; ++index) {
        if (page[index] != 0xffU) return false;
    }
    return true;
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

class HardwareHarness final {
   public:
    explicit HardwareHarness(const esp_partition_t* partition)
        : partition_(partition),
          previousPages_(kPageCount * kPageSize, 0xffU) {}

    bool prefill(std::uint8_t seed) {
        for (std::size_t index = 0U; index < kKeys.size(); ++index) {
            if (!write(kKeys[index],
                       makeValue(kSizes[index],
                                 static_cast<std::uint8_t>(seed + index)))) {
                return false;
            }
        }
        if (!snapshot(previousPages_)) return false;
        ESP_LOGI(kTag, "PREFILL_DONE keys=%u seed=%u", kKeys.size(), seed);
        logStats("PREFILL_STATS");
        logResources("PREFILL_RESOURCE");
        return true;
    }

    bool rotate(std::size_t maximumWrites) {
        const auto limit = maximumWrites == 0U ? kRotationLimit : maximumWrites;
        ESP_LOGI(kTag, "ROTATE_BEGIN max_writes=%u", limit);
        for (std::size_t rotation = 0U; rotation < limit; ++rotation) {
            const auto slot = rotation % kRotatingKeys.size();
            if (!write(kRotatingKeys[slot],
                       makeValue(kRotatingSizes[slot],
                                 static_cast<std::uint8_t>(rotation + 3U)))) {
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
            pass = pass && hashed && result.value.size() <= kSizes[index];
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

    bool snapshot(std::vector<std::uint8_t>& target) const {
        if (target.size() != kPageCount * kPageSize) return false;
        for (std::size_t page = 0U; page < kPageCount; ++page) {
            const auto result =
                esp_partition_read(partition_, page * kPageSize,
                                   target.data() + page * kPageSize, kPageSize);
            if (result != ESP_OK) {
                ESP_LOGE(kTag, "PARTITION_READ_FAIL page=%u err=%s", page,
                         esp_err_to_name(result));
                return false;
            }
        }
        return true;
    }

    bool detectGcErase() {
        std::vector<std::uint8_t> current(previousPages_.size(), 0xffU);
        if (!snapshot(current)) return false;
        bool erasedPriorPage = false;
        bool otherPageChanged = false;
        for (std::size_t page = 0U; page < kPageCount; ++page) {
            const auto* before = previousPages_.data() + page * kPageSize;
            const auto* after = current.data() + page * kPageSize;
            if (!allErased(before) && allErased(after)) erasedPriorPage = true;
            if (std::memcmp(before, after, kPageSize) != 0)
                otherPageChanged = true;
        }
        previousPages_.swap(current);
        return erasedPriorPage && otherPageChanged;
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
        ESP_LOGI(kTag,
                 "%s free_heap_bytes=%u largest_free_block_bytes=%u "
                 "stack_hwm_bytes=%u",
                 marker, esp_get_free_heap_size(),
                 heap_caps_get_largest_free_block(MALLOC_CAP_8BIT),
                 static_cast<unsigned>(uxTaskGetStackHighWaterMark(nullptr)));
    }

    const esp_partition_t* partition_;
    NvsStateStore store_;
    std::vector<std::uint8_t> previousPages_;
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

std::size_t parseUnsigned(const std::string& line, std::size_t offset,
                          std::size_t fallback) {
    std::size_t result = 0U;
    if (offset >= line.size()) return fallback;
    for (std::size_t index = offset; index < line.size(); ++index) {
        const char byte = line[index];
        if (byte < '0' || byte > '9') return fallback;
        result = result * 10U + static_cast<std::size_t>(byte - '0');
    }
    return result;
}

}  // namespace

bool runIssue90NvsHardwareVerification() {
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

    HardwareHarness harness(partition);
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
        } else if (line.rfind("ROTATE max_writes=", 0U) == 0U) {
            const auto value = parseUnsigned(line, 17U, kRotationLimit);
            pass = harness.rotate(value) && pass;
        } else if (line == "READBACK_ALL") {
            pass = harness.readbackAll() && pass;
        } else if (line.rfind("CUT_ARM token=", 0U) == 0U) {
            pass = harness.cutArmed(line.c_str() + 13U) && pass;
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
