#include "fermentation_ui_renderer.hpp"

#include "esp_lvgl_port.h"
#include "lvgl.h"

#include "esp_idf_display_touch_adapter_private.hpp"
#include "esp_idf_display_touch_adapter.hpp"

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

    lvgl_port_touch_cfg_t touchConfig{};
    touchConfig.disp = display;
    touchConfig.handle = handles.touch;
    touchConfig.scale.x = 1.0F;
    touchConfig.scale.y = 1.0F;
    if (lvgl_port_add_touch(&touchConfig) == nullptr || !lvgl_port_lock(1000U)) {
        (void)lvgl_port_remove_disp(display);
        (void)lvgl_port_deinit();
        return {};
    }

    LvglRenderSummary result;
    result.drawCommands = screen.commands.size();
    result.partialBufferPixels = displayConfig.buffer_size;
    result.taskStackBytes = static_cast<std::size_t>(portConfig.task_stack);
    lv_obj_t* root = lv_screen_active();
    lv_obj_set_style_bg_color(root, color565ToLv(0x0000U), 0U);
    for (const auto& command : screen.commands) {
        if (command.kind == ScreenDrawKind::Text) {
            ++result.textCommands;
            result.textBytes += command.text.size();
            lv_obj_t* label = lv_label_create(root);
            lv_label_set_text(label, command.text.c_str());
            lv_obj_set_pos(label, command.rect.left, command.rect.top);
            lv_obj_set_size(label, command.rect.width, command.rect.height);
            lv_obj_set_style_text_color(label, color565ToLv(command.color565),
                                        0U);
        } else {
            lv_obj_t* fill = lv_obj_create(root);
            lv_obj_set_pos(fill, command.rect.left, command.rect.top);
            lv_obj_set_size(fill, command.rect.width, command.rect.height);
            lv_obj_set_style_bg_color(fill, color565ToLv(command.color565), 0U);
            lv_obj_set_style_border_width(fill, 0U, 0U);
            lv_obj_set_style_radius(fill, 0U, 0U);
        }
    }
    lvgl_port_unlock();
    result.success = true;
    return result;
}

}  // namespace fermentation::main_ui
