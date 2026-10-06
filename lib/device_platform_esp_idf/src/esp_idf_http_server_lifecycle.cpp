#include "esp_idf_http_server_lifecycle.hpp"

#include <algorithm>
#include <cstdint>
#include <mutex>
#include <string>

#include "esp_http_server.h"

namespace device_platform_esp_idf {
namespace {

constexpr std::size_t kMaximumHttpBodyBytes = 4096U;
// HTTPD_DEFAULT_CONFIG() uses 4096 bytes, which the web provisioning request
// overflowed on hardware (stack overflow in task httpd). 8192 is the first
// fix candidate; hardware verification decides whether it is sufficient.
constexpr std::size_t kHttpServerTaskStackBytes = 8192U;

const char* methodName(http_method method) {
    switch (method) {
        case HTTP_GET:
            return "GET";
        case HTTP_POST:
            return "POST";
        case HTTP_PUT:
            return "PUT";
        case HTTP_DELETE:
            return "DELETE";
        default:
            return "UNKNOWN";
    }
}

const char* statusLine(std::uint16_t status) {
    switch (status) {
        case 200U:
            return "200 OK";
        case 400U:
            return "400 Bad Request";
        case 404U:
            return "404 Not Found";
        case 409U:
            return "409 Conflict";
        case 422U:
            return "422 Unprocessable Content";
        case 503U:
            return "503 Service Unavailable";
        case 401U:
            return "401 Unauthorized";
        case 403U:
            return "403 Forbidden";
        case 405U:
            return "405 Method Not Allowed";
        case 413U:
            return "413 Content Too Large";
        case 415U:
            return "415 Unsupported Media Type";
        case 429U:
            return "429 Too Many Requests";
        case 431U:
            return "431 Request Header Fields Too Large";
        case 204U:
            return "204 No Content";
        default:
            return "500 Internal Server Error";
    }
}

bool validateBrowserHeaderBlock(httpd_req_t* request) {
    if (request == nullptr) return false;
    const std::size_t rawLength = httpd_get_raw_req_data_len(request);
    if (rawLength < 4U) return false;

    // ESP-IDF 6.1 exposes the parsed header block here, not the original
    // request line. CRLF terminators have been replaced by NUL bytes. The
    // final empty line is therefore four consecutive NULs. Limit the copy to
    // the configured header block; any already-buffered request body follows
    // that terminator and is deliberately ignored.
    const std::size_t copyLength = std::min(
        rawLength, device_platform::kMaximumHttpRequestHeaderBlockBytes);
    std::string raw(copyLength, '\0');
    if (httpd_get_raw_req_data(request, raw.data(), copyLength) != ESP_OK) {
        return false;
    }

    const std::string headerTerminator(4U, '\0');
    const auto headerEnd = raw.find(headerTerminator);
    if (headerEnd == std::string::npos ||
        headerEnd + 4U > device_platform::kMaximumHttpRequestHeaderBlockBytes) {
        return false;
    }

    const std::string lineTerminator(2U, '\0');
    std::string normalized{"GET / HTTP/1.1\r\n"};
    std::size_t start = 0U;
    while (start < headerEnd) {
        const auto lineEnd = raw.find(lineTerminator, start);
        if (lineEnd == std::string::npos || lineEnd > headerEnd ||
            lineEnd == start) {
            return false;
        }
        normalized.append(raw, start, lineEnd - start);
        normalized.append("\r\n");
        start = lineEnd + lineTerminator.size();
    }
    normalized.append("\r\n");
    return device_platform::validateUniqueHttpRequestMetadataHeaders(
        normalized);
}

std::optional<std::string> browserHeader(httpd_req_t* request, const char* name,
                                         std::size_t maximum, bool& invalid) {
    const std::size_t length = httpd_req_get_hdr_value_len(request, name);
    if (length == 0U) {
        char emptyValue[1]{};
        const auto result = httpd_req_get_hdr_value_str(
            request, name, emptyValue, sizeof(emptyValue));
        if (result == ESP_ERR_NOT_FOUND) return std::nullopt;
        invalid = true;
        return std::nullopt;
    }
    if (length > maximum) {
        invalid = true;
        return std::nullopt;
    }
    std::string value(length, '\0');
    if (httpd_req_get_hdr_value_str(request, name, value.data(),
                                    value.size() + 1U) != ESP_OK) {
        invalid = true;
        return std::nullopt;
    }
    return value;
}

device_platform::HttpRequestMetadata browserMetadata(httpd_req_t* request,
                                                     bool& invalid) {
    if (!validateBrowserHeaderBlock(request)) {
        invalid = true;
        return {};
    }
    device_platform::HttpRequestMetadata metadata;
    metadata.host = browserHeader(
        request, "Host", device_platform::kMaximumHttpHostHeaderBytes, invalid);
    metadata.contentType = browserHeader(
        request, "Content-Type",
        device_platform::kMaximumHttpContentTypeHeaderBytes, invalid);
    metadata.cookie =
        browserHeader(request, "Cookie",
                      device_platform::kMaximumHttpCookieHeaderBytes, invalid);
    metadata.csrfToken =
        browserHeader(request, "X-CSRF-Token",
                      device_platform::kMaximumHttpCsrfHeaderBytes, invalid);
    metadata.mutationSeq = browserHeader(
        request, "X-UI-Mutation-Seq",
        device_platform::kMaximumHttpMutationSequenceHeaderBytes, invalid);
    metadata.origin =
        browserHeader(request, "Origin",
                      device_platform::kMaximumHttpOriginHeaderBytes, invalid);
    metadata.referer =
        browserHeader(request, "Referer",
                      device_platform::kMaximumHttpRefererHeaderBytes, invalid);
    metadata.secFetchSite = browserHeader(
        request, "Sec-Fetch-Site",
        device_platform::kMaximumHttpSecFetchSiteHeaderBytes, invalid);
    if (device_platform::validateHttpRequestMetadata(metadata) !=
        device_platform::HttpMetadataValidation::Valid) {
        invalid = true;
    }
    return metadata;
}

}  // namespace

struct EspIdfHttpServerLifecycle::Impl {
    httpd_handle_t server{nullptr};
    device_platform::IHttpRouteSink* routes{nullptr};
    mutable std::mutex mutex;

