#pragma once

#include <memory>
#include <optional>
#include <vector>

#include "device_ui_text.hpp"
#include "esp_idf_display_touch_adapter.hpp"
#include "fermentation_ui_models.hpp"
#include "fermentation_ui_renderer.hpp"

namespace fermentation::main_ui {

[[nodiscard]] std::unique_ptr<class ProductiveLvglRenderer>
makeProductiveUiRenderer(
    device_platform_esp_idf::EspIdfDisplayTouchConfig config);

// The only concrete renderer lifecycle in R1. It owns the LVGL projection and
// the selected ESP-IDF display/touch adapter, while the application continues
// to own snapshots, workspace state and typed command contracts.
class ProductiveLvglRenderer final {
   public:
    explicit ProductiveLvglRenderer(
        device_platform_esp_idf::EspIdfDisplayTouchConfig config);
    ~ProductiveLvglRenderer();

    ProductiveLvglRenderer(const ProductiveLvglRenderer&) = delete;
    ProductiveLvglRenderer& operator=(const ProductiveLvglRenderer&) = delete;
    ProductiveLvglRenderer(ProductiveLvglRenderer&&) = delete;
    ProductiveLvglRenderer& operator=(ProductiveLvglRenderer&&) = delete;

    [[nodiscard]] bool initialize();
    [[nodiscard]] bool render(
        const FermentationUiSnapshot& snapshot,
        FermentationTouchWorkspace& workspace,
        const std::vector<device_platform::TextPackManifest>& textPacks,
        const device_platform::LocaleId& locale,
        std::optional<device_platform::DeviceUiTarget> pressedTarget =
            std::nullopt);
    [[nodiscard]] bool initialized() const noexcept;

   private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace fermentation::main_ui
