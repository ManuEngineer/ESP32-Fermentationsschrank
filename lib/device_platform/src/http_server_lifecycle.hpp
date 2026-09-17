#pragma once

#include <cstdint>
#include <string>

namespace device_platform {

struct HttpRequest {
    std::string method;
    std::string path;
    std::string body;
};

struct HttpResponse {
    std::uint16_t statusCode{500U};
    std::string contentType{"text/plain; charset=utf-8"};
    std::string body;
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
