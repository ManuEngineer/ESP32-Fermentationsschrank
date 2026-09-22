#include "web_browser_policy.hpp"

#include <algorithm>
#include <cctype>

namespace fermentation::web_browser_policy {
namespace {

std::string lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char byte) {
                       return static_cast<char>(std::tolower(byte));
                   });
    return value;
}

bool originMatchesHttpHost(const std::string& value, const std::string& host) {
    const auto schemeEnd = value.find("://");
    if (schemeEnd == std::string::npos || host.empty() ||
        lower(value.substr(0U, schemeEnd)) != "http") {
        return false;
    }
    const auto authorityStart = schemeEnd + 3U;
    const auto authorityEnd = value.find('/', authorityStart);
    const auto authority =
        value.substr(authorityStart, authorityEnd == std::string::npos
                                         ? std::string::npos
                                         : authorityEnd - authorityStart);
    return !authority.empty() && authority.find('@') == std::string::npos &&
           lower(authority) == lower(host);
}

std::string trim(std::string value) {
    const auto first = value.find_first_not_of(" \t");
    if (first == std::string::npos) return {};
    const auto last = value.find_last_not_of(" \t");
    return value.substr(first, last - first + 1U);
}

}  // namespace

bool sameOrigin(const device_platform::HttpRequest& request) {
    if (!request.metadata.secFetchSite.has_value()) return false;
    const auto fetchSite = lower(*request.metadata.secFetchSite);
    if (fetchSite != "same-origin" && fetchSite != "same-site" &&
        fetchSite != "none") {
        return false;
    }

    // Origin is authoritative when present. Referer is only a fallback when
    // the browser omitted Origin altogether; an explicit opaque Origin is not
    // silently converted into a trusted Referer.
    if (request.metadata.origin.has_value()) {
        return *request.metadata.origin != "null" &&
               originMatchesHttpHost(*request.metadata.origin,
                                     request.metadata.host.value_or(""));
    }
    return request.metadata.referer.has_value() &&
           originMatchesHttpHost(*request.metadata.referer,
                                 request.metadata.host.value_or(""));
}

bool exactJsonContentType(const std::string& value) {
    const auto separator = value.find(';');
    if (lower(trim(value.substr(0U, separator))) != "application/json") {
        return false;
    }
    if (separator == std::string::npos) return true;
    const auto parameter = lower(trim(value.substr(separator + 1U)));
    return parameter == "charset=utf-8" || parameter == "charset=\"utf-8\"";
}

}  // namespace fermentation::web_browser_policy
