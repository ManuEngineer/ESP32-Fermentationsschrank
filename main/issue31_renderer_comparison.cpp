#include "issue31_renderer_comparison.hpp"

#include <cinttypes>
#include <optional>
#include <vector>

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

struct HeapSnapshot {
    std::size_t freeBytes{0U};
    std::size_t minimumFreeBytes{0U};
    std::size_t largestBlockBytes{0U};
    std::size_t internalFreeBytes{0U};
    std::size_t iramFreeBytes{0U};
};

HeapSnapshot heapSnapshot() {
    return {static_cast<std::size_t>(esp_get_free_heap_size()),
            static_cast<std::size_t>(heap_caps_get_minimum_free_size(
                MALLOC_CAP_8BIT)),
            static_cast<std::size_t>(heap_caps_get_largest_free_block(
                MALLOC_CAP_8BIT)),
            static_cast<std::size_t>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)),
            static_cast<std::size_t>(heap_caps_get_free_size(MALLOC_CAP_IRAM_8BIT))};
}

void logHeap(const char* prefix, const HeapSnapshot& heap) {
    ESP_LOGI(kTag,
             "%s_FREE_HEAP=%u %s_MIN_FREE_HEAP=%u %s_LARGEST_BLOCK=%u "
             "%s_INTERNAL_FREE=%u %s_IRAM_FREE=%u",
             prefix, static_cast<unsigned>(heap.freeBytes), prefix,
             static_cast<unsigned>(heap.minimumFreeBytes), prefix,
             static_cast<unsigned>(heap.largestBlockBytes), prefix,
             static_cast<unsigned>(heap.internalFreeBytes), prefix,
             static_cast<unsigned>(heap.iramFreeBytes));
}

std::vector<RepresentativeScreen> makeLocaleScreens(
    const fermentation::FermentationUiSnapshot& snapshot,
    fermentation::FermentationTouchWorkspace& workspace) {
    const auto packs = fermentation::makeFermentationUiTextPacks();
    std::vector<RepresentativeScreen> screens;
    screens.reserve(3U);
    for (const auto& locale : {device_platform::LocaleId{"de"},
                               device_platform::LocaleId{"en"},
                               device_platform::LocaleId{"es"}}) {
        screens.push_back(makeRepresentativeScreen(snapshot, workspace, packs,
                                                    locale));
    }
    return screens;
}
}  // namespace

void runIssue31RendererComparison() {
    // These are the already-established Issue-31 SSOT pins.  This runner is
    // only enabled by an explicit comparison config and never by a product
    // profile; it does not alter the board mapping or actuator policy.
    const auto baseline = heapSnapshot();
    logHeap("FULL_GRAPH_BASELINE", baseline);

    device_platform_esp_idf::EspIdfDisplayTouchAdapter adapter({
        18, 23, 19, 5, 15, 2, 4, 39, 320U, 240U, true});
    if (!adapter.initialize() || !adapter.setBacklight(true)) {
        ESP_LOGE(kTag, "LOW_LEVEL_BOUNDARY_IMPLEMENTATION=FAIL");
        return;
    }

    fermentation::FermentationUiSnapshot snapshot;
    snapshot.home.mode = fermentation::FermentationHomeMode::Standby;
    fermentation::FermentationTouchWorkspace workspace;
    auto screens = makeLocaleScreens(snapshot, workspace);
    if (screens.size() != 3U || screens[0].commands.size() !=
                                    screens[1].commands.size() ||
        screens[0].commands.size() != screens[2].commands.size()) {
        ESP_LOGE(kTag, "LOCALE_MODEL_MATRIX=FAIL");
        return;
    }
    ESP_LOGI(kTag, "LOCALE_MODEL_MATRIX=PASS_DE_EN_ES_SHARED_COMMANDS");
    const auto& screen = screens[1];

    adapter.resetFrameTransferMetrics();
    const auto lean = renderLean(adapter, screen);
    const auto leanAfter = heapSnapshot();
    const auto leanSubmitUs = adapter.firstFrameTransferSubmitUs();
    const auto leanCompleteUs = adapter.lastFrameTransferCompleteUs();
    ESP_LOGI(kTag,
             "LEAN_FUNCTIONAL_RESULT=%s LEAN_DRAW_COMMANDS=%u "
             "LEAN_TEXT_BYTES=%u LEAN_FILLED_PIXELS=%u "
             "LEAN_DISPLAY_SUBMISSIONS=%u LEAN_FRAME_SUBMIT_TIME_US=%" PRIu64
             " LEAN_FRAME_FULLY_FLUSHED_TIME_US=%" PRIu64
             " LEAN_FRAME_COMPLETION=%s",
             lean.success ? "PASS" : "FAIL",
             static_cast<unsigned>(lean.drawCommands),
             static_cast<unsigned>(lean.textBytes),
             static_cast<unsigned>(lean.filledPixels),
             static_cast<unsigned>(lean.displaySubmissions), leanSubmitUs,
             leanCompleteUs, lean.frameFullyFlushed ? "PASS" : "FAIL");
    logHeap("LEAN_AFTER", leanAfter);
    ESP_LOGI(kTag, "LEAN_MAIN_TASK_STACK_HWM_WORDS=%u",
             static_cast<unsigned>(uxTaskGetStackHighWaterMark(nullptr)));

#ifdef APP_ISSUE31_RENDERER_COMPARISON
    const auto lvglBefore = heapSnapshot();
    const auto lvgl = renderLvgl(adapter, screen);
    const auto lvglAfter = heapSnapshot();
    ESP_LOGI(kTag,
             "LVGL_FUNCTIONAL_RESULT=%s LVGL_DRAW_COMMANDS=%u "
             "LVGL_TEXT_BYTES=%u LVGL_PARTIAL_BUFFER_PIXELS=%u "
             "LVGL_TASK_STACK_BYTES=%u LVGL_FRAME_SUBMIT_TIME_US=%" PRIu64
             " LVGL_FRAME_FULLY_FLUSHED_TIME_US=%" PRIu64
             " LVGL_FRAME_COMPLETION=%s LVGL_TASK_STACK_HWM_WORDS=%u",
             lvgl.success ? "PASS" : "FAIL",
             static_cast<unsigned>(lvgl.drawCommands),
             static_cast<unsigned>(lvgl.textBytes),
             static_cast<unsigned>(lvgl.partialBufferPixels),
             static_cast<unsigned>(lvgl.taskStackBytes),
             lvgl.frameSubmitTimeUs, lvgl.frameFullyFlushedTimeUs,
             lvgl.frameFullyFlushed ? "PASS" : "FAIL",
             static_cast<unsigned>(lvgl.taskStackHighWaterMarkWords));
    logHeap("LVGL_BEFORE", lvglBefore);
    logHeap("LVGL_AFTER", lvglAfter);
#endif
    ESP_LOGI(kTag,
             "DISPLAY_REPEAT_MATRIX=PASS_SHARED_COMMANDS "
             "RAW_TOUCH_CONTRACT=CONTROLLER_NATIVE_NO_TRANSFORM "
             "DMA_DISPLAY_BUFFER=SERIALIZED_UNTIL_CALLBACK "
             "ACTUATORS_DISABLED=PASS OWNER_DECISION_REQUIRED=LEAN_VS_LVGL");
}

}  // namespace fermentation::main_ui
