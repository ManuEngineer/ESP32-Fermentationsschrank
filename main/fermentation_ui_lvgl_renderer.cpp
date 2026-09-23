#include "fermentation_ui_lvgl_renderer.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <utility>

#include "esp_idf_display_touch_adapter_private.hpp"
#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "lvgl.h"

namespace fermentation::main_ui {
namespace {

lv_color_t color565ToLv(std::uint16_t color) {
    const auto red = static_cast<std::uint8_t>((color >> 11U) & 0x1FU);
    const auto green = static_cast<std::uint8_t>((color >> 5U) & 0x3FU);
    const auto blue = static_cast<std::uint8_t>(color & 0x1FU);
    return lv_color_make(static_cast<std::uint8_t>((red * 255U) / 31U),
                         static_cast<std::uint8_t>((green * 255U) / 63U),
                         static_cast<std::uint8_t>((blue * 255U) / 31U));
}

void styleObject(lv_obj_t* object, device_platform::ThemeToken foreground,
                 device_platform::ThemeToken background) {
    lv_obj_set_style_text_color(object,
                                color565ToLv(themeColor565(foreground)), 0U);
    lv_obj_set_style_bg_color(object,
                              color565ToLv(themeColor565(background)), 0U);
    lv_obj_set_style_bg_opa(object, LV_OPA_COVER, 0U);
    lv_obj_set_style_border_width(object, 0U, 0U);
    lv_obj_set_style_radius(object, 0U, 0U);
}

}  // namespace

struct ProductiveLvglRenderer::Impl final {
    explicit Impl(device_platform_esp_idf::EspIdfDisplayTouchConfig value)
        : config(std::move(value)) {}

    ~Impl() {
        if (touchInput != nullptr) {
            lv_indev_delete(touchInput);
            touchInput = nullptr;
        }
        if (display != nullptr) {
            (void)lvgl_port_remove_disp(display);
            display = nullptr;
        }
        if (portStarted) {
            (void)lvgl_port_deinit();
            portStarted = false;
        }
    }

    device_platform_esp_idf::EspIdfDisplayTouchConfig config;
    std::unique_ptr<device_platform_esp_idf::EspIdfDisplayTouchAdapter>
        adapter;
    lv_display_t* display{nullptr};
    lv_obj_t* root{nullptr};
    lv_indev_t* touchInput{nullptr};
    bool portStarted{false};
    bool initialized{false};
    bool touchCalibrationAvailable{false};
    bool touchCalibrationWarningLogged{false};
    std::optional<ScreenRenderKey> renderedKey;

