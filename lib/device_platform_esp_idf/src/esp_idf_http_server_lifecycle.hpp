#pragma once

#include "esp_http_server.h"
#include "http_server_lifecycle.hpp"

namespace device_platform_esp_idf {

class EspIdfHttpServerLifecycle final
    : public device_platform::IHttpServerLifecycle {
   public:
    EspIdfHttpServerLifecycle() = default;
    ~EspIdfHttpServerLifecycle() override;

    EspIdfHttpServerLifecycle(const EspIdfHttpServerLifecycle&) = delete;
    EspIdfHttpServerLifecycle& operator=(const EspIdfHttpServerLifecycle&) =
        delete;
    EspIdfHttpServerLifecycle(EspIdfHttpServerLifecycle&&) = delete;
    EspIdfHttpServerLifecycle& operator=(EspIdfHttpServerLifecycle&&) = delete;

    [[nodiscard]] bool start(device_platform::IHttpRouteSink& routes) override;
    [[nodiscard]] bool stop() override;
    [[nodiscard]] bool running() const override { return server_ != nullptr; }

   private:
    static esp_err_t handleRequest(httpd_req_t* request);

    httpd_handle_t server_{nullptr};
    device_platform::IHttpRouteSink* routes_{nullptr};
};

}  // namespace device_platform_esp_idf
