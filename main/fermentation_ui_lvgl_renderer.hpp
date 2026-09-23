#pragma once

#include <memory>
#include <optional>
#include <vector>

#include "device_ui_text.hpp"
#include "esp_idf_display_touch_adapter.hpp"
#include "fermentation_ui_models.hpp"
#include "fermentation_ui_renderer.hpp"
#include "touch_calibration.hpp"

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
    // Sets or clears the active calibration model the touch input callback
    // uses. Passing std::nullopt (the default until this is called) keeps
    // the touch path fail-closed. The caller is responsible for the load/
    // classify decision (device_platform::TouchCalibrationStore) and for
    // only ever passing a model from a TouchCalibrationLoadStatus::Available
    // result; this renderer never loads, classifies or invents one itself.
    void setTouchCalibration(
        std::optional<device_platform::TouchCalibrationModel> activeModel);
    [[nodiscard]] bool render(
        const FermentationUiSnapshot& snapshot,
        FermentationTouchWorkspace& workspace,
        const std::vector<device_platform::TextPackManifest>& textPacks,
        const device_platform::LocaleId& locale,
        std::optional<device_platform::DeviceUiTarget> pressedTarget =
            std::nullopt,
        const ProgramCatalog* catalog = nullptr,
        device_platform::DeviceUiNetworkStatus networkStatus =
            device_platform::DeviceUiNetworkStatus::Unavailable,
        device_platform::ClockViewInput clock = {});
    [[nodiscard]] bool initialized() const noexcept;

   private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace fermentation::main_ui
