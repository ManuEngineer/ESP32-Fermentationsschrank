#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "device_ui_contracts.hpp"

namespace device_platform {

enum class PinEntryActionKind : std::uint8_t {
    Digit,
    Backspace,
    Clear,
    Cancel,
    Commit,
};

struct PinEntryAction {
    PinEntryActionKind kind{PinEntryActionKind::Digit};
    std::uint8_t digit{0U};
};

enum class PinEntryState : std::uint8_t {
    Empty,
    Incomplete,
    Pending,
    RetryWait,
    Rejected,
    Accepted,
};

struct PinEntryOwnerState {
    PinEntryState state{PinEntryState::Empty};
    std::optional<TextKey> message;
};

class PinEntryModel {
   public:
    static constexpr std::size_t kDigitCount = 4U;

    [[nodiscard]] bool apply(const PinEntryAction& action) noexcept;
    void setOwnerState(PinEntryOwnerState state) noexcept;
    void reset() noexcept;

    [[nodiscard]] PinEntryState state() const noexcept { return owner_.state; }
    [[nodiscard]] const std::optional<TextKey>& message() const noexcept {
        return owner_.message;
    }
    [[nodiscard]] bool complete() const noexcept {
        return candidate_.size() == kDigitCount;
    }
    [[nodiscard]] std::string masked() const;
    // The owner may pass this transient candidate to its verifier. The model
    // never persists, logs, exports or makes an authentication decision.
    [[nodiscard]] const std::string& candidate() const noexcept {
        return candidate_;
    }

   private:
    std::string candidate_;
    PinEntryOwnerState owner_;
};

}  // namespace device_platform
