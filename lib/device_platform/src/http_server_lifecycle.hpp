#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <utility>

namespace device_platform {

// The application-facing HTTP boundary deliberately exposes only the small
// metadata set required by the R1 browser policy.  It is not a generic header
// map: unknown headers never become application data.
struct HttpRequestMetadata {
    std::optional<std::string> host;
    std::optional<std::string> contentType;
    std::optional<std::string> cookie;
    std::optional<std::string> csrfToken;
    std::optional<std::string> mutationSeq;
    std::optional<std::string> origin;
    std::optional<std::string> referer;
    std::optional<std::string> secFetchSite;
};

struct HttpResponseMetadata {
    std::optional<std::string> setCookie;
    std::optional<std::string> retryAfter;
};

enum class HttpMetadataValidation : std::uint8_t {
    Valid,
    Invalid,
};

[[nodiscard]] HttpMetadataValidation validateHttpRequestMetadata(
    const HttpRequestMetadata& metadata) noexcept;
[[nodiscard]] bool validateHttpResponseMetadata(
    const HttpResponseMetadata& metadata) noexcept;

struct HttpRequest {
    HttpRequest() = default;
    HttpRequest(std::string requestMethod, std::string requestPath,
                std::string requestBody,
                HttpRequestMetadata requestMetadata = {})
        : method(std::move(requestMethod)),
          path(std::move(requestPath)),
          body(std::move(requestBody)),
          metadata(std::move(requestMetadata)) {}

    std::string method;
    std::string path;
    std::string body;
    HttpRequestMetadata metadata;
};

struct HttpResponse {
    std::uint16_t statusCode{500U};
    std::string contentType{"text/plain; charset=utf-8"};
    std::string body;
    HttpResponseMetadata metadata;
};

class IHttpRouteSink {
   public:
    IHttpRouteSink() = default;
    virtual ~IHttpRouteSink() = default;

    IHttpRouteSink(const IHttpRouteSink&) = delete;
    IHttpRouteSink& operator=(const IHttpRouteSink&) = delete;
    IHttpRouteSink(IHttpRouteSink&&) = delete;
    IHttpRouteSink& operator=(IHttpRouteSink&&) = delete;

    [[nodiscard]] virtual bool handle(const HttpRequest& request,
                                      HttpResponse& response) = 0;
};

// Exactly one server lifecycle is owned by the concrete platform adapter.
// Consumers register through the one route sink; they do not start a second
// HTTP server.
class IHttpServerLifecycle {
   public:
    IHttpServerLifecycle() = default;
    virtual ~IHttpServerLifecycle() = default;

    IHttpServerLifecycle(const IHttpServerLifecycle&) = delete;
    IHttpServerLifecycle& operator=(const IHttpServerLifecycle&) = delete;
    IHttpServerLifecycle(IHttpServerLifecycle&&) = delete;
    IHttpServerLifecycle& operator=(IHttpServerLifecycle&&) = delete;

    [[nodiscard]] virtual bool start(IHttpRouteSink& routes) = 0;
    [[nodiscard]] virtual bool stop() = 0;
    [[nodiscard]] virtual bool running() const = 0;
};

}  // namespace device_platform
