#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "configuration_documents.hpp"
#include "device_ui_contracts.hpp"
#include "run_commands.hpp"
#include "storage_types.hpp"

namespace fermentation {

enum class ApplicationRunIdentityStatus : std::uint8_t {
    Allocated,
    NotInitialized,
    Overflow,
    Unavailable,
};

// A command identity is minted only by ApplicationRunIdentity. Its public
// accessors let the application bind one value to both UI correlation and
// CommandEnvelope::id, while adapters cannot construct or replace it.
class ApplicationCommandIdentity {
   public:
    [[nodiscard]] CommandId commandId() const noexcept { return commandId_; }
    [[nodiscard]] device_platform::UiRequestId uiRequestId() const noexcept {
        return device_platform::UiRequestId{commandId_};
    }

   private:
    friend class ApplicationRunIdentity;
    explicit ApplicationCommandIdentity(CommandId commandId) noexcept
        : commandId_(commandId) {}
    CommandId commandId_{0U};
};

struct ApplicationCommandIdentityResult {
    ApplicationRunIdentityStatus status{
        ApplicationRunIdentityStatus::NotInitialized};
    std::optional<ApplicationCommandIdentity> identity;
};

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
    ~ApplicationRunIdentity() = default;

    [[nodiscard]] static std::optional<ApplicationRunIdentity> create(
        device_platform::StorageEpoch epoch,
        std::optional<CommandId> committedHighWater) noexcept;

    [[nodiscard]] device_platform::StorageEpoch storageEpoch() const noexcept {
        return epoch_;
    }
    [[nodiscard]] std::optional<CommandId> nextCommandId() const noexcept {
        if (nextCommandId_ == 0U) {
            return std::nullopt;
        }
        return nextCommandId_;
    }

   private:
    friend class FermentationApplication;
    friend class ApplicationRunIdentityTestAccess;

    [[nodiscard]] ApplicationCommandIdentityResult
    allocateForApplication() noexcept;
    [[nodiscard]] std::optional<std::string> makeRunId(
        CommandId startCommandId) const;

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
