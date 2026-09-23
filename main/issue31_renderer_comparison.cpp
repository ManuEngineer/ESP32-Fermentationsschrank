#include "issue31_renderer_comparison.hpp"

#include <cinttypes>

#include "esp_heap_caps.h"
#include "esp_idf_display_touch_adapter.hpp"
#include "esp_log.h"
#include "esp_timer.h"
#include "fermentation_ui_renderer.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace fermentation::main_ui {

LvglRenderSummary renderLvgl(
    device_platform_esp_idf::EspIdfDisplayTouchAdapter& adapter,
    const RepresentativeScreen& screen);

namespace {
constexpr char kTag[] = "issue31_renderer";
}  // namespace

void runIssue31RendererComparison() {
    // These are the already-established Issue-31 SSOT pins.  This runner is
    // only enabled by an explicit comparison config and never by a product
    // profile; it does not alter the board mapping or actuator policy.
    device_platform_esp_idf::EspIdfDisplayTouchAdapter adapter({
        18, 23, 19, 5, 15, 2, 4, 39, 320U, 240U, true});
    if (!adapter.initialize() || !adapter.setBacklight(true)) {
        ESP_LOGE(kTag, "LOW_LEVEL_BOUNDARY_IMPLEMENTATION=FAIL");
        return;
    }

    fermentation::FermentationUiSnapshot snapshot;
    snapshot.home.mode = fermentation::FermentationHomeMode::Standby;
    fermentation::FermentationTouchWorkspace workspace;
    const auto screen = makeRepresentativeScreen(
        snapshot, workspace, fermentation::makeFermentationUiTextPacks(),
        device_platform::LocaleId{"en"});

    const auto leanStartUs = esp_timer_get_time();
    const auto lean = renderLean(adapter, screen);
    const auto leanUpdateUs = esp_timer_get_time() - leanStartUs;
    ESP_LOGI(kTag,
             "LEAN_FUNCTIONAL_RESULT=%s LEAN_DRAW_COMMANDS=%u "
             "LEAN_TEXT_BYTES=%u LEAN_FILLED_PIXELS=%u "
             "LEAN_FREE_MIN_HEAP=%u LEAN_LARGEST_HEAP=%u "
             "LEAN_TASK_STACK_HWM_WORDS=%u LEAN_UPDATE_TIMING_US=%" PRId64,
             lean.success ? "PASS" : "FAIL",
             static_cast<unsigned>(lean.drawCommands),
             static_cast<unsigned>(lean.textBytes),
             static_cast<unsigned>(lean.filledPixels),
             static_cast<unsigned>(
                 heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT)),
             static_cast<unsigned>(
                 heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)),
             static_cast<unsigned>(uxTaskGetStackHighWaterMark(nullptr)),
             leanUpdateUs);

#ifdef APP_ISSUE31_RENDERER_COMPARISON
    const auto lvglStartUs = esp_timer_get_time();
    const auto lvgl = renderLvgl(adapter, screen);
    const auto lvglUpdateUs = esp_timer_get_time() - lvglStartUs;
    ESP_LOGI(kTag,
             "LVGL_FUNCTIONAL_RESULT=%s LVGL_DRAW_COMMANDS=%u "
             "LVGL_TEXT_BYTES=%u LVGL_PARTIAL_BUFFER_PIXELS=%u "
             "LVGL_TASK_STACK_BYTES=%u LVGL_FREE_MIN_HEAP=%u "
             "LVGL_LARGEST_HEAP=%u LVGL_TASK_STACK_HWM_WORDS=%u "
             "LVGL_UPDATE_TIMING_US=%" PRId64,
             lvgl.success ? "PASS" : "FAIL",
             static_cast<unsigned>(lvgl.drawCommands),
             static_cast<unsigned>(lvgl.textBytes),
             static_cast<unsigned>(lvgl.partialBufferPixels),
             static_cast<unsigned>(lvgl.taskStackBytes),
             static_cast<unsigned>(
                 heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT)),
             static_cast<unsigned>(
                 heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)),
             static_cast<unsigned>(uxTaskGetStackHighWaterMark(nullptr)),
             lvglUpdateUs);
#endif
    ESP_LOGI(kTag, "ACTUATORS_DISABLED=PASS OWNER_DECISION_REQUIRED=LEAN_VS_LVGL");
}

}  // namespace fermentation::main_ui
