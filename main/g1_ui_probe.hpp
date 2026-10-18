#pragma once

// TEMPORARY PR203 G1 overlay; main-task only, never product or a platform port.
#include <cstddef>
#include <cstdint>
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"

namespace g1_ui {
enum Phase : unsigned { Step, Touch, Render };
struct Record {
    unsigned phase{}, page{}, locale{}, home{}, hold{}, kind{};
    unsigned fresh{}, skipped{}, held{}, released{}, allowed{};
    unsigned commands{}, textBytes{}, maxText{};
    std::uint64_t start{}, end{};
    std::size_t pre{}, post{}, low{}, blockPre{}, blockPost{}, blockLow{};
    std::size_t bootPre{}, bootPost{};
};
struct Aggregate {
    Record worst{};
    unsigned count{};
    std::size_t low{}, blockLow{};
    unsigned maxTextBytes{}, maxText{};
};
inline Aggregate pending[8];
inline unsigned dropped{}, pendingCount{};
inline std::uint64_t lastFlush{}, skips{}, resumes{}, pageSkips[32];
inline bool paused{};
inline unsigned held{}, released{}, allowed{};
inline std::size_t measuredGateBlock{};
inline std::uint64_t gateMeasuredAt{};
inline std::uint64_t sampleAge{}, lastObservationUs{};
inline unsigned localeCode(const char* value, std::size_t size) {
    if (size == 2U && value[0] == 'd' && value[1] == 'e') return 1U;
    if (size == 2U && value[0] == 'e' && value[1] == 'n') return 2U;
    if (size == 2U && value[0] == 'e' && value[1] == 's') return 3U;
    return 0U;
}
inline std::size_t demand(const Record& r) {
    return r.pre > r.low ? r.pre - r.low : 0U;
}
inline bool same(const Record& a, const Record& b) {
    return a.phase == b.phase && a.page == b.page && a.locale == b.locale &&
           a.home == b.home && a.hold == b.hold && a.kind == b.kind &&
           a.fresh == b.fresh && a.skipped == b.skipped && a.held == b.held &&
           a.released == b.released && a.allowed == b.allowed;
}
inline void queue(const Record& r) {
    unsigned index = 0U;
    while (index < pendingCount && !same(pending[index].worst, r)) ++index;
    if (index == 8U) {
        ++dropped;
        return;
    }
    auto& a = pending[index];
    if (index == pendingCount) {
        ++pendingCount;
        a = {};
        a.worst = r;
        a.low = r.low;
        a.blockLow = r.blockLow;
    }
    ++a.count;
    if (demand(r) > demand(a.worst)) a.worst = r;
    if (r.low < a.low) a.low = r.low;
    if (r.blockLow < a.blockLow) a.blockLow = r.blockLow;
    if (r.textBytes > a.maxTextBytes) a.maxTextBytes = r.textBytes;
    if (r.maxText > a.maxText) a.maxText = r.maxText;
}
// No logging until the outermost measured scope and its owned models end.
// Flush at most once/s, bounded eight aggregates; overflow remains explicit.
inline void flush(std::uint64_t now) {
    if (now - lastFlush < 1000000U) return;
    lastFlush = now;
    for (unsigned i = 0; i < pendingCount; ++i) {
        const auto& a = pending[i];
        const auto& r = a.worst;
        ESP_LOGI(
            "g1_ui",
            "phase=%u page=%u locale=%u home=%u hold=%u kind=%u fresh=%u "
            "skip=%u held=%u release=%u allowed=%u n=%u t0=%llu t1=%llu pre=%u "
            "post=%u low=%u block_pre=%u block_post=%u block_low=%u "
            "boot_pre=%u boot_post=%u demand=%u commands=%u text_bytes=%u "
            "max_text=%u any_low=%u any_block_low=%u max_text_bytes=%u "
            "max_text_seen=%u",
            r.phase, r.page, r.locale, r.home, r.hold, r.kind, r.fresh,
            r.skipped, r.held, r.released, r.allowed, a.count,
            static_cast<unsigned long long>(r.start),
            static_cast<unsigned long long>(r.end),
            static_cast<unsigned>(r.pre), static_cast<unsigned>(r.post),
            static_cast<unsigned>(r.low), static_cast<unsigned>(r.blockPre),
            static_cast<unsigned>(r.blockPost),
            static_cast<unsigned>(r.blockLow), static_cast<unsigned>(r.bootPre),
            static_cast<unsigned>(r.bootPost), static_cast<unsigned>(demand(r)),
            r.commands, r.textBytes, r.maxText, static_cast<unsigned>(a.low),
            static_cast<unsigned>(a.blockLow), a.maxTextBytes, a.maxText);
    }
    pendingCount = 0U;
    if (dropped)
        ESP_LOGE("g1_probe_error", "aggregate_overflow dropped=%u", dropped);
    // Gate observations are diagnostics, never a release/contact substitute.
    ESP_LOGI("g1_gate",
             "skips=%llu resumes=%llu paused=%u held=%u release=%u allowed=%u "
             "sample_age_ms=%llu dropped=%u gate_block=%u gate_at_us=%llu",
             static_cast<unsigned long long>(skips),
             static_cast<unsigned long long>(resumes), paused ? 1U : 0U, held,
             released, allowed,
             static_cast<unsigned long long>(
                 sampleAge == UINT64_MAX || !lastObservationUs
                     ? UINT64_MAX
                     : sampleAge + (now - lastObservationUs) / 1000U),
             dropped, static_cast<unsigned>(measuredGateBlock),
             static_cast<unsigned long long>(gateMeasuredAt));
    for (unsigned p = 0; p < 32; ++p)
        if (pageSkips[p])
            ESP_LOGI("g1_skip", "page=%u total=%llu", p,
                     static_cast<unsigned long long>(pageSkips[p]));
}
class Probe;
inline Probe* active{};
class Probe {
   public:
    Probe(unsigned phase, unsigned page, unsigned locale) : parent_(active) {
        r_.phase = phase;
        r_.page = page;
        r_.locale = locale;
        if (parent_) {
            r_.home = parent_->r_.home;
            r_.hold = parent_->r_.hold;
            r_.kind = parent_->r_.kind;
            r_.fresh = parent_->r_.fresh;
            parent_->interesting_ = true;
        }
        interesting_ = phase != Step;
        r_.held = held;
        r_.released = released;
        r_.allowed = allowed;
        r_.start = static_cast<std::uint64_t>(esp_timer_get_time());
        r_.pre = heap_caps_get_free_size(caps);
        r_.blockPre = heap_caps_get_largest_free_block(caps);
        r_.bootPre = heap_caps_get_minimum_free_size(caps);
        r_.low = r_.pre;
        r_.blockLow = r_.blockPre;
        active = this;
    }
    ~Probe() {
        checkpoint();
        r_.post = heap_caps_get_free_size(caps);
        r_.blockPost = heap_caps_get_largest_free_block(caps);
        for (auto* p = active; p; p = p->parent_) {
            if (r_.post < p->r_.low) p->r_.low = r_.post;
            if (r_.blockPost < p->r_.blockLow) p->r_.blockLow = r_.blockPost;
        }
        r_.bootPost = heap_caps_get_minimum_free_size(caps);
        r_.end = static_cast<std::uint64_t>(esp_timer_get_time());
        active = parent_;
        if (interesting_) queue(r_);
        if (!parent_) flush(r_.end);
    }
    Probe(const Probe&) = delete;
    Probe& operator=(const Probe&) = delete;
    void context(unsigned home, unsigned hold, unsigned kind, bool fresh) {
        r_.home = home;
        r_.hold = hold;
        r_.kind = kind;
        r_.fresh = fresh ? 1U : 0U;
    }
    void skip(std::uint64_t count) {
        r_.skipped = 1U;
        interesting_ = true;
        paused = true;
        skips = count;
        if (r_.page < 32U) ++pageSkips[r_.page];
    }
    static void gateBlock(std::size_t block) {
        measuredGateBlock = block;
        gateMeasuredAt = static_cast<std::uint64_t>(esp_timer_get_time());
    }
    static void resumed() {
        if (paused) ++resumes;
        paused = false;
    }
    static void touch(bool heldNow, bool releaseNow, bool allowedNow,
                      std::uint64_t age) {
        held = heldNow ? 1U : 0U;
        released = releaseNow ? 1U : 0U;
        allowed = allowedNow ? 1U : 0U;
        sampleAge = age;
        lastObservationUs = static_cast<std::uint64_t>(esp_timer_get_time());
        for (auto* p = active; p; p = p->parent_) {
            p->r_.held = held;
            p->r_.released = released;
            p->r_.allowed = allowed;
        }
    }
    static void model(unsigned commands, unsigned bytes, unsigned maxText) {
        for (auto* p = active; p; p = p->parent_) {
            p->r_.commands = commands;
            p->r_.textBytes = bytes;
            p->r_.maxText = maxText;
        }
        checkpoint();
    }
    static void checkpoint() {
        if (!active) return;
        const auto free = heap_caps_get_free_size(caps);
        const auto block = heap_caps_get_largest_free_block(caps);
        for (auto* p = active; p; p = p->parent_) {
            if (free < p->r_.low) p->r_.low = free;
            if (block < p->r_.blockLow) p->r_.blockLow = block;
        }
    }

   private:
    static constexpr unsigned caps = MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT;
    Probe* parent_;
    Record r_{};
    bool interesting_{};
};
}  // namespace g1_ui
