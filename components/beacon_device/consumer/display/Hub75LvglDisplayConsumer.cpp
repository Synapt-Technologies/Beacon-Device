#include "consumer/display/Hub75LvglDisplayConsumer.hpp"

#include "esp_lvgl_port.h"
#include "esp_log.h"

LV_FONT_DECLARE(helvatica_140);

namespace {
constexpr const char* TAG = "Hub75Display";
}

// ── Construction / destruction ───────────────────────────────────────

Hub75LvglDisplayConsumer::Hub75LvglDisplayConsumer(const Hub75Config& config,
                                                   const IDisplayConsumer::Zone* zones,
                                                   uint8_t zoneCount)
    : ILvglDisplayConsumer(zones, zoneCount, &helvatica_140, &lv_font_montserrat_28),
      _config(config),
      _driver(_config)
{
    finishInit(initHardware());
}

Hub75LvglDisplayConsumer::~Hub75LvglDisplayConsumer() {
    if (_disp) {
        if (lvgl_port_lock(portMAX_DELAY)) {
            lv_display_delete(_disp);
            lvgl_port_unlock();
        }
        _disp = nullptr;
    }
}

// ── Hardware init ────────────────────────────────────────────────────

lv_display_t* Hub75LvglDisplayConsumer::initHardware() {
    if (!_driver.begin()) {
        ESP_LOGE(TAG, "Failed to initialize HUB75 driver!");
        return nullptr;
    }

    ESP_LOGI(TAG, "HUB75 driver initialized: %ux%u pixels",
             _driver.get_width(), _driver.get_height());

    const uint16_t W = _driver.get_width();
    const uint16_t numRows = _driver.get_height() / 2;  // 16 for standard two-scan

    ESP_LOGI(TAG, "Row-scan test: sweeping green line through rows 0-%u (1s each)...", numRows - 1);
    ESP_LOGI(TAG, "  Watch the panel: line should move down each second.");
    ESP_LOGI(TAG, "  If same two rows stay lit every step -> address lines stuck at 0.");

    for (uint16_t row = 0; row < numRows; row++) {
        _driver.clear();
        _driver.fill(0, row,          W, 1, 0, 255, 0);   // green line in upper half
        _driver.fill(0, row + numRows, W, 1, 255, 0, 0);  // red line in lower half
        ESP_LOGI(TAG, "  row %u/%u", row, numRows - 1);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    _driver.clear();

    ensureLvglPortInited();

    lv_display_t* disp = lv_display_create(_driver.get_width(), _driver.get_height());
    if (!disp) {
        ESP_LOGE(TAG, "Failed to create LVGL display!");
        return nullptr;
    }

    lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565);

    // Full-frame buffer: Hub75 has its own hardware frame buffer, so rendering
    // the whole frame at once then flipping avoids tearing with double buffering.
    lv_draw_buf_t* draw_buf = lv_draw_buf_create(
        _driver.get_width(), _driver.get_height(), LV_COLOR_FORMAT_RGB565, 0);
    if (!draw_buf) {
        ESP_LOGE(TAG, "Failed to create LVGL draw buffer!");
        lv_display_delete(disp);
        return nullptr;
    }

    lv_display_set_draw_buffers(disp, draw_buf, nullptr);
    lv_display_set_render_mode(disp, LV_DISPLAY_RENDER_MODE_FULL);
    lv_display_set_user_data(disp, this);
    lv_display_set_flush_cb(disp, flushCb);

    return disp;
}

// ── Flush callback ───────────────────────────────────────────────────

void Hub75LvglDisplayConsumer::flushCb(lv_display_t* disp, const lv_area_t* area, uint8_t* px_map) {
    auto* self = static_cast<Hub75LvglDisplayConsumer*>(lv_display_get_user_data(disp));
    if (!self) {
        lv_display_flush_ready(disp);
        return;
    }

    const uint16_t x = static_cast<uint16_t>(area->x1);
    const uint16_t y = static_cast<uint16_t>(area->y1);
    const uint16_t w = static_cast<uint16_t>(area->x2 - area->x1 + 1);
    const uint16_t h = static_cast<uint16_t>(area->y2 - area->y1 + 1);
    self->_driver.draw_pixels(x, y, w, h, px_map, Hub75PixelFormat::RGB565);

    if (self->_config.double_buffer) {
        self->_driver.flip_buffer();
    }

    lv_display_flush_ready(disp);
}
