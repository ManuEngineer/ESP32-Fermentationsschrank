#include "fermentation_ui_lvgl_renderer.hpp"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <optional>
#include <utility>

#include "esp_idf_display_touch_adapter_private.hpp"
#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "generated/manuengineer_logo_168x24.h"
#include "lvgl.h"
#include "touch_calibration.hpp"

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
    lv_obj_set_style_text_color(object, color565ToLv(themeColor565(foreground)),
                                0U);
    lv_obj_set_style_bg_color(object, color565ToLv(themeColor565(background)),
                              0U);
    lv_obj_set_style_bg_opa(object, LV_OPA_COVER, 0U);
    lv_obj_set_style_border_width(object, 0U, 0U);
    lv_obj_set_style_radius(object, 0U, 0U);
}

void showQrUnavailable(lv_obj_t* parent,
                       const device_platform::DisplayRect& rect) {
    auto* label = lv_label_create(parent);
    if (label == nullptr) return;
    lv_label_set_text(label, "QR unavailable");
    lv_obj_set_pos(label, rect.left, rect.top);
    lv_obj_set_style_text_font(label, LV_FONT_DEFAULT, 0U);
    lv_obj_set_size(label, rect.width,
                    lv_font_get_line_height(LV_FONT_DEFAULT));
    styleObject(label, device_platform::ThemeToken::StatusWarning,
                device_platform::ThemeToken::Canvas);
}

std::uint16_t clampToDisplay(double value, std::uint16_t maxInclusive) {
    if (value < 0.0) return 0U;
    const auto upper = static_cast<double>(maxInclusive);
    if (value > upper) return maxInclusive;
    return static_cast<std::uint16_t>(value);
}

}  // namespace

struct ProductiveLvglRenderer::Impl final {
    explicit Impl(device_platform_esp_idf::EspIdfDisplayTouchConfig value)
        : config(std::move(value)) {}

    ~Impl() {
        // lvgl_port_deinit() only clears a flag the LVGL task observes on
        // its next loop iteration - it does not synchronously stop the
        // task. The lock is therefore what actually prevents deleting
        // touchInput/display while the LVGL task could still be mid-tick
        // using them; lv_indev_delete() also fully unregisters the indev
        // from LVGL's own list, so the task never revisits it afterward
        // regardless of exact task-exit timing.
        //
        // lvgl_port_lock(0) blocks indefinitely (esp_lvgl_port maps a 0 ms
        // timeout to portMAX_DELAY, confirmed against esp_lvgl_port.c) -
        // this is the documented, intended teardown contract, not merely a
        // best-effort attempt. There is deliberately no unprotected
        // fallback delete on a lock failure: a partially initialized
        // handle (initialize() failed after creating some, but not all,
        // objects) is torn down the same way, under the same lock.
        if (portStarted) {
            if (lvgl_port_lock(0U)) {
                if (touchInput != nullptr) {
                    lv_indev_delete(touchInput);
                    touchInput = nullptr;
                }
                if (display != nullptr) {
                    (void)lvgl_port_remove_disp(display);
                    display = nullptr;
                }
                lvgl_port_unlock();
            }
            // If the lock could not be taken (should never happen with an
            // indefinite wait, but this is the exact case the Auftrag
            // names), touchInput/display are deliberately left non-null
            // rather than deleted unprotected: a leak, not a possible
            // use-after-free.
            (void)lvgl_port_deinit();
            portStarted = false;
        }
    }

    device_platform_esp_idf::EspIdfDisplayTouchConfig config;
    std::unique_ptr<device_platform_esp_idf::EspIdfDisplayTouchAdapter> adapter;
    lv_display_t* display{nullptr};
    lv_obj_t* root{nullptr};
    lv_indev_t* touchInput{nullptr};
    bool portStarted{false};
    bool initialized{false};
    // Set only from a real TouchCalibrationLoadStatus::Available
    // classification (see ProductiveLvglRenderer::setTouchCalibration()).
    // No default/placeholder model is ever substituted here. Written from
    // the caller's task under lvgl_port_lock(); read from the LVGL task,
    // which already holds the same (recursive) lock for the whole duration
    // of the indev read callback (see esp_lvgl_port's task loop, which
    // wraps lv_indev_read() in lvgl_port_lock()) - no additional lock is
    // taken on that read side.
    std::optional<device_platform::TouchCalibrationModel> touchCalibrationModel;
    bool touchCalibrationWarningLogged{false};
    std::optional<ScreenRenderKey> renderedKey;

