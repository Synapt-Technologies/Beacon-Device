#include "consumer/CYDDisplayConsumer.hpp"

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_ili9341.h"
#include "esp_lvgl_port.h"
#include "esp_log.h"

namespace {
constexpr const char* TAG = "CYDDisplay";
constexpr int LCD_PIXEL_CLOCK_HZ    = 80 * 1000 * 1000;
constexpr int LCD_CMD_BITS          = 8;
constexpr int LCD_PARAM_BITS        = 8;
constexpr int LCD_DRAW_BUFFER_LINES = 80;
}

// ── Construction / destruction ───────────────────────────────────────

CYDDisplayConsumer::CYDDisplayConsumer(const IDisplayConsumer::Zone* zones, uint8_t zoneCount)
    : _displayZones(zones), _zoneCount(zoneCount) {
    _zoneObjs = new lv_obj_t*[_zoneCount]();
    rebuildLut();
    initLvgl();
    buildUi();
}

CYDDisplayConsumer::~CYDDisplayConsumer() {
    if (_disp)        { lvgl_port_remove_disp(_disp); _disp = nullptr; }
    if (_panelHandle) { esp_lcd_panel_del(_panelHandle); }
    if (_ioHandle)    { esp_lcd_panel_io_del(_ioHandle); }
    spi_bus_free(CYD_SPI_HOST);
    delete[] _zoneObjs;
}

// ── Hardware / LVGL init ─────────────────────────────────────────────

void CYDDisplayConsumer::initLvgl() {
    gpio_config_t bl = {
        .pin_bit_mask = 1ULL << CYD_PIN_BL,
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&bl));
    ESP_ERROR_CHECK(gpio_set_level((gpio_num_t)CYD_PIN_BL, 1));

    spi_bus_config_t bus = {};
    bus.mosi_io_num     = CYD_PIN_MOSI;
    bus.miso_io_num     = -1;
    bus.sclk_io_num     = CYD_PIN_SCLK;
    bus.quadwp_io_num   = -1;
    bus.quadhd_io_num   = -1;
    bus.max_transfer_sz = CYD_LCD_W * LCD_DRAW_BUFFER_LINES * sizeof(uint16_t) * 2;
    ESP_ERROR_CHECK(spi_bus_initialize(CYD_SPI_HOST, &bus, SPI_DMA_CH_AUTO));

    esp_lcd_panel_io_spi_config_t io = {};
    io.cs_gpio_num       = (gpio_num_t)CYD_PIN_CS;
    io.dc_gpio_num       = (gpio_num_t)CYD_PIN_DC;
    io.spi_mode          = 0;
    io.pclk_hz           = LCD_PIXEL_CLOCK_HZ;
    io.trans_queue_depth = 10;
    io.lcd_cmd_bits      = LCD_CMD_BITS;
    io.lcd_param_bits    = LCD_PARAM_BITS;
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(
        (esp_lcd_spi_bus_handle_t)CYD_SPI_HOST, &io, &_ioHandle));

    esp_lcd_panel_dev_config_t panel = {};
    panel.reset_gpio_num = (gpio_num_t)CYD_PIN_RST;
    panel.rgb_ele_order  = LCD_RGB_ELEMENT_ORDER_RGB;
    panel.bits_per_pixel = 16;
    ESP_ERROR_CHECK(esp_lcd_new_panel_ili9341(_ioHandle, &panel, &_panelHandle));

    ESP_ERROR_CHECK(esp_lcd_panel_reset(_panelHandle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(_panelHandle));
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(_panelHandle, false));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(_panelHandle, true));
    // swap_xy / mirror are owned by esp_lvgl_port via disp_cfg.rotation

    static bool s_lvgl_inited = false; // singleton: lvgl_port is a global resource
    if (!s_lvgl_inited) {
        lvgl_port_cfg_t port = ESP_LVGL_PORT_INIT_CONFIG();
        port.task_priority   = 4;
        port.task_stack      = 8 * 1024;
        port.timer_period_ms = 5;
        ESP_ERROR_CHECK(lvgl_port_init(&port));
        s_lvgl_inited = true;
    }

    lvgl_port_display_cfg_t disp = {};
    disp.io_handle         = _ioHandle;
    disp.panel_handle      = _panelHandle;
    disp.buffer_size       = CYD_LCD_W * LCD_DRAW_BUFFER_LINES;
    disp.double_buffer     = true;
    disp.hres              = CYD_LCD_W;
    disp.vres              = CYD_LCD_H;
    disp.monochrome        = false;
    disp.rotation.swap_xy  = false;
    disp.rotation.mirror_x = true;
    disp.rotation.mirror_y = false;
    disp.flags.buff_dma    = true;
    disp.flags.swap_bytes  = true;
    _disp = lvgl_port_add_disp(&disp);
    if (!_disp) ESP_LOGE(TAG, "lvgl_port_add_disp failed");
}

