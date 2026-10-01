#include <array>
#include <cstdlib>
#include <cstdint>
#include <iostream>
#include <string_view>

#include "esp_idf_replay_digest.hpp"

extern "C" void* esp_mbedtls_mem_calloc(std::size_t count, std::size_t size) {
    return std::calloc(count, size);
}

extern "C" void esp_mbedtls_mem_free(void* pointer) { std::free(pointer); }

extern "C" void app_main() {}

namespace {

bool equal(const device_platform::ReplayDigest& actual,
           std::string_view expectedHex) {
    constexpr char hex[] = "0123456789abcdef";
    if (expectedHex.size() != actual.size() * 2U) return false;
    for (std::size_t index = 0U; index < actual.size(); ++index) {
        if (expectedHex[index * 2U] != hex[actual[index] >> 4U] ||
            expectedHex[index * 2U + 1U] != hex[actual[index] & 0x0FU])
            return false;
    }
    return true;
}

}  // namespace

int main() {
    device_platform_esp_idf::EspIdfSha256ReplayDigest digest;
    device_platform::ReplayDigest actual{};
    const device_platform::ReplayDigestInput input{
        "POST", "/internal/ui/run", "0", {}};
    if (!digest.digest(input, actual)) {
        std::cerr << "REPLAY_DIGEST_ADAPTER=FAIL\n";
        return 1;
    }

    // SHA-256("POST\\0/internal/ui/run\\00\\0").
    constexpr std::string_view expected{
        "c6c04aa05325910ec223a89c167b2f3fcab7ab77fe18a2669f1dc292bc23432e"};
    if (!equal(actual, expected)) {
        std::cerr << "REPLAY_DIGEST_KNOWN_VECTOR=FAIL\n";
        return 1;
    }
    std::cout << "REPLAY_DIGEST_KNOWN_VECTOR=PASS\n";
    return 0;
}
