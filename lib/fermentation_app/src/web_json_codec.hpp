#pragma once

#include <cstddef>
#include <string>

#include "fermentation_ui_models.hpp"
#include "web_application_routes.hpp"

namespace fermentation {

inline constexpr std::size_t kMaximumWebLoginBodyBytes = 768U;
inline constexpr std::size_t kMaximumWebProvisionBodyBytes = 1024U;
inline constexpr std::size_t kMaximumWebApiResponseBodyBytes = 3072U;
inline constexpr std::size_t kMaximumWebApiAlertCount = 16U;
inline constexpr unsigned kMaximumWebJsonNesting = 4U;

enum class WebRunMutationDecodeStatus : unsigned char {
    Success,
    TooLarge,
    Invalid,
};

struct WebLoginDto {
    std::string password;
};

enum class WebLoginDecodeStatus : unsigned char {
    Success,
    TooLarge,
    Invalid,
};

// First-time web access setup request. The codec only checks the schema;
// password and Service-PIN rules stay with the authentication domain, so
// present-but-invalid values reach the Application and end as 422.
struct WebProvisionDto {
    WebProvisionMode mode{WebProvisionMode::Protect};
    std::string password;
    std::string servicePin;
};

enum class WebProvisionDecodeStatus : unsigned char {
    Success,
    TooLarge,
    Invalid,
};

[[nodiscard]] WebProvisionDecodeStatus decodeWebProvision(
    const std::string& exactBody, WebProvisionDto& output);

[[nodiscard]] WebLoginDecodeStatus decodeWebLogin(const std::string& exactBody,
                                                  WebLoginDto& output);

// cJSON is confined to this codec implementation. The decoder accepts one
// versioned schema and publishes no partial DTO on failure.
[[nodiscard]] WebRunMutationDecodeStatus decodeWebRunMutation(
    const std::string& exactBody, WebRunMutationDto& output);

[[nodiscard]] bool encodeWebApiStatus(const FermentationUiSnapshot& snapshot,
                                      std::string& output);
[[nodiscard]] bool encodeWebApiTemperatures(
    const FermentationUiSnapshot& snapshot, std::string& output);
[[nodiscard]] bool encodeWebApiAlerts(const FermentationUiSnapshot& snapshot,
                                      std::string& output);

}  // namespace fermentation
