#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

#include "fermentation_ui_commands.hpp"
#include "fermentation_ui_editing.hpp"
#include "network_mode.hpp"
#include "web_session.hpp"

namespace fermentation {

enum class WebApiCodecStatus : std::uint8_t {
    Success,
    InvalidJson,
    MissingField,
    WrongType,
    CapacityExceeded,
};

// HTTP remains an adapter boundary: this DTO contains only bounded user
// values, expected revisions, and the explicit UI confirmation bit. It does
// not carry runtime evidence, a command identity, or a persistence result.
struct WebUiRunCommand {
    FermentationUiExpectedRevisions expected;
    bool confirmed{false};
    FermentationUiEnvelopePayload payload{FermentationUiResetFaultIntent{}};
};

struct WebProgramEditCommand {
    ProgramCatalogRevision expectedProgramCatalogRevision;
    FermentationUiProgramEditRequest request;
};

struct WebConfigurationCommitCommand {
    FermentationUiConfigurationCommitCommand command;
};

[[nodiscard]] WebApiCodecStatus encodeUiSnapshot(
    const FermentationUiSnapshot& snapshot, std::string& out);
[[nodiscard]] WebApiCodecStatus encodeUiSnapshot(
    const FermentationUiSnapshot& snapshot,
    const MutationSequenceView& sequence, std::string& out);
[[nodiscard]] WebApiCodecStatus encodeUiSnapshot(
    const FermentationUiSnapshot& snapshot,
    const MutationSequenceView& sequence,
    std::optional<bool> webPasswordEnabled, std::string& out);
[[nodiscard]] WebApiCodecStatus encodeUiSnapshot(
    const FermentationUiSnapshot& snapshot,
    const MutationSequenceView& sequence,
    std::optional<bool> webPasswordEnabled,
    std::optional<std::string> csrfToken, std::string& out);
[[nodiscard]] WebApiCodecStatus encodeStatus(
    const FermentationUiSnapshot& snapshot, std::string& out);
[[nodiscard]] WebApiCodecStatus encodeStatus(
    const FermentationUiSnapshot& snapshot,
    std::optional<bool> webPasswordEnabled, std::string& out);
[[nodiscard]] WebApiCodecStatus encodeTemperatures(
    const FermentationUiSnapshot& snapshot, std::string& out);
[[nodiscard]] WebApiCodecStatus encodeAlerts(
    const FermentationUiSnapshot& snapshot, std::string& out);
[[nodiscard]] WebApiCodecStatus encodeProgramList(
    const std::vector<FermentationUiProgramListEntry>& programs,
    std::string& out);
[[nodiscard]] WebApiCodecStatus encodeProgramPreview(
    const ConfigurationPreviewView& preview, std::string& out);
[[nodiscard]] WebApiCodecStatus encodeSessionHandoff(
    const std::string& csrfToken, const MutationSequenceView& sequence,
    std::string& out);
[[nodiscard]] WebApiCodecStatus encodeError(const char* code,
                                            const char* message,
                                            std::string& out);

[[nodiscard]] WebApiCodecStatus decodeCredentialField(const std::string& body,
                                                      const char* field,
                                                      std::size_t maximumBytes,
                                                      std::string& out);
[[nodiscard]] WebApiCodecStatus decodeBooleanField(const std::string& body,
                                                   const char* field,
                                                   bool& out);
[[nodiscard]] WebApiCodecStatus decodeNetworkMode(
    const std::string& body, device_platform::NetworkMode& out,
    std::optional<UserConfigurationRevision>& expectedRevision);
[[nodiscard]] WebApiCodecStatus decodeWebUiRunCommand(const std::string& body,
                                                      WebUiRunCommand& out);
[[nodiscard]] WebApiCodecStatus decodeWebProgramEditCommand(
    const std::string& body, WebProgramEditCommand& out);
[[nodiscard]] WebApiCodecStatus decodeWebConfigurationCommitCommand(
    const std::string& body, WebConfigurationCommitCommand& out);

}  // namespace fermentation
