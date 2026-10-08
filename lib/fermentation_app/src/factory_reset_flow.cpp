#include "factory_reset_flow.hpp"

namespace fermentation {

bool FactoryResetFlow::begin(FactoryResetKind kind) noexcept {
    if (!configured()) return false;
    if (stage_ != FactoryResetStage::Idle) return false;
    kind_ = kind;
    outcome_ = FactoryResetOutcome::None;
    holdStartedMs_.reset();
    stage_ = kind == FactoryResetKind::PinProtected
                 ? FactoryResetStage::PinRequired
                 : FactoryResetStage::Warning;
    return true;
}

bool FactoryResetFlow::pinVerified() noexcept {
    if (stage_ != FactoryResetStage::PinRequired ||
        kind_ != FactoryResetKind::PinProtected) {
        return false;
    }
    stage_ = FactoryResetStage::Warning;
    return true;
}

bool FactoryResetFlow::acknowledge() noexcept {
    switch (stage_) {
        case FactoryResetStage::Warning:
            stage_ = FactoryResetStage::Confirm;
            return true;
        case FactoryResetStage::Confirm:
            stage_ = FactoryResetStage::Hold;
            holdStartedMs_.reset();
            return true;
        case FactoryResetStage::Idle:
        case FactoryResetStage::PinRequired:
        case FactoryResetStage::Hold:
        case FactoryResetStage::Executing:
        case FactoryResetStage::Finished:
            return false;
    }
    return false;
}

void FactoryResetFlow::cancel() noexcept {
    if (stage_ == FactoryResetStage::Executing ||
        stage_ == FactoryResetStage::Idle) {
        return;
    }
    stage_ = FactoryResetStage::Idle;
    outcome_ = FactoryResetOutcome::None;
    holdStartedMs_.reset();
}

bool FactoryResetFlow::updateHold(bool held, std::uint64_t nowMs) noexcept {
    if (stage_ != FactoryResetStage::Hold || !holdMillis_.has_value()) {
        return false;
    }
    if (!held) {
        holdStartedMs_.reset();
        return false;
    }
    if (!holdStartedMs_.has_value() || nowMs < *holdStartedMs_) {
        holdStartedMs_ = nowMs;
        return false;
    }
    if (nowMs - *holdStartedMs_ < *holdMillis_) {
        return false;
    }
    holdStartedMs_.reset();
    stage_ = FactoryResetStage::Executing;
    return true;
}

void FactoryResetFlow::finish(FactoryResetOutcome outcome) noexcept {
    if (stage_ != FactoryResetStage::Executing) return;
    outcome_ = outcome;
    stage_ = FactoryResetStage::Finished;
}

void FactoryResetFlow::dismiss() noexcept {
    if (stage_ != FactoryResetStage::Finished) return;
    stage_ = FactoryResetStage::Idle;
    outcome_ = FactoryResetOutcome::None;
}

std::uint32_t FactoryResetFlow::heldMillis(std::uint64_t nowMs) const noexcept {
    if (stage_ != FactoryResetStage::Hold || !holdStartedMs_.has_value() ||
        nowMs < *holdStartedMs_) {
        return 0U;
    }
    const auto held = nowMs - *holdStartedMs_;
    return held > 0xFFFFFFFFULL ? 0xFFFFFFFFU
                                : static_cast<std::uint32_t>(held);
}

}  // namespace fermentation
