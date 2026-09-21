#pragma once

#include <string>

#include "http_server_lifecycle.hpp"

namespace fermentation::web_browser_policy {

// Browser mutations and login require explicit same-origin evidence.  The
// transport adapter rejects duplicate/oversized headers before this pure
// application policy is called.
[[nodiscard]] bool sameOrigin(const device_platform::HttpRequest& request);
[[nodiscard]] bool exactJsonContentType(const std::string& value);

}  // namespace fermentation::web_browser_policy
