#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "configuration_documents.hpp"
#include "device_ui_contracts.hpp"
#include "run_commands.hpp"
#include "storage_types.hpp"

namespace fermentation {

// Application-owned identity allocator shared by every local or remote
// adapter. The committed high-water value is supplied by the canonical run
// persistence head; a logical empty store is represented by zero.
class ApplicationRunIdentity {
   public:
    ApplicationRunIdentity(const ApplicationRunIdentity&) = delete;
    ApplicationRunIdentity& operator=(const ApplicationRunIdentity&) = delete;
    ApplicationRunIdentity(ApplicationRunIdentity&&) noexcept = default;
    ApplicationRunIdentity& operator=(ApplicationRunIdentity&&) noexcept =
        default;

    [[nodiscard]] static std::optional<ApplicationRunIdentity> create(
        device_platform::StorageEpoch epoch,
        std::optional<CommandId> committedHighWater) noexcept;

    [[nodiscard]] std::optional<CommandId> allocateCommandId() noexcept;
    [[nodiscard]] std::optional<device_platform::UiRequestId>
    allocateUiRequestId() noexcept;
    [[nodiscard]] std::optional<std::string> makeRunId(
        CommandId startCommandId) const;

    [[nodiscard]] device_platform::StorageEpoch storageEpoch() const noexcept {
        return epoch_;
    }
    [[nodiscard]] std::optional<CommandId> nextCommandId() const noexcept {
        if (nextCommandId_ == 0U) return std::nullopt;
        return nextCommandId_;
    }

   private:
    ApplicationRunIdentity(device_platform::StorageEpoch epoch,
                           CommandId nextCommandId) noexcept
        : epoch_(epoch), nextCommandId_(nextCommandId) {}

    device_platform::StorageEpoch epoch_;
    // Zero is the internal exhausted sentinel; CommandId zero is never
    // returned to an adapter.
    CommandId nextCommandId_{0U};
};

// The application is the only boundary that gives the neutral run
// provenance its source value. This conversion carries no per-program
// meaning and rejects the reserved invalid catalog revision.
[[nodiscard]] std::optional<RunProgramSourceRevision>
makeRunProgramSourceRevision(ProgramCatalogRevision catalogRevision) noexcept;

}  // namespace fermentation
