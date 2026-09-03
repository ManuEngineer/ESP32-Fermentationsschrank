#include "application_run_identity.hpp"

#include <limits>

namespace fermentation {

std::optional<RunProgramSourceRevision> makeRunProgramSourceRevision(
    ProgramCatalogRevision catalogRevision) noexcept {
    if (catalogRevision.value() == 0U) return std::nullopt;
    return RunProgramSourceRevision{catalogRevision.value()};
}

std::optional<ApplicationRunIdentity> ApplicationRunIdentity::create(
    device_platform::StorageEpoch epoch,
    std::optional<CommandId> committedHighWater) noexcept {
    if (epoch.value() == 0U || !committedHighWater.has_value()) {
        return std::nullopt;
    }
    const auto highWater = *committedHighWater;
    if (highWater == std::numeric_limits<CommandId>::max()) {
        return ApplicationRunIdentity(epoch, 0U);
    }
    return ApplicationRunIdentity(epoch, highWater + 1U);
}

std::optional<CommandId> ApplicationRunIdentity::allocateCommandId() noexcept {
    if (nextCommandId_ == 0U) return std::nullopt;
    const auto allocated = nextCommandId_;
    nextCommandId_ = allocated == std::numeric_limits<CommandId>::max()
                         ? 0U
                         : allocated + 1U;
    return allocated;
}

std::optional<device_platform::UiRequestId>
ApplicationRunIdentity::allocateUiRequestId() noexcept {
    const auto id = allocateCommandId();
    if (!id.has_value()) return std::nullopt;
    return device_platform::UiRequestId{*id};
}

std::optional<std::string> ApplicationRunIdentity::makeRunId(
    CommandId startCommandId) const {
    if (epoch_.value() == 0U || startCommandId == 0U) return std::nullopt;
    const auto value = std::string{"e"} + std::to_string(epoch_.value()) +
                       "-c" + std::to_string(startCommandId);
    if (value.size() < 1U || value.size() > 48U) return std::nullopt;
    return value;
}

}  // namespace fermentation
