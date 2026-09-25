#pragma once

#include "esp_idf_display_touch_adapter.hpp"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"

namespace device_platform_esp_idf::detail {

// This header is private to the concrete main/LVGL composition.  The
// public adapter header intentionally exposes none of these ESP-IDF types.
struct EspIdfDisplayTouchHandles {
    esp_lcd_panel_io_handle_t displayIo{nullptr};
    esp_lcd_panel_handle_t panel{nullptr};
};

}  // namespace device_platform_esp_idf::detail
