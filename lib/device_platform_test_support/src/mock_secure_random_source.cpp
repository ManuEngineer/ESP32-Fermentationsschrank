#include "mock_secure_random_source.hpp"

#include <cstring>

namespace device_platform_test_support {

namespace {
// Deterministischer splitmix64-Schritt; ausschliesslich fuer native Tests,
// keine kryptografische Eigenschaft noetig.
uint64_t nextSplitMix64(uint64_t& state) {
    state += 0x9E3779B97F4A7C15ULL;
    uint64_t result = state;
    result = (result ^ (result >> 30U)) * 0xBF58476D1CE4E5B9ULL;
    result = (result ^ (result >> 27U)) * 0x94D049BB133111EBULL;
    return result ^ (result >> 31U);
}
}  // namespace

bool MockSecureRandomSource::fill(void* buffer, std::size_t length) {
    // Laenge 0 ist immer ein erfolgreicher No-op, unabhaengig von
    // `shouldFail_` oder einem vorbereiteten Override (analog zu
    // `ByteWriter::writeBytes`, bei dem Laenge 0 ebenfalls vor jeder anderen
    // Pruefung erfolgreich ist) - `buffer` darf `nullptr` sein und wird nicht
    // dereferenziert, kein interner Zustand aendert sich.
    if (length == 0U) {
        return true;
    }
    // Positive Laenge mit `nullptr`: beobachtbar ablehnen, bevor Override
    // oder Generatorzustand angefasst werden.
    if (buffer == nullptr) {
        return false;
    }
    if (shouldFail_) {
        return false;
    }
    if (hasNextBytes_) {
        if (nextBytes_.size() != length) {
            return false;
        }
        std::memcpy(buffer, nextBytes_.data(), length);
        hasNextBytes_ = false;
        nextBytes_.clear();
        return true;
    }
    auto* out = static_cast<uint8_t*>(buffer);
    std::size_t written = 0U;
    while (written < length) {
        const uint64_t word = nextSplitMix64(state_);
        const std::size_t chunk =
            length - written < sizeof(word) ? length - written : sizeof(word);
        std::memcpy(out + written, &word, chunk);
        written += chunk;
    }
    return true;
}

void MockSecureRandomSource::setNextBytes(std::string bytes) {
    nextBytes_ = std::move(bytes);
    hasNextBytes_ = true;
}

void MockSecureRandomSource::injectFailure(bool shouldFail) {
    shouldFail_ = shouldFail;
}

}  // namespace device_platform_test_support