void CYDDisplayConsumer::buildUi() {
    if (!lvgl_port_lock(portMAX_DELAY)) return;

    lv_obj_t* scr = lv_display_get_screen_active(_disp);

    for (uint8_t i = 0; i < _zoneCount; i++) {
        const IDisplayConsumer::Zone& z = _displayZones[i];
        _zoneObjs[i] = lv_obj_create(scr);
        lv_obj_remove_style_all(_zoneObjs[i]);
        lv_obj_set_size(_zoneObjs[i], z.w, z.h);
        lv_obj_set_pos(_zoneObjs[i],  z.x, z.y);
        lv_obj_set_style_bg_opa(_zoneObjs[i],
            z.stateColored ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
        lv_obj_remove_flag(_zoneObjs[i], LV_OBJ_FLAG_SCROLLABLE);
    }

    _labels[0] = lv_label_create(scr);
    lv_obj_set_style_text_font(_labels[0], &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(_labels[0], lv_color_white(), 0);
    lv_label_set_text(_labels[0], "");
    lv_obj_align(_labels[0], LV_ALIGN_CENTER, 0, -16);

    _labels[1] = lv_label_create(scr);
    lv_obj_set_style_text_font(_labels[1], &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(_labels[1], lv_color_white(), 0);
    lv_label_set_text(_labels[1], "");
    lv_obj_align(_labels[1], LV_ALIGN_CENTER, 0, 20);

    lvgl_port_unlock();
}

// ── IConsumer overrides ──────────────────────────────────────────────

void CYDDisplayConsumer::setColor(uint8_t r, uint8_t g, uint8_t b) {
    if (!_zoneObjs || !lvgl_port_lock(portMAX_DELAY)) return;

    const uint8_t sr = scale_brightness(r);
    const uint8_t sg = scale_brightness(g);
    const uint8_t sb = scale_brightness(b);
    const lv_color_t col = lv_color_make(sr, sg, sb);

    for (uint8_t i = 0; i < _zoneCount; i++) {
        const IDisplayConsumer::Zone& z = _displayZones[i];
        if (_state < z.minState) {
            lv_obj_set_style_bg_opa(_zoneObjs[i], LV_OPA_TRANSP, 0);
        } else if (z.stateColored) {
            lv_obj_set_style_bg_color(_zoneObjs[i], col, 0);
            lv_obj_set_style_bg_opa(_zoneObjs[i], LV_OPA_COVER, 0);
        } else if (!_alertTask) {
            lv_obj_set_style_bg_opa(_zoneObjs[i], LV_OPA_TRANSP, 0);
        }
    }
    lv_obj_set_style_text_color(_labels[0], contrastTextColor(sr, sg, sb, 255), 0);
    lv_obj_set_style_text_color(_labels[1], contrastTextColor(sr, sg, sb, 100), 0);

    if (!isRevertPending(0)) applySlot(0);

    lvgl_port_unlock();
}

void CYDDisplayConsumer::setAlertStep(DeviceAlertAction action,
                                       DeviceAlertTarget target, uint8_t step) {
    const AlertPatternConfig* cfg = getAlertPattern(action);
    if (!cfg || !_zoneObjs || !lvgl_port_lock(portMAX_DELAY)) return;

    for (uint8_t i = 0; i < _zoneCount; i++) {
        const IDisplayConsumer::Zone& z = _displayZones[i];

        if (z.alertTarget != DeviceAlertTarget::ALL
            && target != DeviceAlertTarget::ALL
            && z.alertTarget != target) continue;

        uint8_t v = z.alertVariant % cfg->variantCount;
        TallyState s = cfg->patterns[v][step % cfg->patternLen];

        if (s == TallyState::NONE) {
            if (z.stateColored) {
                uint8_t r, g, b;
                stateToColor(_state, r, g, b);
                lv_obj_set_style_bg_color(_zoneObjs[i],
                    lv_color_make(scale_brightness(r),
                                  scale_brightness(g),
                                  scale_brightness(b)), 0);
                lv_obj_set_style_bg_opa(_zoneObjs[i], LV_OPA_COVER, 0);
            } else {
                lv_obj_set_style_bg_opa(_zoneObjs[i], LV_OPA_TRANSP, 0);
            }
        } else {
            uint8_t r, g, b;
            stateToColor(s, r, g, b);
            lv_obj_set_style_bg_color(_zoneObjs[i],
                lv_color_make(scale_brightness(r),
                              scale_brightness(g),
                              scale_brightness(b)), 0);
            lv_obj_set_style_bg_opa(_zoneObjs[i], LV_OPA_COVER, 0);
        }
    }

    lvgl_port_unlock();
}

uint32_t CYDDisplayConsumer::getAlertStepLength(DeviceAlertAction action) {
    return getAlertPattern(action)->speedMs;
}

uint8_t CYDDisplayConsumer::getAlertStepCount(DeviceAlertAction action) {
    return getAlertPattern(action)->patternLen;
}

// ── IDisplayConsumer override ────────────────────────────────────────

void CYDDisplayConsumer::onTextChanged(uint8_t index, const char* text) {
    if (index >= 2 || !_labels[index]) return;
    if (!lvgl_port_lock(portMAX_DELAY)) return;
    lv_label_set_text(_labels[index], text);
    lvgl_port_unlock();
}

// ── Private helpers ──────────────────────────────────────────────────

void CYDDisplayConsumer::applySlot(uint8_t index) {
    lv_label_set_text(_labels[index], getBaseText(index));
}

lv_color_t CYDDisplayConsumer::contrastTextColor(uint8_t r, uint8_t g, uint8_t b, uint8_t brightness) {
    uint16_t y = (r * 299u + g * 587u + b * 114u) / 1000u;
    return (y > 140) ? lv_color_make(0, 0, 0) : lv_color_make(brightness, brightness, brightness);
}

// ── Alert pattern table ──────────────────────────────────────────────

const CYDDisplayConsumer::AlertPatternConfig*
CYDDisplayConsumer::getAlertPattern(DeviceAlertAction action) {

    static const TallyState IDENT[][5]  = {
        { TallyState::NONE,    TallyState::NONE,    TallyState::NONE,    TallyState::NONE },
        { TallyState::PREVIEW, TallyState::PROGRAM, TallyState::PREVIEW, TallyState::PROGRAM },
        { TallyState::PROGRAM, TallyState::PREVIEW, TallyState::PROGRAM, TallyState::PREVIEW },
    };
    static const TallyState INFO[][5]   = {
        { TallyState::NONE, TallyState::NONE, TallyState::NONE, TallyState::NONE },
        { TallyState::INFO, TallyState::NONE, TallyState::INFO, TallyState::NONE },
        { TallyState::INFO, TallyState::NONE, TallyState::INFO, TallyState::NONE },
    };
    static const TallyState NORMAL[][5] = {
        { TallyState::NONE,    TallyState::NONE,    TallyState::NONE,    TallyState::NONE },
        { TallyState::WARNING, TallyState::NONE,    TallyState::WARNING, TallyState::NONE },
        { TallyState::WARNING, TallyState::NONE,    TallyState::WARNING, TallyState::NONE },
    };
    static const TallyState PRIO[][5]   = {
        { TallyState::NONE,    TallyState::NONE,    TallyState::NONE,    TallyState::NONE },
        { TallyState::PROGRAM, TallyState::WARNING, TallyState::PROGRAM, TallyState::WARNING },
        { TallyState::WARNING, TallyState::PROGRAM, TallyState::WARNING, TallyState::PROGRAM },
    };

    static const AlertPatternConfig PATTERNS[] = {
        { 400, IDENT,  4, 3 },
        { 300, INFO,   4, 3 },
        { 400, NORMAL, 4, 3 },
        { 150, PRIO,   4, 3 },
    };

    switch (action) {
        case DeviceAlertAction::IDENT:  return &PATTERNS[0];
        case DeviceAlertAction::INFO:   return &PATTERNS[1];
        case DeviceAlertAction::NORMAL: return &PATTERNS[2];
        case DeviceAlertAction::PRIO:   return &PATTERNS[3];
        default:                        return nullptr;
    }
}