    // Touch poll state the LVGL task's read callback publishes and
    // ProductiveLvglRenderer::pollTouch() (caller's task) consumes, both
    // under lvgl_port_lock() on the consuming side. This is the only touch
    // sampling point; pollTouch() never re-samples the adapter itself, so
    // the LVGL pointer and the #26 target/press path always agree on the
    // same observed contact.
    bool touchPressActive{false};
    std::uint16_t touchPressX{0U};
    std::uint16_t touchPressY{0U};
    bool touchPressEdgePending{false};

    static void readTouchFailClosed(lv_indev_t* indev, lv_indev_data_t* data) {
        auto* state = static_cast<Impl*>(lv_indev_get_user_data(indev));
        data->state = LV_INDEV_STATE_RELEASED;
        if (state == nullptr || state->adapter == nullptr) return;

        // The XPT2046 adapter intentionally exposes controller-native raw
        // values. Without an available, well-formed persisted calibration,
        // raw values are never turned into a coordinate or an action.
        const auto sample = state->adapter->sampleTouch();
        if (!state->touchCalibrationModel.has_value()) {
            if (!state->touchCalibrationWarningLogged) {
                state->touchCalibrationWarningLogged = true;
                ESP_LOGW(
                    "issue31_ui",
                    "touch input held released until calibration is available");
            }
            state->touchPressActive = false;
            return;
        }
        if (sample.status != device_platform::RawTouchSampleStatus::Contact ||
            !sample.contact) {
            state->touchPressActive = false;
            return;
        }
        auto calibrated = device_platform::applyTouchCalibration(
            *state->touchCalibrationModel, sample.rawX, sample.rawY);
        // Clamp to the fixed 320x240 display surface: a calibration record
        // is trusted for its coefficients, never for guaranteeing every
        // transformed point stays on-screen.
        const auto clampedX =
            clampToDisplay(calibrated.x, RepresentativeScreen::kWidth - 1U);
        const auto clampedY =
            clampToDisplay(calibrated.y, RepresentativeScreen::kHeight - 1U);
        data->point.x = static_cast<lv_coord_t>(clampedX);
        data->point.y = static_cast<lv_coord_t>(clampedY);
        data->state = LV_INDEV_STATE_PRESSED;

        if (!state->touchPressActive) {
            state->touchPressEdgePending = true;
        }
        state->touchPressActive = true;
        state->touchPressX = clampedX;
        state->touchPressY = clampedY;
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

void ProductiveLvglRenderer::setTouchCalibration(
    std::optional<device_platform::TouchCalibrationModel> activeModel) {
    auto& state = *impl_;
    if (!state.portStarted) {
        // The LVGL task does not exist yet; there is nothing to serialize
        // against, and no touch state has been published yet either.
        state.touchCalibrationModel = std::move(activeModel);
        return;
    }
    if (!lvgl_port_lock(1000U)) {
        ESP_LOGW("issue31_ui",
                 "touch calibration update dropped: lvgl port lock timeout, "
                 "previous calibration state unchanged");
        return;
    }
    state.touchCalibrationModel = std::move(activeModel);
    // A calibration change/clear invalidates any already-published touch
    // state: a contact or fresh-press edge observed under the previous
    // model must never be consumed under the new one. Only a genuinely new
    // raw contact sampled after this point may produce a fresh press
    // again. This is the same published state pollTouch() reads, reset
    // under the same lock - no second event state is introduced.
    state.touchPressActive = false;
    state.touchPressEdgePending = false;
    state.touchPressX = 0U;
    state.touchPressY = 0U;
    lvgl_port_unlock();
}

ProductiveLvglRenderer::TouchPollResult ProductiveLvglRenderer::pollTouch() {
    TouchPollResult result;
    auto& state = *impl_;
    if (!state.initialized || !state.portStarted) {
        // No event, rather than fabricating a release.
        return result;
    }
    if (!lvgl_port_lock(1000U)) {
        return result;
    }
    result.contactHeld = state.touchPressActive;
    if (state.touchPressActive) {
        result.point = TouchPoint{state.touchPressX, state.touchPressY};
    }
    if (state.touchPressEdgePending) {
        result.freshPressEdge = true;
        state.touchPressEdgePending = false;
    }
    lvgl_port_unlock();
    return result;
}

bool ProductiveLvglRenderer::initialize() {
    auto& state = *impl_;
    if (state.initialized) return true;

    state.adapter =
        std::make_unique<device_platform_esp_idf::EspIdfDisplayTouchAdapter>(
            state.config);
    if (state.adapter == nullptr || !state.adapter->initialize() ||
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
    // Keep LVGL's initial hardware rotation aligned with the same R1
    // rotation already applied by the adapter. The LVGL port otherwise
    // restores its zero-initialized rotation and undoes the landscape setup.
    const auto rotation = device_platform_esp_idf::displayRotationTransform(
        state.config.rotation);
    displayConfig.rotation.swap_xy = rotation.swap_xy;
    displayConfig.rotation.mirror_x = rotation.mirror_x;
    displayConfig.rotation.mirror_y = rotation.mirror_y;
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
    const FermentationUiSnapshot& snapshot,
    FermentationTouchWorkspace& workspace,
    const std::vector<device_platform::TextPackManifest>& textPacks,
    const device_platform::LocaleId& locale,
    std::optional<device_platform::DeviceUiTarget> pressedTarget,
    const ProgramCatalog* catalog,
    device_platform::DeviceUiNetworkStatus networkStatus,
    device_platform::ClockViewInput clock,
    const std::optional<device_platform::NetworkAccessPointInfo>&
        networkAccessPointInfo) {
    auto& state = *impl_;
    if (!state.initialized || state.display == nullptr || state.root == nullptr)
        return false;

    // Building the screen model is a cheap pure projection; only the LVGL
    // widget rebuild below is expensive. The render key captures every
    // semantic input relevant to the visible projection (application
    // revision, local workspace/page/pager/dialog state, locale and header
    // values), so a page/pager/locale change is never masked by an unchanged
    // application UiRefreshRevision.
    const auto screen = makeRepresentativeScreen(
        snapshot, workspace, textPacks, locale, pressedTarget, catalog,
        networkStatus, clock, networkAccessPointInfo);
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
        if (command.kind == ScreenDrawKind::Logo) {
            // The generated asset (scripts/generate_branding_asset.py,
            // main/generated/manuengineer_logo_168x24.{c,h}) is exactly
            // 168x24, matching this command's rect exactly - no stretch or
            // crop. ScreenDrawCommand carries no image-descriptor pointer
            // (it stays a renderer-independent model); this main-side
            // compile-time constant is looked up here only.
            auto* logo = lv_image_create(state.root);
            lv_image_set_src(logo, &manuengineer_logo_168x24);
            lv_obj_set_pos(logo, command.rect.left, command.rect.top);
        } else if (command.kind == ScreenDrawKind::QrCode) {
            auto* qrCode = lv_qrcode_create(state.root);
            if (qrCode == nullptr) {
                showQrUnavailable(state.root, command.rect);
                continue;
            }
            lv_qrcode_set_size(qrCode, command.rect.width);
            lv_obj_set_pos(qrCode, command.rect.left, command.rect.top);
            const auto updateResult = lv_qrcode_update(
                qrCode, command.text.data(),
                static_cast<std::uint32_t>(command.text.size()));
            if (updateResult != LV_RESULT_OK) {
                showQrUnavailable(state.root, command.rect);
            }
        } else if (command.kind == ScreenDrawKind::Text ||
                   command.kind == ScreenDrawKind::NetworkStatusIcon) {
            auto* label = lv_label_create(state.root);
            const char* text = command.kind == ScreenDrawKind::NetworkStatusIcon
                                   ? LV_SYMBOL_WIFI
                                   : command.text.c_str();
            lv_label_set_text(label, text);
            lv_obj_set_pos(label, command.rect.left, command.rect.top);
            lv_obj_set_style_text_font(label, LV_FONT_DEFAULT, 0U);
            const auto lineHeight = std::max<lv_coord_t>(
                static_cast<lv_coord_t>(command.rect.height),
                lv_font_get_line_height(LV_FONT_DEFAULT));
            lv_obj_set_size(label, command.rect.width, lineHeight);
            lv_label_set_long_mode(label, command.wrapText
                                              ? LV_LABEL_LONG_WRAP
                                              : LV_LABEL_LONG_CLIP);
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
