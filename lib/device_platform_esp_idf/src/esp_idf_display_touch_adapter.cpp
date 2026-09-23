#include "esp_idf_display_touch_adapter.hpp"

#include "../private/esp_idf_display_touch_adapter_private.hpp"

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <limits>
#include <utility>

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_heap_caps.h"
#include "esp_lcd_ili9341.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_touch.h"
#include "esp_lcd_touch_xpt2046.h"
#include "esp_attr.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

namespace device_platform_esp_idf {
namespace {

constexpr spi_host_device_t kSpiHost = SPI2_HOST;
constexpr std::size_t kPartialBufferPixels = 320U * 8U;

bool validPin(int pin) noexcept { return pin >= 0; }

gpio_num_t gpio(int pin) noexcept { return static_cast<gpio_num_t>(pin); }

}  // namespace

class EspIdfDisplayTouchAdapter::Impl final {
   public:
    explicit Impl(EspIdfDisplayTouchConfig value) : config(std::move(value)) {}

    ~Impl() {
        // esp_lcd_panel_draw_bitmap() retains the DMA buffer until its
        // on_color_trans_done callback. Never free or recycle it before the
        // callback, even on the teardown path.
        if (transferPending && transferDone != nullptr) {
            (void)xSemaphoreTake(transferDone, portMAX_DELAY);
            transferPending = false;
        }
        if (touch != nullptr) {
            (void)esp_lcd_touch_del(touch);
            touch = nullptr;
        }
        if (touchIo != nullptr) {
            (void)esp_lcd_panel_io_del(touchIo);
            touchIo = nullptr;
        }
        if (panel != nullptr) {
            (void)esp_lcd_panel_del(panel);
            panel = nullptr;
        }
        if (displayIo != nullptr) {
            (void)esp_lcd_panel_io_del(displayIo);
            displayIo = nullptr;
        }
        if (busInitialized) {
            (void)spi_bus_free(kSpiHost);
            busInitialized = false;
        }
        if (dmaPixels != nullptr) {
            heap_caps_free(dmaPixels);
            dmaPixels = nullptr;
        }
        if (transferDone != nullptr) {
            vSemaphoreDelete(transferDone);
            transferDone = nullptr;
        }
        if (validPin(config.backlightPin)) {
            (void)gpio_reset_pin(gpio(config.backlightPin));
        }
    }

    EspIdfDisplayTouchConfig config;
    esp_lcd_panel_io_handle_t displayIo{nullptr};
    esp_lcd_panel_io_handle_t touchIo{nullptr};
    esp_lcd_panel_handle_t panel{nullptr};
    esp_lcd_touch_handle_t touch{nullptr};
    std::uint16_t* dmaPixels{nullptr};
    bool busInitialized{false};
    bool initialized{false};
    SemaphoreHandle_t transferDone{nullptr};
    bool transferPending{false};
    bool transferFaulted{false};
    EspIdfDisplayTouchAdapter::DisplayTransferObserver observer{nullptr};
    void* observerContext{nullptr};
    std::uint64_t firstSubmitUs{0U};
    std::uint64_t lastCompleteUs{0U};
    device_platform::DisplayRotation rotation{
        device_platform::DisplayRotation::Rotate0};

    static bool IRAM_ATTR onColorTransferDone(
        esp_lcd_panel_io_handle_t, esp_lcd_panel_io_event_data_t*,
        void* userContext) noexcept {
        auto* state = static_cast<Impl*>(userContext);
        if (state == nullptr) return false;
        state->transferPending = false;
        state->lastCompleteUs =
            static_cast<std::uint64_t>(esp_timer_get_time());
        BaseType_t higherPriorityTaskWoken = pdFALSE;
        if (state->transferDone != nullptr) {
            (void)xSemaphoreGiveFromISR(state->transferDone,
                                        &higherPriorityTaskWoken);
        }
        if (state->observer != nullptr) {
            state->observer(state->observerContext);
        }
        if (higherPriorityTaskWoken == pdTRUE) {
            portYIELD_FROM_ISR();
        }
        return higherPriorityTaskWoken == pdTRUE;
    }

