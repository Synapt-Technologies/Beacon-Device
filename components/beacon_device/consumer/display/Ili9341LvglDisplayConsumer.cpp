#include "consumer/display/Ili9341LvglDisplayConsumer.hpp"

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_ili9341.h"
#include "esp_lvgl_port.h"
#include "esp_log.h"

LV_FONT_DECLARE(helvatica_140);

namespace {
constexpr const char* TAG               = "Ili9341";
constexpr int LCD_PIXEL_CLOCK_HZ        = 80 * 1000 * 1000;
constexpr int LCD_CMD_BITS              = 8;
constexpr int LCD_PARAM_BITS            = 8;
constexpr int LCD_DRAW_BUFFER_LINES     = 80;
}

// ── Construction / destruction ───────────────────────────────────────

Ili9341LvglDisplayConsumer::Ili9341LvglDisplayConsumer(const IDisplayConsumer::Zone* zones,
                                                       uint8_t zoneCount)
    : ILvglDisplayConsumer(zones, zoneCount, &helvatica_140, &lv_font_montserrat_28)
{
    finishInit(initHardware());
}

Ili9341LvglDisplayConsumer::~Ili9341LvglDisplayConsumer() {
    // Remove display from LVGL before releasing the hardware it references.
    if (_disp) { lvgl_port_remove_disp(_disp); _disp = nullptr; }
    if (_panelHandle) { esp_lcd_panel_del(_panelHandle); }
    if (_ioHandle)    { esp_lcd_panel_io_del(_ioHandle); }
    spi_bus_free(SPI_HOST);
}

// ── Hardware init ────────────────────────────────────────────────────

lv_display_t* Ili9341LvglDisplayConsumer::initHardware() {
    gpio_config_t bl = {
        .pin_bit_mask = 1ULL << PIN_BL,
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&bl));
    ESP_ERROR_CHECK(gpio_set_level((gpio_num_t)PIN_BL, 1));

    spi_bus_config_t bus = {};
    bus.mosi_io_num     = PIN_MOSI;
    bus.miso_io_num     = -1;
    bus.sclk_io_num     = PIN_SCLK;
    bus.quadwp_io_num   = -1;
    bus.quadhd_io_num   = -1;
    bus.max_transfer_sz = LCD_W * LCD_DRAW_BUFFER_LINES * sizeof(uint16_t) * 2;
    ESP_ERROR_CHECK(spi_bus_initialize(SPI_HOST, &bus, SPI_DMA_CH_AUTO));

    esp_lcd_panel_io_spi_config_t io = {};
    io.cs_gpio_num       = (gpio_num_t)PIN_CS;
    io.dc_gpio_num       = (gpio_num_t)PIN_DC;
    io.spi_mode          = 0;
    io.pclk_hz           = LCD_PIXEL_CLOCK_HZ;
    io.trans_queue_depth = 10;
    io.lcd_cmd_bits      = LCD_CMD_BITS;
    io.lcd_param_bits    = LCD_PARAM_BITS;
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(
        (esp_lcd_spi_bus_handle_t)SPI_HOST, &io, &_ioHandle));

    esp_lcd_panel_dev_config_t panel = {};
    panel.reset_gpio_num = (gpio_num_t)PIN_RST;
    panel.rgb_ele_order  = LCD_RGB_ELEMENT_ORDER_RGB;
    panel.bits_per_pixel = 16;
    ESP_ERROR_CHECK(esp_lcd_new_panel_ili9341(_ioHandle, &panel, &_panelHandle));

    ESP_ERROR_CHECK(esp_lcd_panel_reset(_panelHandle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(_panelHandle));
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(_panelHandle, false));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(_panelHandle, true));

    ensureLvglPortInited();

    lvgl_port_display_cfg_t disp = {};
    disp.io_handle         = _ioHandle;
    disp.panel_handle      = _panelHandle;
    disp.buffer_size       = LCD_W * LCD_DRAW_BUFFER_LINES;
    disp.double_buffer     = true;
    disp.hres              = LCD_W;
    disp.vres              = LCD_H;
    disp.monochrome        = false;
    disp.rotation.swap_xy  = false;
    disp.rotation.mirror_x = true;
    disp.rotation.mirror_y = false;
    disp.flags.buff_dma    = true;
    disp.flags.swap_bytes  = true;

    lv_display_t* d = lvgl_port_add_disp(&disp);
    if (!d) ESP_LOGE(TAG, "lvgl_port_add_disp failed");
    return d;
}
