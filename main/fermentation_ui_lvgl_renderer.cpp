#include "fermentation_ui_renderer.hpp"

#include "esp_lvgl_port.h"
#include "lvgl.h"

#include "esp_idf_display_touch_adapter_private.hpp"
#include "esp_idf_display_touch_adapter_comparison_private.hpp"
#include "esp_idf_display_touch_adapter.hpp"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
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

struct LvglFrameCompletion {
    lv_display_t* display{nullptr};
    SemaphoreHandle_t lastFlushDone{nullptr};
    bool comparisonFrameArmed{false};
    bool lastFlushCompleted{false};
    std::uint64_t completeTimestampUs{0U};
};

bool lvglTransferDone(void* context,
                      std::uint64_t completionTimestampUs) noexcept {
    auto* frame = static_cast<LvglFrameCompletion*>(context);
    if (frame == nullptr || frame->display == nullptr) return false;

    // LVGL owns the partial-refresh sequence. It must be told about every
    // completed transfer, but only the completion of its last flush completes
    // the comparison frame.
    const bool lastFlush = lv_display_flush_is_last(frame->display);
    lv_display_flush_ready(frame->display);
    if (!lastFlush || frame->lastFlushDone == nullptr) return false;

    if (frame->comparisonFrameArmed) {
        frame->comparisonFrameArmed = false;
        frame->lastFlushCompleted = true;
        frame->completeTimestampUs = completionTimestampUs;
    }
    BaseType_t higherPriorityTaskWoken = pdFALSE;
    (void)xSemaphoreGiveFromISR(frame->lastFlushDone,
                                &higherPriorityTaskWoken);
    return higherPriorityTaskWoken == pdTRUE;
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
    LvglFrameCompletion frame;
    frame.display = display;
    frame.lastFlushDone = xSemaphoreCreateBinary();
    if (frame.lastFlushDone == nullptr ||
        !device_platform_esp_idf::detail::ComparisonDisplayTransferAccess::
            installObserver(adapter, &lvglTransferDone, &frame)) {
        if (frame.lastFlushDone != nullptr) vSemaphoreDelete(frame.lastFlushDone);
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
        device_platform_esp_idf::detail::ComparisonDisplayTransferAccess::
            clearObserver(adapter);
        vSemaphoreDelete(frame.lastFlushDone);
        (void)lvgl_port_remove_disp(display);
        (void)lvgl_port_deinit();
        return {};
    }

    // Drain the display-add flush before arming the comparison frame. A
    // callback from LVGL setup is valid, but must not become the measured
    // frame's completion timestamp.
    while (xSemaphoreTake(frame.lastFlushDone, 0U) == pdTRUE) {
    }
    lv_obj_invalidate(lv_screen_active());
    lv_refr_now(display);
    lvgl_port_unlock();
    if (xSemaphoreTake(frame.lastFlushDone, pdMS_TO_TICKS(1000U)) != pdTRUE ||
        !lvgl_port_lock(1000U)) {
        device_platform_esp_idf::detail::ComparisonDisplayTransferAccess::
            clearObserver(adapter);
        vSemaphoreDelete(frame.lastFlushDone);
        return {};
    }

    LvglRenderSummary result;
    result.drawCommands = screen.commands.size();
    result.partialBufferPixels = displayConfig.buffer_size;
    result.taskStackBytes = static_cast<std::size_t>(portConfig.task_stack);
    result.renderStartTimestampUs =
        static_cast<std::uint64_t>(esp_timer_get_time());
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
    frame.lastFlushCompleted = false;
    frame.completeTimestampUs = 0U;
    frame.comparisonFrameArmed = true;
    result.frameSubmitTimestampUs =
        static_cast<std::uint64_t>(esp_timer_get_time());
    lv_obj_invalidate(root);
    lv_refr_now(display);
    lvgl_port_unlock();
    result.frameSubmitted = result.frameSubmitTimestampUs != 0U;
    const bool lastFlushSignaled =
        xSemaphoreTake(frame.lastFlushDone, pdMS_TO_TICKS(1000U)) == pdTRUE;
    result.lastFlushCompletion =
        lastFlushSignaled && frame.lastFlushCompleted;
    result.frameCompleteTimestampUs = frame.completeTimestampUs;
    result.frameFullyFlushed = result.lastFlushCompletion &&
                               result.frameCompleteTimestampUs >=
                                   result.frameSubmitTimestampUs;
    result.success = result.frameSubmitted && result.frameFullyFlushed;

    if (const auto task = xTaskGetHandle("taskLVGL"); task != nullptr) {
        result.taskStackHighWaterMarkBytes =
            static_cast<std::size_t>(uxTaskGetStackHighWaterMark(task));
    }

    if (!result.success) {
        // A timed-out frame may still own the LVGL display. Remove the
        // comparison callback so it cannot retain this stack context, but do
        // not free the display/touch/port while transfer completion is absent.
        device_platform_esp_idf::detail::ComparisonDisplayTransferAccess::
            clearObserver(adapter);
        vSemaphoreDelete(frame.lastFlushDone);
        return result;
    }

    // No callback retains lv_display_t after this point. The last transfer is
    // confirmed, so comparison-only LVGL resources can now be removed safely.
    device_platform_esp_idf::detail::ComparisonDisplayTransferAccess::
        clearObserver(adapter);
    result.callbackLifetimeReleased = true;
    vSemaphoreDelete(frame.lastFlushDone);
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
