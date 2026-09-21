#include "http_server_lifecycle.hpp"

#include <cctype>

namespace device_platform {
namespace {

constexpr std::size_t kMaximumRequestMetadataBytes = 2048U;
constexpr std::size_t kMaximumResponseSetCookieBytes = 512U;
constexpr std::size_t kMaximumResponseRetryAfterBytes = 8U;

bool validValue(const std::string& value, std::size_t maximum,
                bool asciiOnly = false) noexcept {
    if (value.empty() || value.size() > maximum) return false;
    for (const unsigned char byte : value) {
        if (byte == 0U || byte == '\r' || byte == '\n' || byte < 0x20U ||
            byte == 0x7FU || (asciiOnly && byte >= 0x80U)) {
            return false;
        }
    }
    return true;
}

template <typename T>
void account(const std::optional<T>& value, std::size_t& count,
             std::size_t& bytes) noexcept {
    if (value.has_value()) {
        ++count;
        bytes += value->size();
    }
}

}  // namespace

HttpMetadataValidation validateHttpRequestMetadata(
    const HttpRequestMetadata& metadata) noexcept {
    std::size_t count = 0U;
    std::size_t bytes = 0U;
    account(metadata.host, count, bytes);
    account(metadata.contentType, count, bytes);
    account(metadata.cookie, count, bytes);
    account(metadata.csrfToken, count, bytes);
    account(metadata.mutationSeq, count, bytes);
    account(metadata.origin, count, bytes);
    account(metadata.referer, count, bytes);
    account(metadata.secFetchSite, count, bytes);
    if (count > 8U || bytes > kMaximumRequestMetadataBytes) {
        return HttpMetadataValidation::Invalid;
    }
    if ((metadata.host.has_value() &&
         !validValue(*metadata.host, 256U)) ||
        (metadata.contentType.has_value() &&
         !validValue(*metadata.contentType, 64U)) ||
        (metadata.cookie.has_value() &&
         !validValue(*metadata.cookie, 512U)) ||
        (metadata.csrfToken.has_value() &&
         !validValue(*metadata.csrfToken, 64U, true)) ||
        (metadata.mutationSeq.has_value() &&
         !validValue(*metadata.mutationSeq, 20U, true)) ||
        (metadata.origin.has_value() &&
         !validValue(*metadata.origin, 256U)) ||
        (metadata.referer.has_value() &&
         !validValue(*metadata.referer, 512U)) ||
        (metadata.secFetchSite.has_value() &&
         !validValue(*metadata.secFetchSite, 32U, true))) {
        return HttpMetadataValidation::Invalid;
    }
    return HttpMetadataValidation::Valid;
}

bool validateHttpResponseMetadata(const HttpResponseMetadata& metadata) noexcept {
    if (metadata.setCookie.has_value() &&
        !validValue(*metadata.setCookie, kMaximumResponseSetCookieBytes)) {
        return false;
    }
    if (metadata.retryAfter.has_value() &&
        !validValue(*metadata.retryAfter, kMaximumResponseRetryAfterBytes,
                    true)) {
        return false;
    }
    if (metadata.retryAfter.has_value()) {
        for (const unsigned char byte : *metadata.retryAfter) {
            if (!std::isdigit(byte)) return false;
        }
    }
    return true;
}

}  // namespace device_platform
