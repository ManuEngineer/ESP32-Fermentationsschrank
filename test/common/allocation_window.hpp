#pragma once

// Allocation accounting window shared by native tests (PR 206, plan 3376a19).
// Include in exactly ONE translation unit of a test executable: it replaces
// the global allocation functions, which must not be defined twice.
//
// Global operator new/delete and cJSON (malloc/free through cJSON_InitHooks,
// installed only inside measure()) feed one set of counters, so a cJSON tree
// and std::string copies are measured together. Every block carries a 16 byte
// header with its size and a flag saying whether it was allocated inside the
// window; blocks allocated before the window are not counted when freed.
// Limits: host build (64-bit, cJSON node 48 B instead of 40 B on Xtensa),
// requested bytes without allocator block headers, no fragmentation, no
// concurrency; this is not an ESP32 heap measurement.

#include <cstddef>
#include <cstdlib>
#include <new>

#include "cJSON.h"

namespace alloc_window {

struct Window {
    bool active{false};
    std::size_t count{0U};
    std::size_t requestedBytes{0U};
    std::size_t largestBlock{0U};
    std::size_t liveBytes{0U};
    std::size_t peakLiveBytes{0U};
    std::size_t cJsonCount{0U};
    std::size_t cJsonLiveBytes{0U};
};

inline Window gWindow;
inline constexpr std::size_t kHeader = 16U;
struct Header {
    std::size_t size;
    bool tracked;
};

inline void* allocate(std::size_t size, bool fromCJson) {
    auto* raw = static_cast<char*>(std::malloc(size + kHeader));
    if (raw == nullptr) return nullptr;
    auto* header = reinterpret_cast<Header*>(raw);
    header->size = size;
    header->tracked = gWindow.active;
    if (gWindow.active) {
        ++gWindow.count;
        gWindow.requestedBytes += size;
        if (size > gWindow.largestBlock) gWindow.largestBlock = size;
        gWindow.liveBytes += size;
        if (gWindow.liveBytes > gWindow.peakLiveBytes) {
            gWindow.peakLiveBytes = gWindow.liveBytes;
        }
        if (fromCJson) {
            ++gWindow.cJsonCount;
            gWindow.cJsonLiveBytes += size;
        }
    }
    return raw + kHeader;
}

inline void release(void* pointer, bool fromCJson) {
    if (pointer == nullptr) return;
    auto* raw = static_cast<char*>(pointer) - kHeader;
    const auto* header = reinterpret_cast<const Header*>(raw);
    if (header->tracked) {
        gWindow.liveBytes -= header->size;
        if (fromCJson) gWindow.cJsonLiveBytes -= header->size;
    }
    std::free(raw);
}

inline void* cJsonMalloc(std::size_t size) { return allocate(size, true); }
inline void cJsonFree(void* pointer) { release(pointer, true); }

// Runs `action` inside a fresh window with both allocation paths counted and
// returns the counters (cJsonLiveBytes != 0 means cJSON memory leaked out of
// the window).
template <typename Action>
Window measure(Action&& action) {
    cJSON_Hooks hooks{cJsonMalloc, cJsonFree};
    cJSON_InitHooks(&hooks);
    gWindow = Window{};
    gWindow.active = true;
    action();
    gWindow.active = false;
    cJSON_InitHooks(nullptr);
    return gWindow;
}

}  // namespace alloc_window

void* operator new(std::size_t size) {
    void* pointer = alloc_window::allocate(size == 0U ? 1U : size, false);
    if (pointer == nullptr) throw std::bad_alloc();
    return pointer;
}
void* operator new[](std::size_t size) { return operator new(size); }
void operator delete(void* pointer) noexcept {
    alloc_window::release(pointer, false);
}
void operator delete[](void* pointer) noexcept {
    alloc_window::release(pointer, false);
}
void operator delete(void* pointer, std::size_t) noexcept {
    alloc_window::release(pointer, false);
}
void operator delete[](void* pointer, std::size_t) noexcept {
    alloc_window::release(pointer, false);
}
