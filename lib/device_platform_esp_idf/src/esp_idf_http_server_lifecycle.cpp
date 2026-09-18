#include "esp_idf_http_server_lifecycle.hpp"

#include <cstdint>
#include <mutex>
#include <string>

#include "esp_http_server.h"

namespace device_platform_esp_idf {
namespace {

constexpr std::size_t kMaximumHttpBodyBytes = 4096U;

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
        default:
            return "500 Internal Server Error";
    }
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
    config.max_uri_handlers = 1U;
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
    if (routes == nullptr || request->content_len > kMaximumHttpBodyBytes) {
        return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST,
                                   "invalid request");
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
        std::move(body)};
    if (!routes->handle(input, response)) {
        return httpd_resp_send_err(request, HTTPD_404_NOT_FOUND, "not found");
    }
    static_cast<void>(
        httpd_resp_set_status(request, statusLine(response.statusCode)));
    static_cast<void>(
        httpd_resp_set_type(request, response.contentType.c_str()));
    return httpd_resp_send(request, response.body.data(), response.body.size());
}

}  // namespace device_platform_esp_idf
