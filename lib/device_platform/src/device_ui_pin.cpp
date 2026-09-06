#include "device_ui_pin.hpp"

#include <utility>

namespace device_platform {

bool PinEntryModel::apply(const PinEntryAction& action) noexcept {
    switch (action.kind) {
        case PinEntryActionKind::Digit:
            if (action.digit > 9U || complete()) return false;
            candidate_.push_back(static_cast<char>('0' + action.digit));
            owner_.state = candidate_.empty() ? PinEntryState::Empty
                                              : PinEntryState::Incomplete;
            owner_.message.reset();
            return true;
        case PinEntryActionKind::Backspace:
            if (candidate_.empty()) return false;
            candidate_.pop_back();
            owner_.state = candidate_.empty() ? PinEntryState::Empty
                                              : PinEntryState::Incomplete;
            owner_.message.reset();
            return true;
        case PinEntryActionKind::Clear:
            if (candidate_.empty()) return false;
            candidate_.clear();
            owner_.state = PinEntryState::Empty;
            owner_.message.reset();
            return true;
        case PinEntryActionKind::Cancel:
            reset();
            return true;
        case PinEntryActionKind::Commit:
            if (!complete()) return false;
            owner_.state = PinEntryState::Pending;
            owner_.message.reset();
            return true;
    }
    return false;
}

void PinEntryModel::setOwnerState(PinEntryOwnerState state) noexcept {
    owner_ = std::move(state);
}

void PinEntryModel::reset() noexcept {
    candidate_.clear();
    owner_ = {};
}

std::string PinEntryModel::masked() const {
    return std::string(candidate_.size(), '*');
}

}  // namespace device_platform
