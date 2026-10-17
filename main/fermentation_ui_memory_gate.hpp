#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>

namespace fermentation::main_ui {

// Composition-only policy. Each call obtains a new allocation-free measurement;
// passing it does not reserve memory or guarantee subsequent allocations.
class UiMemoryGate {
   public:
    UiMemoryGate(std::size_t minimum, std::size_t resumeMargin)
        : minimum_(minimum), resumeAt_(minimum + resumeMargin) {}

    template <typename LargestFreeBlock>
    [[nodiscard]] bool allowStep(LargestFreeBlock&& measure,
                                 std::uint64_t now) {
        const auto block = measure();
        if (block < (paused_ ? resumeAt_ : minimum_)) {
            paused_ = true;
            ++skippedSteps_;
            logSkip_ = !lastLog_.has_value() || now - *lastLog_ >= 10000U;
            if (logSkip_) lastLog_ = now;
            return false;
        }
        paused_ = false;
        logSkip_ = false;
        return true;
    }

    [[nodiscard]] bool logSkip() const noexcept { return logSkip_; }
    [[nodiscard]] std::uint64_t skippedSteps() const noexcept {
        return skippedSteps_;
    }

   private:
    std::size_t minimum_;
    std::size_t resumeAt_;
    bool paused_{false};
    bool logSkip_{false};
    std::uint64_t skippedSteps_{0U};
    std::optional<std::uint64_t> lastLog_;
};

// Only inhibits the existing factory-reset hold path; the flow remains the
// sole owner of its stages, timer and reset. Milliseconds share the monotonic
// time source with the renderer's single raw-sampling callback.
class UiResetHoldContactGate {
   public:
    void skipped(std::uint64_t now) noexcept {
        state_ = State::AwaitRelease;
        after_ = now;
    }

    [[nodiscard]] bool observe(
        bool physicalRelease, bool validContact,
        std::optional<std::uint64_t> sampledAt) noexcept {
        if (state_ == State::Allowed) return true;
        // A cached release, no sample, controller error or uncalibrated
        // contact cannot unlock a hold interrupted by a memory skip.
        if (!sampledAt.has_value() || *sampledAt <= after_) return false;
        if (state_ == State::AwaitRelease && physicalRelease) {
            state_ = State::AwaitContact;
            after_ = *sampledAt;
        } else if (state_ == State::AwaitContact && validContact) {
            state_ = State::Allowed;
        }
        return state_ == State::Allowed;
    }

   private:
    enum class State : std::uint8_t { Allowed, AwaitRelease, AwaitContact };
    State state_{State::Allowed};
    std::uint64_t after_{0U};
};

}  // namespace fermentation::main_ui
