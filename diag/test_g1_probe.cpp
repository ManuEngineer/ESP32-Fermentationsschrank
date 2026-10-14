#include <cassert>
#include <cstdlib>
#include <cstring>
#include <new>
#include "g1_ui_probe.hpp"
static unsigned allocations{}, logCalls{}, errors{};
static std::size_t freeBytes = 10000U, blockBytes = 8000U;
static std::int64_t now = 1000;
void* operator new(std::size_t size) {
    ++allocations;
    if (auto* p = std::malloc(size)) return p;
    throw std::bad_alloc();
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
std::size_t heap_caps_get_free_size(unsigned caps) {
    assert(caps == 3U);
    return freeBytes;
}
std::size_t heap_caps_get_largest_free_block(unsigned caps) {
    assert(caps == 3U);
    return blockBytes;
}
std::size_t heap_caps_get_minimum_free_size(unsigned caps) {
    assert(caps == 3U);
    return 1000U;
}
std::int64_t esp_timer_get_time() { return ++now; }
void g1_host_log(const char* tag, const char*, ...) {
    assert(g1_ui::active == nullptr);
    ++logCalls;
    if (std::strcmp(tag, "g1_probe_error") == 0) ++errors;
}
int main() {
    const auto before = allocations;
    g1_ui::Probe::gateBlock(12345U);
    assert(g1_ui::measuredGateBlock == 12345U && g1_ui::gateMeasuredAt > 0U);
    {
        g1_ui::Probe step(g1_ui::Step, 0U, 1U);
        freeBytes = 9000U;
        {
            g1_ui::Probe touch(g1_ui::Touch, 0U, 1U);
            freeBytes = 4500U;
            blockBytes = 3000U;
            g1_ui::Probe::checkpoint();
            freeBytes = 8800U;
            blockBytes = 6000U;
        }
        assert(logCalls == 0U && g1_ui::pendingCount == 1U);
        assert(g1_ui::pending[0].worst.pre == 9000U);
        assert(g1_ui::pending[0].worst.post == 8800U);
        assert(g1_ui::pending[0].worst.low == 4500U);
        freeBytes = 10000U;
        blockBytes = 8000U;
    }
    assert(allocations == before && logCalls == 0U);
    assert(g1_ui::pendingCount == 2U);
    assert(g1_ui::demand(g1_ui::pending[1].worst) == 5500U);
    assert(g1_ui::pending[1].blockLow == 3000U);
    g1_ui::flush(2000000U);
    assert(logCalls == 3U && g1_ui::pendingCount == 0U);
    assert(allocations == before);
    for (unsigned p = 0U; p < 9U; ++p) {
        g1_ui::Record r{};
        r.page = p;
        g1_ui::queue(r);
    }
    assert(g1_ui::dropped == 1U);
    g1_ui::flush(4000000U);
    assert(errors == 1U && allocations == before);
    return 0;
}
