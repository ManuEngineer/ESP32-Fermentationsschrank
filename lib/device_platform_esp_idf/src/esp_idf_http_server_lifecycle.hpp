#pragma once

#include <memory>

#include "http_server_lifecycle.hpp"

namespace device_platform_esp_idf {

class EspIdfHttpServerLifecycle final
    : public device_platform::IHttpServerLifecycle {
   public:
    EspIdfHttpServerLifecycle();
    ~EspIdfHttpServerLifecycle() override;

    EspIdfHttpServerLifecycle(const EspIdfHttpServerLifecycle&) = delete;
    EspIdfHttpServerLifecycle& operator=(const EspIdfHttpServerLifecycle&) =
        delete;
    EspIdfHttpServerLifecycle(EspIdfHttpServerLifecycle&&) = delete;
    EspIdfHttpServerLifecycle& operator=(EspIdfHttpServerLifecycle&&) = delete;

    [[nodiscard]] bool start(device_platform::IHttpRouteSink& routes) override;
    [[nodiscard]] bool stop() override;
    [[nodiscard]] bool running() const override;

   private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace device_platform_esp_idf