    static esp_err_t handleRequest(httpd_req_t* request);
};

EspIdfHttpServerLifecycle::EspIdfHttpServerLifecycle()
    : impl_(std::make_unique<Impl>()) {}

EspIdfHttpServerLifecycle::~EspIdfHttpServerLifecycle() {
    static_cast<void>(stop());
}

bool EspIdfHttpServerLifecycle::start(device_platform::IHttpRouteSink& routes) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (impl_->server != nullptr) {
        return false;
    }
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.stack_size = kHttpServerTaskStackBytes;
    config.max_uri_handlers = 1U;
    config.max_req_hdr_len =
        device_platform::kMaximumHttpRequestHeaderBlockBytes;
    config.max_uri_len = device_platform::kMaximumHttpRequestUriBytes;
    config.uri_match_fn = httpd_uri_match_wildcard;
    if (httpd_start(&impl_->server, &config) != ESP_OK) {
        impl_->server = nullptr;
        return false;
    }
    impl_->routes = &routes;
    const httpd_uri_t wildcard{
        .uri = "/*",
        .method = static_cast<httpd_method_t>(HTTP_ANY),
        .handler = &Impl::handleRequest,
        .user_ctx = impl_.get(),
    };
    if (httpd_register_uri_handler(impl_->server, &wildcard) != ESP_OK) {
        static_cast<void>(httpd_stop(impl_->server));
        impl_->server = nullptr;
        impl_->routes = nullptr;
        return false;
    }
    return true;
}

bool EspIdfHttpServerLifecycle::stop() {
    httpd_handle_t server = nullptr;
    {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        server = impl_->server;
        impl_->server = nullptr;
        impl_->routes = nullptr;
    }
    if (server == nullptr) {
        return true;
    }
    return httpd_stop(server) == ESP_OK;
}

bool EspIdfHttpServerLifecycle::running() const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return impl_->server != nullptr;
}

esp_err_t EspIdfHttpServerLifecycle::Impl::handleRequest(httpd_req_t* request) {
    if (request == nullptr || request->user_ctx == nullptr) {
        return ESP_FAIL;
    }
    auto* self = static_cast<Impl*>(request->user_ctx);
    device_platform::IHttpRouteSink* routes = nullptr;
    {
        std::lock_guard<std::mutex> lock(self->mutex);
        routes = self->routes;
    }
    if (routes == nullptr) {
        return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST,
                                   "invalid request");
    }
    if (request->content_len > kMaximumHttpBodyBytes) {
        return httpd_resp_send_err(request, HTTPD_413_CONTENT_TOO_LARGE,
                                   "request body too large");
    }
    bool invalidMetadata = false;
    auto metadata = browserMetadata(request, invalidMetadata);
    if (invalidMetadata) {
        return httpd_resp_send_err(request, HTTPD_431_REQ_HDR_FIELDS_TOO_LARGE,
                                   "invalid request metadata");
    }
    std::string body(request->content_len, '\0');
    std::size_t received = 0U;
    while (received < body.size()) {
        const int count = httpd_req_recv(request, body.data() + received,
                                         body.size() - received);
        if (count <= 0) {
            return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST,
                                       "request body unavailable");
        }
        received += static_cast<std::size_t>(count);
    }
    device_platform::HttpResponse response;
    const device_platform::HttpRequest input{
        methodName(static_cast<http_method>(request->method)), request->uri,
        std::move(body), std::move(metadata)};
    if (!routes->handle(input, response)) {
        return httpd_resp_send_err(request, HTTPD_404_NOT_FOUND, "not found");
    }
    if (!device_platform::validateHttpResponseMetadata(response.metadata)) {
        return httpd_resp_send_err(request, HTTPD_500_INTERNAL_SERVER_ERROR,
                                   "invalid response metadata");
    }
    static_cast<void>(
        httpd_resp_set_status(request, statusLine(response.statusCode)));
    static_cast<void>(
        httpd_resp_set_type(request, response.contentType.c_str()));
    if (response.metadata.setCookie.has_value() &&
        httpd_resp_set_hdr(request, "Set-Cookie",
                           response.metadata.setCookie->c_str()) != ESP_OK) {
        return httpd_resp_send_err(request, HTTPD_500_INTERNAL_SERVER_ERROR,
                                   "response metadata unavailable");
    }
    if (response.metadata.retryAfter.has_value() &&
        httpd_resp_set_hdr(request, "Retry-After",
                           response.metadata.retryAfter->c_str()) != ESP_OK) {
        return httpd_resp_send_err(request, HTTPD_500_INTERNAL_SERVER_ERROR,
                                   "response metadata unavailable");
    }
    return httpd_resp_send(request, response.body.data(), response.body.size());
}

}  // namespace device_platform_esp_idf
