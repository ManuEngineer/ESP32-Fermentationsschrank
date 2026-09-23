#include "fermentation_ui_renderer.hpp"

#include "esp_lvgl_port.h"
#include "lvgl.h"

#include "esp_idf_display_touch_adapter_private.hpp"
#include "esp_idf_display_touch_adapter.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace fermentation::main_ui {
namespace {

lv_color_t color565ToLv(std::uint16_t color) {
    const auto red = static_cast<std::uint8_t>((color >> 11U) & 0x1FU);
    const auto green = static_cast<std::uint8_t>((color >> 5U) & 0x3FU);
    const auto blue = static_cast<std::uint8_t>(color & 0x1FU);
    const auto red8 = static_cast<std::uint8_t>((red * 255U) / 31U);
    const auto green8 = static_cast<std::uint8_t>((green * 255U) / 63U);
    const auto blue8 = static_cast<std::uint8_t>((blue * 255U) / 31U);
    return lv_color_make(red8, green8, blue8);
}

void lvglTransferDone(void* context) noexcept {
    auto* display = static_cast<lv_display_t*>(context);
    if (display != nullptr) lv_display_flush_ready(display);
}

}  // namespace

LvglRenderSummary renderLvgl(
    device_platform_esp_idf::EspIdfDisplayTouchAdapter& adapter,
    const RepresentativeScreen& screen) {
    device_platform_esp_idf::detail::EspIdfDisplayTouchHandles handles;
    if (!device_platform_esp_idf::detail::bindEspIdfDisplayTouchHandles(
            adapter, handles)) {
        return {};
    }

    lvgl_port_cfg_t portConfig = ESP_LVGL_PORT_INIT_CONFIG();
    if (lvgl_port_init(&portConfig) != ESP_OK) return {};

    lvgl_port_display_cfg_t displayConfig{};
    displayConfig.io_handle = handles.displayIo;
    displayConfig.panel_handle = handles.panel;
    displayConfig.control_handle = handles.panel;
    displayConfig.buffer_size = screen.kWidth * 20U;
    displayConfig.trans_size = screen.kWidth * 20U;
    displayConfig.hres = screen.kWidth;
    displayConfig.vres = screen.kHeight;
    displayConfig.color_format = LV_COLOR_FORMAT_RGB565;
    displayConfig.flags.buff_dma = 1U;
    displayConfig.flags.buff_spiram = 0U;
    displayConfig.flags.swap_bytes = 1U;
    lv_display_t* display = lvgl_port_add_disp(&displayConfig);
    if (display == nullptr) {
        (void)lvgl_port_deinit();
        return {};
    }
    if (!adapter.setDisplayTransferObserver(&lvglTransferDone, display)) {
        (void)lvgl_port_remove_disp(display);
        (void)lvgl_port_deinit();
        return {};
    }

    lvgl_port_touch_cfg_t touchConfig{};
    touchConfig.disp = display;
    touchConfig.handle = handles.touch;
    touchConfig.scale.x = 1.0F;
    touchConfig.scale.y = 1.0F;
    lv_indev_t* touch = lvgl_port_add_touch(&touchConfig);
    if (touch == nullptr || !lvgl_port_lock(1000U)) {
        if (touch != nullptr) (void)lvgl_port_remove_touch(touch);
        (void)lvgl_port_remove_disp(display);
        (void)lvgl_port_deinit();
        return {};
    }

    LvglRenderSummary result;
    result.drawCommands = screen.commands.size();
    result.partialBufferPixels = displayConfig.buffer_size;
    result.taskStackBytes = static_cast<std::size_t>(portConfig.task_stack);
    lv_obj_t* root = lv_screen_active();
    lv_obj_set_style_bg_color(root,
                              color565ToLv(themeColor565(
                                  device_platform::ThemeToken::Canvas)),
                              0U);
    lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0U);
    for (const auto& command : screen.commands) {
        if (command.kind == ScreenDrawKind::Text ||
            command.kind == ScreenDrawKind::Logo) {
            ++result.textCommands;
            result.textBytes += command.text.size();
            lv_obj_t* label = lv_label_create(root);
            lv_label_set_text(label, command.text.c_str());
            lv_obj_set_pos(label, command.rect.left, command.rect.top);
            lv_obj_set_size(label, command.rect.width, command.rect.height);
            lv_obj_set_style_text_color(label,
                                        color565ToLv(themeColor565(command.token)),
                                        0U);
            lv_obj_set_style_bg_color(
                label, color565ToLv(themeColor565(command.backgroundToken)), 0U);
            lv_obj_set_style_bg_opa(label, LV_OPA_COVER, 0U);
        } else if (command.kind == ScreenDrawKind::Fill ||
                   command.kind == ScreenDrawKind::PressFeedback) {
            lv_obj_t* fill = lv_obj_create(root);
            lv_obj_set_pos(fill, command.rect.left, command.rect.top);
            lv_obj_set_size(fill, command.rect.width, command.rect.height);
            lv_obj_set_style_bg_color(fill,
                                      color565ToLv(themeColor565(command.token)),
                                      0U);
            lv_obj_set_style_bg_opa(fill, LV_OPA_COVER, 0U);
            lv_obj_set_style_border_width(fill, 0U, 0U);
            lv_obj_set_style_radius(fill, 0U, 0U);
        }
    }
    adapter.resetFrameTransferMetrics();
    if (!adapter.beginExternalDisplayTransfer()) {
        lvgl_port_unlock();
        (void)lvgl_port_remove_touch(touch);
        (void)lvgl_port_remove_disp(display);
        (void)lvgl_port_deinit();
        vTaskDelay(2U);
        return {};
    }
    lv_obj_invalidate(root);
    lv_refr_now(display);
    lvgl_port_unlock();
    result.frameSubmitted = adapter.firstFrameTransferSubmitUs() != 0U;
    result.frameSubmitTimeUs = adapter.firstFrameTransferSubmitUs();
    result.success = adapter.waitForDisplayTransfer(1000U);
    result.frameFullyFlushed = result.success && adapter.frameTransferCompleted();
    result.frameFullyFlushedTimeUs = adapter.lastFrameTransferCompleteUs();
    result.success = result.success && result.frameSubmitted &&
                     result.frameFullyFlushed;

    if (const auto task = xTaskGetHandle("taskLVGL"); task != nullptr) {
        result.taskStackHighWaterMarkWords =
            static_cast<std::size_t>(uxTaskGetStackHighWaterMark(task));
    }

    (void)lvgl_port_remove_touch(touch);
    (void)lvgl_port_remove_disp(display);
    (void)lvgl_port_deinit();
    // The LVGL port task owns the LVGL/timer primitives and releases them
    // after deinit is requested. Give it a bounded scheduling opportunity
    // before the next resource probe starts.
    vTaskDelay(2U);
    return result;
}

}  // namespace fermentation::main_ui
