#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

namespace device_platform {

inline constexpr std::size_t kMaximumHttpRequestMetadataBytes = 2048U;
inline constexpr std::size_t kMaximumHttpRequestHeaderBlockBytes = 2048U;
inline constexpr std::size_t kMaximumHttpRequestUriBytes = 512U;
inline constexpr std::size_t kMaximumHttpRequestLineOverheadBytes = 32U;
inline constexpr std::size_t kMaximumRawHttpRequestDataBytes =
    kMaximumHttpRequestHeaderBlockBytes + kMaximumHttpRequestUriBytes +
    kMaximumHttpRequestLineOverheadBytes;
inline constexpr std::size_t kMaximumHttpHostHeaderBytes = 256U;
inline constexpr std::size_t kMaximumHttpContentTypeHeaderBytes = 64U;
inline constexpr std::size_t kMaximumHttpCookieHeaderBytes = 512U;
inline constexpr std::size_t kMaximumHttpCsrfHeaderBytes = 64U;
inline constexpr std::size_t kMaximumHttpMutationSequenceHeaderBytes = 20U;
inline constexpr std::size_t kMaximumHttpOriginHeaderBytes = 256U;
inline constexpr std::size_t kMaximumHttpRefererHeaderBytes = 512U;
inline constexpr std::size_t kMaximumHttpSecFetchSiteHeaderBytes = 32U;

// Only the bounded browser metadata required by the local Web UI crosses
// this platform boundary. Arbitrary request/response header maps are
// deliberately not part of the portable HTTP contract.
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
// Validates the bounded raw request-line/header block returned by the HTTP
// transport and rejects duplicate or empty browser-security metadata fields.
[[nodiscard]] bool validateUniqueHttpRequestMetadataHeaders(
    const std::string& rawRequestData) noexcept;
[[nodiscard]] bool validateHttpResponseMetadata(
    const HttpResponseMetadata& metadata) noexcept;

struct HttpRequest {
    std::string method;
    std::string path;
    std::string body;
    HttpRequestMetadata metadata{};
};

struct HttpResponse {
    std::uint16_t statusCode{500U};
    std::string contentType{"text/plain; charset=utf-8"};
    std::string body;
    HttpResponseMetadata metadata{};
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