    [[nodiscard]] bool waitForTransfer(TickType_t timeoutTicks) noexcept {
        if (!transferPending) return !transferFaulted;
        if (transferDone == nullptr ||
            xSemaphoreTake(transferDone, timeoutTicks) != pdTRUE) {
            transferFaulted = true;
            return false;
        }
        return !transferFaulted;
    }
};

EspIdfDisplayTouchAdapter::EspIdfDisplayTouchAdapter(
    EspIdfDisplayTouchConfig config)
    : impl_(std::make_unique<Impl>(std::move(config))) {}

EspIdfDisplayTouchAdapter::~EspIdfDisplayTouchAdapter() = default;

bool EspIdfDisplayTouchAdapter::initialize() {
    auto& state = *impl_;
    if (state.initialized || state.config.width == 0U ||
        state.config.height == 0U || !validPin(state.config.sclkPin) ||
        !validPin(state.config.mosiPin) || !validPin(state.config.misoPin) ||
        !validPin(state.config.displayChipSelectPin) ||
        !validPin(state.config.touchChipSelectPin) ||
        !validPin(state.config.dataCommandPin) ||
        !validPin(state.config.backlightPin)) {
        return false;
    }

    gpio_config_t backlightConfig{};
    backlightConfig.pin_bit_mask = 1ULL << state.config.backlightPin;
    backlightConfig.mode = GPIO_MODE_OUTPUT;
    backlightConfig.pull_up_en = GPIO_PULLUP_DISABLE;
    backlightConfig.pull_down_en = GPIO_PULLDOWN_DISABLE;
    backlightConfig.intr_type = GPIO_INTR_DISABLE;
    if (gpio_config(&backlightConfig) != ESP_OK ||
        gpio_set_level(gpio(state.config.backlightPin),
                       state.config.backlightActiveHigh ? 0 : 1) != ESP_OK) {
        return false;
    }

    state.transferDone = xSemaphoreCreateBinary();
    if (state.transferDone == nullptr) return false;

    spi_bus_config_t busConfig{};
    busConfig.sclk_io_num = state.config.sclkPin;
    busConfig.mosi_io_num = state.config.mosiPin;
    busConfig.miso_io_num = state.config.misoPin;
    busConfig.quadwp_io_num = -1;
    busConfig.quadhd_io_num = -1;
    busConfig.max_transfer_sz =
        static_cast<int>(kPartialBufferPixels * sizeof(std::uint16_t));
    if (spi_bus_initialize(kSpiHost, &busConfig, SPI_DMA_CH_AUTO) != ESP_OK) {
        return false;
    }
    state.busInitialized = true;

    esp_lcd_panel_io_spi_config_t displayIoConfig{};
    displayIoConfig.cs_gpio_num = gpio(state.config.displayChipSelectPin);
    displayIoConfig.dc_gpio_num = gpio(state.config.dataCommandPin);
    displayIoConfig.spi_mode = 0;
    displayIoConfig.pclk_hz = 40U * 1000U * 1000U;
    displayIoConfig.trans_queue_depth = 10U;
    displayIoConfig.lcd_cmd_bits = 8;
    displayIoConfig.lcd_param_bits = 8;
    if (esp_lcd_new_panel_io_spi(kSpiHost, &displayIoConfig,
                                 &state.displayIo) != ESP_OK) {
        return false;
    }
    esp_lcd_panel_io_callbacks_t displayCallbacks{};
    displayCallbacks.on_color_trans_done = &Impl::onColorTransferDone;
    if (esp_lcd_panel_io_register_event_callbacks(
            state.displayIo, &displayCallbacks, &state) != ESP_OK) {
        return false;
    }

    esp_lcd_panel_dev_config_t panelConfig{};
    panelConfig.reset_gpio_num = GPIO_NUM_NC;
    panelConfig.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR;
    panelConfig.data_endian = LCD_RGB_DATA_ENDIAN_LITTLE;
    panelConfig.bits_per_pixel = 16U;
    if (esp_lcd_new_panel_ili9341(state.displayIo, &panelConfig,
                                  &state.panel) != ESP_OK ||
        esp_lcd_panel_reset(state.panel) != ESP_OK ||
        esp_lcd_panel_init(state.panel) != ESP_OK ||
        esp_lcd_panel_disp_on_off(state.panel, true) != ESP_OK) {
        return false;
    }

    esp_lcd_panel_io_spi_config_t touchIoConfig{};
    touchIoConfig.cs_gpio_num = gpio(state.config.touchChipSelectPin);
    touchIoConfig.dc_gpio_num = GPIO_NUM_NC;
    touchIoConfig.spi_mode = 0;
    touchIoConfig.pclk_hz = ESP_LCD_TOUCH_SPI_CLOCK_HZ;
    touchIoConfig.trans_queue_depth = 3U;
    touchIoConfig.lcd_cmd_bits = 8;
    touchIoConfig.lcd_param_bits = 8;
    if (esp_lcd_new_panel_io_spi(kSpiHost, &touchIoConfig, &state.touchIo) !=
        ESP_OK) {
        return false;
    }

    esp_lcd_touch_config_t touchConfig{};
    // The application port is explicitly controller-native. The XPT2046
    // component must be built with coordinate conversion disabled; these
    // limits therefore describe the raw 12-bit ADC domain only.
    touchConfig.x_max = 4095U;
    touchConfig.y_max = 4095U;
    touchConfig.rst_gpio_num = GPIO_NUM_NC;
    touchConfig.int_gpio_num =
        validPin(state.config.touchInterruptPin)
            ? gpio(state.config.touchInterruptPin)
            : GPIO_NUM_NC;
    touchConfig.levels.reset = 0U;
    touchConfig.levels.interrupt = 0U;
    if (esp_lcd_touch_new_spi_xpt2046(state.touchIo, &touchConfig,
                                      &state.touch) != ESP_OK) {
        return false;
    }

    state.dmaPixels = static_cast<std::uint16_t*>(heap_caps_malloc(
        kPartialBufferPixels * sizeof(std::uint16_t), MALLOC_CAP_DMA));
    if (state.dmaPixels == nullptr) {
        return false;
    }
    state.initialized = true;
    return setRotation(device_platform::DisplayRotation::Rotate0);
}

bool EspIdfDisplayTouchAdapter::setRotation(
    device_platform::DisplayRotation rotation) {
    auto& state = *impl_;
    if (!state.initialized || state.panel == nullptr) {
        return false;
    }

    bool swap = false;
    bool mirrorX = false;
    bool mirrorY = false;
    switch (rotation) {
        case device_platform::DisplayRotation::Rotate0:
            break;
        case device_platform::DisplayRotation::Rotate90:
            swap = true;
            mirrorX = true;
            break;
        case device_platform::DisplayRotation::Rotate180:
            mirrorX = true;
            mirrorY = true;
            break;
        case device_platform::DisplayRotation::Rotate270:
            swap = true;
            mirrorY = true;
            break;
    }

    // Rotation is a display concern. Raw touch samples must not be silently
    // calibrated or transformed at this adapter boundary.
    if (esp_lcd_panel_swap_xy(state.panel, swap) != ESP_OK ||
        esp_lcd_panel_mirror(state.panel, mirrorX, mirrorY) != ESP_OK) {
        return false;
    }
    state.rotation = rotation;
    return true;
}

bool EspIdfDisplayTouchAdapter::setBacklight(bool enabled) {
    const auto& config = impl_->config;
    if (!validPin(config.backlightPin)) {
        return false;
    }
    const int level = enabled == config.backlightActiveHigh ? 1 : 0;
    return gpio_set_level(gpio(config.backlightPin), level) == ESP_OK;
}

bool EspIdfDisplayTouchAdapter::fillRect(device_platform::DisplayRect rect,
                                         std::uint16_t rgb565) {
    auto& state = *impl_;
    if (!state.initialized || state.panel == nullptr || state.dmaPixels == nullptr ||
        rect.width == 0U || rect.height == 0U ||
        static_cast<std::uint32_t>(rect.left) + rect.width > state.config.width ||
        static_cast<std::uint32_t>(rect.top) + rect.height > state.config.height) {
        return false;
    }

    if (state.transferFaulted) return false;
    std::fill_n(state.dmaPixels, kPartialBufferPixels, rgb565);
    std::uint16_t remainingRows = rect.height;
    std::uint16_t row = rect.top;
    while (remainingRows > 0U) {
        const auto rows = static_cast<std::uint16_t>(std::min<std::size_t>(
            remainingRows, kPartialBufferPixels / rect.width));
        if (rows == 0U || state.transferPending) {
            return false;
        }
        state.transferPending = true;
        if (state.firstSubmitUs == 0U) {
            state.firstSubmitUs =
                static_cast<std::uint64_t>(esp_timer_get_time());
        }
        if (esp_lcd_panel_draw_bitmap(
                state.panel, rect.left, row, rect.left + rect.width,
                row + rows, state.dmaPixels) != ESP_OK) {
            state.transferPending = false;
            state.transferFaulted = true;
            return false;
        }
        if (!state.waitForTransfer(pdMS_TO_TICKS(1000U))) return false;
        row = static_cast<std::uint16_t>(row + rows);
        remainingRows = static_cast<std::uint16_t>(remainingRows - rows);
    }
    return true;
}

bool EspIdfDisplayTouchAdapter::flushRgb565(
    device_platform::DisplayRect rect, const std::uint16_t* pixels,
    std::size_t pixelCount) {
    auto& state = *impl_;
    const auto expectedPixels = static_cast<std::size_t>(rect.width) *
                                static_cast<std::size_t>(rect.height);
    if (!state.initialized || state.panel == nullptr || state.dmaPixels == nullptr ||
        pixels == nullptr || pixelCount != expectedPixels || rect.width == 0U ||
        rect.height == 0U ||
        static_cast<std::uint32_t>(rect.left) + rect.width > state.config.width ||
        static_cast<std::uint32_t>(rect.top) + rect.height > state.config.height ||
        state.transferFaulted) {
        return false;
    }

    std::size_t sourceOffset = 0U;
    std::uint16_t row = rect.top;
    while (row < static_cast<std::uint16_t>(rect.top + rect.height)) {
        const auto rows = static_cast<std::uint16_t>(std::min<std::size_t>(
            rect.height - (row - rect.top), kPartialBufferPixels / rect.width));
        if (rows == 0U || state.transferPending) return false;
        const auto chunkPixels = static_cast<std::size_t>(rows) * rect.width;
        std::memcpy(state.dmaPixels, pixels + sourceOffset,
                    chunkPixels * sizeof(std::uint16_t));
        state.transferPending = true;
        if (state.firstSubmitUs == 0U) {
            state.firstSubmitUs =
                static_cast<std::uint64_t>(esp_timer_get_time());
        }
        if (esp_lcd_panel_draw_bitmap(
                state.panel, rect.left, row, rect.left + rect.width,
                row + rows, state.dmaPixels) != ESP_OK) {
            state.transferPending = false;
            state.transferFaulted = true;
            return false;
        }
        if (!state.waitForTransfer(pdMS_TO_TICKS(1000U))) return false;
        sourceOffset += chunkPixels;
        row = static_cast<std::uint16_t>(row + rows);
    }
    return true;
}

device_platform::RawTouchSample EspIdfDisplayTouchAdapter::sampleTouch() {
    auto& state = *impl_;
    const auto now = static_cast<std::uint64_t>(esp_timer_get_time());
    if (!state.initialized || state.touch == nullptr ||
        esp_lcd_touch_read_data(state.touch) != ESP_OK) {
        return {device_platform::RawTouchSampleStatus::ControllerError,
                0U, 0U, 0U, now, false};
    }

    esp_lcd_touch_point_data_t point[1]{};
    std::uint8_t points = 0U;
    if (esp_lcd_touch_get_data(state.touch, point, &points, 1U) != ESP_OK) {
        return {device_platform::RawTouchSampleStatus::ControllerError,
                0U, 0U, 0U, now, false};
    }
    if (points == 0U) {
        return {device_platform::RawTouchSampleStatus::NoContact,
                0U, 0U, 0U, now, false};
    }
    return {device_platform::RawTouchSampleStatus::Contact,
            point[0].x, point[0].y, point[0].strength, now, true};
}

bool EspIdfDisplayTouchAdapter::setDisplayTransferObserver(
    DisplayTransferObserver observer, void* context) noexcept {
    auto& state = *impl_;
    if (!state.initialized || state.displayIo == nullptr || observer == nullptr) {
        return false;
    }
    state.observer = observer;
    state.observerContext = context;
    esp_lcd_panel_io_callbacks_t callbacks{};
    callbacks.on_color_trans_done = &Impl::onColorTransferDone;
    return esp_lcd_panel_io_register_event_callbacks(state.displayIo, &callbacks,
                                                     &state) == ESP_OK;
}

bool EspIdfDisplayTouchAdapter::waitForDisplayTransfer(
    std::uint32_t timeoutMs) noexcept {
    auto& state = *impl_;
    if (timeoutMs == UINT32_MAX) {
        return state.waitForTransfer(portMAX_DELAY);
    }
    const auto ticks = std::max<TickType_t>(1, pdMS_TO_TICKS(timeoutMs));
    return state.waitForTransfer(ticks);
}

void EspIdfDisplayTouchAdapter::resetFrameTransferMetrics() noexcept {
    auto& state = *impl_;
    state.firstSubmitUs = 0U;
    state.lastCompleteUs = 0U;
    state.transferFaulted = false;
}

std::uint64_t EspIdfDisplayTouchAdapter::firstFrameTransferSubmitUs() const noexcept {
    return impl_->firstSubmitUs;
}

std::uint64_t EspIdfDisplayTouchAdapter::lastFrameTransferCompleteUs() const noexcept {
    return impl_->lastCompleteUs;
}

bool EspIdfDisplayTouchAdapter::frameTransferCompleted() const noexcept {
    return impl_->firstSubmitUs != 0U && impl_->lastCompleteUs >= impl_->firstSubmitUs &&
           !impl_->transferPending && !impl_->transferFaulted;
}

namespace detail {

bool bindEspIdfDisplayTouchHandles(
    const EspIdfDisplayTouchAdapter& adapter,
    EspIdfDisplayTouchHandles& handles) noexcept {
    if (adapter.impl_ == nullptr || !adapter.impl_->initialized ||
        adapter.impl_->displayIo == nullptr || adapter.impl_->panel == nullptr ||
        adapter.impl_->touch == nullptr) {
        return false;
    }
    handles.displayIo = adapter.impl_->displayIo;
    handles.panel = adapter.impl_->panel;
    handles.touch = adapter.impl_->touch;
    return true;
}

}  // namespace detail

}  // namespace device_platform_esp_idf