    static void readTouchFailClosed(lv_indev_t* indev,
                                    lv_indev_data_t* data) {
        auto* state = static_cast<Impl*>(lv_indev_get_user_data(indev));
        data->state = LV_INDEV_STATE_RELEASED;
        if (state == nullptr || state->adapter == nullptr) return;

        // The XPT2046 adapter intentionally exposes controller-native raw
        // values. Until the planned, persisted board calibration is
        // available, do not invent a coordinate transform and do not turn
        // raw values into actions.
        const auto sample = state->adapter->sampleTouch();
        if (!state->touchCalibrationAvailable &&
            !state->touchCalibrationWarningLogged) {
            state->touchCalibrationWarningLogged = true;
            ESP_LOGW("issue31_ui",
                     "touch input held released until calibration is available");
            (void)sample;
        }
    }
};

std::unique_ptr<ProductiveLvglRenderer> makeProductiveUiRenderer(
    device_platform_esp_idf::EspIdfDisplayTouchConfig config) {
    return std::make_unique<ProductiveLvglRenderer>(std::move(config));
}

ProductiveLvglRenderer::ProductiveLvglRenderer(
    device_platform_esp_idf::EspIdfDisplayTouchConfig config)
    : impl_(std::make_unique<Impl>(std::move(config))) {}

ProductiveLvglRenderer::~ProductiveLvglRenderer() = default;

bool ProductiveLvglRenderer::initialize() {
    auto& state = *impl_;
    if (state.initialized) return true;

    state.adapter = std::make_unique<
        device_platform_esp_idf::EspIdfDisplayTouchAdapter>(state.config);
    if (state.adapter == nullptr || !state.adapter->initialize() ||
        !state.adapter->setRotation(device_platform::DisplayRotation::Rotate0) ||
        !state.adapter->setBacklight(true)) {
        return false;
    }

    device_platform_esp_idf::detail::EspIdfDisplayTouchHandles handles;
    if (!device_platform_esp_idf::detail::bindEspIdfDisplayTouchHandles(
            *state.adapter, handles)) {
        return false;
    }

    lvgl_port_cfg_t portConfig = ESP_LVGL_PORT_INIT_CONFIG();
    if (lvgl_port_init(&portConfig) != ESP_OK) return false;
    state.portStarted = true;

    lvgl_port_display_cfg_t displayConfig{};
    displayConfig.io_handle = handles.displayIo;
    displayConfig.panel_handle = handles.panel;
    displayConfig.control_handle = handles.panel;
    displayConfig.buffer_size = RepresentativeScreen::kWidth * 20U;
    displayConfig.trans_size = RepresentativeScreen::kWidth * 20U;
    displayConfig.hres = RepresentativeScreen::kWidth;
    displayConfig.vres = RepresentativeScreen::kHeight;
    displayConfig.color_format = LV_COLOR_FORMAT_RGB565;
    displayConfig.flags.buff_dma = 1U;
    displayConfig.flags.buff_spiram = 0U;
    displayConfig.flags.swap_bytes = 1U;
    state.display = lvgl_port_add_disp(&displayConfig);
    if (state.display == nullptr) return false;

    // The neutral adapter remains the sole raw-touch owner. LVGL gets a
    // bounded input device whose read callback stays fail-closed until the
    // persisted board calibration contract is available.
    if (!lvgl_port_lock(1000U)) return false;
    state.root = lv_screen_active();
    state.touchInput = lv_indev_create();
    if (state.touchInput == nullptr) {
        lvgl_port_unlock();
        return false;
    }
    lv_indev_set_type(state.touchInput, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(state.touchInput, &Impl::readTouchFailClosed);
    lv_indev_set_user_data(state.touchInput, &state);
    lvgl_port_unlock();

    state.initialized = true;
    return true;
}

bool ProductiveLvglRenderer::render(
    const FermentationUiSnapshot& snapshot, FermentationTouchWorkspace& workspace,
    const std::vector<device_platform::TextPackManifest>& textPacks,
    const device_platform::LocaleId& locale,
    std::optional<device_platform::DeviceUiTarget> pressedTarget) {
    auto& state = *impl_;
    if (!state.initialized || state.display == nullptr || state.root == nullptr)
        return false;

    // Building the screen model is a cheap pure projection; only the LVGL
    // widget rebuild below is expensive. The render key captures every
    // semantic input relevant to the visible projection (application
    // revision, local workspace/page/pager/dialog state, locale and header
    // values), so a page/pager/locale change is never masked by an unchanged
    // application UiRefreshRevision.
    const auto screen = makeRepresentativeScreen(snapshot, workspace, textPacks,
                                                  locale, pressedTarget);
    const auto key = makeScreenRenderKey(screen);
    if (state.renderedKey.has_value() && *state.renderedKey == key) {
        return true;
    }
    if (!lvgl_port_lock(1000U)) return false;

    lv_obj_clean(state.root);
    lv_obj_set_style_bg_color(
        state.root,
        color565ToLv(themeColor565(device_platform::ThemeToken::Canvas)), 0U);
    lv_obj_set_style_bg_opa(state.root, LV_OPA_COVER, 0U);

    for (const auto& command : screen.commands) {
        if (command.kind == ScreenDrawKind::Text ||
            command.kind == ScreenDrawKind::Logo) {
            auto* label = lv_label_create(state.root);
            lv_label_set_text(label, command.text.c_str());
            lv_obj_set_pos(label, command.rect.left, command.rect.top);
            lv_obj_set_size(label, command.rect.width, command.rect.height);
            lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP);
            styleObject(label, command.token, command.backgroundToken);
        } else if (command.kind == ScreenDrawKind::Fill ||
                   command.kind == ScreenDrawKind::PressFeedback) {
            auto* fill = lv_obj_create(state.root);
            lv_obj_set_pos(fill, command.rect.left, command.rect.top);
            lv_obj_set_size(fill, command.rect.width, command.rect.height);
            styleObject(fill, command.token, command.token);
        }
    }
    lv_obj_invalidate(state.root);
    lvgl_port_unlock();
    state.renderedKey = key;
    return true;
}

bool ProductiveLvglRenderer::initialized() const noexcept {
    return impl_ != nullptr && impl_->initialized;
}

}  // namespace fermentation::main_ui
