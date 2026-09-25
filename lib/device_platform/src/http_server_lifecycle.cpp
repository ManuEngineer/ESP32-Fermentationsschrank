#include "http_server_lifecycle.hpp"

#include <array>
#include <cctype>
#include <string_view>

namespace device_platform {
namespace {

constexpr std::size_t kMaximumSetCookieBytes = 512U;
constexpr std::size_t kMaximumRetryAfterBytes = 8U;
constexpr std::array<std::string_view, 8U> kBrowserMetadataHeaders{
    "Host",         "Content-Type",      "Cookie",
    "X-CSRF-Token", "X-UI-Mutation-Seq", "Origin",
    "Referer",      "Sec-Fetch-Site"};

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

bool isHeaderNameCharacter(unsigned char byte) noexcept {
    return (byte >= '0' && byte <= '9') || (byte >= 'A' && byte <= 'Z') ||
           (byte >= 'a' && byte <= 'z') ||
           std::string_view("!#$%&'*+-.^_`|~").find(static_cast<char>(byte)) !=
               std::string_view::npos;
}

bool asciiEqualIgnoreCase(const std::string& raw, std::size_t start,
                          std::size_t length,
                          std::string_view expected) noexcept {
    if (length != expected.size()) return false;
    for (std::size_t offset = 0U; offset < length; ++offset) {
        const auto actual = static_cast<unsigned char>(raw[start + offset]);
        const auto wanted = static_cast<unsigned char>(expected[offset]);
        if (std::tolower(actual) != std::tolower(wanted)) return false;
    }
    return true;
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
    if (count > 8U || bytes > kMaximumHttpRequestMetadataBytes) {
        return HttpMetadataValidation::Invalid;
    }
    if ((metadata.host.has_value() &&
         !validValue(*metadata.host, kMaximumHttpHostHeaderBytes)) ||
        (metadata.contentType.has_value() &&
         !validValue(*metadata.contentType,
                     kMaximumHttpContentTypeHeaderBytes)) ||
        (metadata.cookie.has_value() &&
         !validValue(*metadata.cookie, kMaximumHttpCookieHeaderBytes)) ||
        (metadata.csrfToken.has_value() &&
         !validValue(*metadata.csrfToken, kMaximumHttpCsrfHeaderBytes, true)) ||
        (metadata.mutationSeq.has_value() &&
         !validValue(*metadata.mutationSeq,
                     kMaximumHttpMutationSequenceHeaderBytes, true)) ||
        (metadata.origin.has_value() &&
         !validValue(*metadata.origin, kMaximumHttpOriginHeaderBytes)) ||
        (metadata.referer.has_value() &&
         !validValue(*metadata.referer, kMaximumHttpRefererHeaderBytes)) ||
        (metadata.secFetchSite.has_value() &&
         !validValue(*metadata.secFetchSite,
                     kMaximumHttpSecFetchSiteHeaderBytes, true))) {
        return HttpMetadataValidation::Invalid;
    }
    return HttpMetadataValidation::Valid;
}

bool validateUniqueHttpRequestMetadataHeaders(
    const std::string& rawRequestData) noexcept {
    if (rawRequestData.size() < 4U ||
        rawRequestData.size() > kMaximumRawHttpRequestDataBytes) {
        return false;
    }
    const auto requestLineEnd = rawRequestData.find("\r\n");
    if (requestLineEnd == std::string::npos || requestLineEnd == 0U ||
        requestLineEnd > kMaximumHttpRequestUriBytes +
                             kMaximumHttpRequestLineOverheadBytes) {
        return false;
    }
    for (std::size_t index = 0U; index < requestLineEnd; ++index) {
        const auto byte = static_cast<unsigned char>(rawRequestData[index]);
        if (byte < 0x20U || byte > 0x7EU) return false;
    }

    std::array<std::size_t, kBrowserMetadataHeaders.size()> occurrences{};
    std::size_t headerBytes = 0U;
    std::size_t start = requestLineEnd + 2U;
    while (start < rawRequestData.size()) {
        const auto lineEnd = rawRequestData.find("\r\n", start);
        if (lineEnd == std::string::npos) return false;
        if (lineEnd == start) {
            return headerBytes <= kMaximumHttpRequestHeaderBlockBytes &&
                   lineEnd + 2U == rawRequestData.size();
        }
        headerBytes += lineEnd - start + 2U;
        if (headerBytes > kMaximumHttpRequestHeaderBlockBytes ||
            rawRequestData[start] == ' ' || rawRequestData[start] == '\t') {
            return false;
        }

        const auto colon = rawRequestData.find(':', start);
        if (colon == std::string::npos || colon >= lineEnd || colon == start) {
            return false;
        }
        for (std::size_t index = start; index < colon; ++index) {
            if (!isHeaderNameCharacter(
                    static_cast<unsigned char>(rawRequestData[index]))) {
                return false;
            }
        }
        for (std::size_t index = colon + 1U; index < lineEnd; ++index) {
            const auto byte = static_cast<unsigned char>(rawRequestData[index]);
            if ((byte < 0x20U && byte != '\t') || byte == 0x7FU) return false;
        }

        for (std::size_t index = 0U; index < kBrowserMetadataHeaders.size();
             ++index) {
            if (!asciiEqualIgnoreCase(rawRequestData, start, colon - start,
                                      kBrowserMetadataHeaders[index])) {
                continue;
            }
            if (++occurrences[index] > 1U) return false;
            const auto valueStart =
                rawRequestData.find_first_not_of(" \t", colon + 1U);
            if (valueStart == std::string::npos || valueStart >= lineEnd) {
                return false;
            }
        }
        start = lineEnd + 2U;
    }
    return false;
}

bool validateHttpResponseMetadata(
    const HttpResponseMetadata& metadata) noexcept {
    if (metadata.setCookie.has_value() &&
        !validValue(*metadata.setCookie, kMaximumSetCookieBytes)) {
        return false;
    }
    if (metadata.retryAfter.has_value() &&
        !validValue(*metadata.retryAfter, kMaximumRetryAfterBytes, true)) {
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
