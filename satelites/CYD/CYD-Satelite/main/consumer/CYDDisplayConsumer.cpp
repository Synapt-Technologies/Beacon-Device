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

constexpr int LCD_PIXEL_CLOCK_HZ      = 40 * 1000 * 1000;
constexpr int LCD_CMD_BITS            = 8;
constexpr int LCD_PARAM_BITS          = 8;
constexpr int LCD_DRAW_BUFFER_LINES   = 40; // double-buffered, 320*40*2 = 25 KB each
}


CYDDisplayConsumer::CYDDisplayConsumer() {
    rebuildLut();
    initLvgl();
    buildUi();
}

CYDDisplayConsumer::~CYDDisplayConsumer() {
    if (_disp) {
        lvgl_port_remove_disp(_disp);
        _disp = nullptr;
    }
    if (_panelHandle) esp_lcd_panel_del(_panelHandle);
    if (_ioHandle)    esp_lcd_panel_io_del(_ioHandle);
    spi_bus_free(CYD_SPI_HOST);
}

// ── LVGL / display init ──────────────────────────────────────────────

void CYDDisplayConsumer::initLvgl() {
    // Backlight on (active high on CYD).
    gpio_config_t bl = {
        .pin_bit_mask = 1ULL << CYD_PIN_BL,
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&bl));
    ESP_ERROR_CHECK(gpio_set_level(static_cast<gpio_num_t>(CYD_PIN_BL), 1));

    // SPI bus.
    spi_bus_config_t buscfg = {};
    buscfg.mosi_io_num     = CYD_PIN_MOSI;
    buscfg.miso_io_num     = -1;
    buscfg.sclk_io_num     = CYD_PIN_SCLK;
    buscfg.quadwp_io_num   = -1;
    buscfg.quadhd_io_num   = -1;
    buscfg.max_transfer_sz = CYD_LCD_W * LCD_DRAW_BUFFER_LINES * sizeof(uint16_t);
    ESP_ERROR_CHECK(spi_bus_initialize(CYD_SPI_HOST, &buscfg, SPI_DMA_CH_AUTO));

    // Panel IO over SPI.
    esp_lcd_panel_io_spi_config_t io_cfg = {};
    io_cfg.cs_gpio_num         = (gpio_num_t)CYD_PIN_CS;
    io_cfg.dc_gpio_num         = (gpio_num_t)CYD_PIN_DC;
    io_cfg.spi_mode            = 0;
    io_cfg.pclk_hz             = LCD_PIXEL_CLOCK_HZ;
    io_cfg.trans_queue_depth   = 10;
    io_cfg.lcd_cmd_bits        = LCD_CMD_BITS;
    io_cfg.lcd_param_bits      = LCD_PARAM_BITS;
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(
        (esp_lcd_spi_bus_handle_t)CYD_SPI_HOST, &io_cfg, &_ioHandle));

    // ILI9341 panel.
    esp_lcd_panel_dev_config_t panel_cfg = {};
    panel_cfg.reset_gpio_num = (gpio_num_t)CYD_PIN_RST;
    panel_cfg.rgb_ele_order  = LCD_RGB_ELEMENT_ORDER_BGR; // CYD is BGR; flip if colors swap
    panel_cfg.bits_per_pixel = 16;
    ESP_ERROR_CHECK(esp_lcd_new_panel_ili9341(_ioHandle, &panel_cfg, &_panelHandle));

    ESP_ERROR_CHECK(esp_lcd_panel_reset(_panelHandle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(_panelHandle));
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(_panelHandle, false));
    ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(_panelHandle, true));
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(_panelHandle, true, false));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(_panelHandle, true));

    // LVGL port (owns the LVGL task + tick).
    static bool s_lvgl_inited = false;
    if (!s_lvgl_inited) {
        lvgl_port_cfg_t port_cfg = ESP_LVGL_PORT_INIT_CONFIG();
        port_cfg.task_priority   = 4;
        port_cfg.task_stack      = 6 * 1024;
        port_cfg.timer_period_ms = 5;
        ESP_ERROR_CHECK(lvgl_port_init(&port_cfg));
        s_lvgl_inited = true;
    }

    lvgl_port_display_cfg_t disp_cfg = {};
    disp_cfg.io_handle      = _ioHandle;
    disp_cfg.panel_handle   = _panelHandle;
    disp_cfg.buffer_size    = CYD_LCD_W * LCD_DRAW_BUFFER_LINES;
    disp_cfg.double_buffer  = true;
    disp_cfg.hres           = CYD_LCD_W;
    disp_cfg.vres           = CYD_LCD_H;
    disp_cfg.monochrome     = false;
    disp_cfg.rotation.swap_xy  = true;
    disp_cfg.rotation.mirror_x = true;
    disp_cfg.rotation.mirror_y = false;
    disp_cfg.flags.swap_bytes  = true;
    _disp = lvgl_port_add_disp(&disp_cfg);
    if (!_disp) {
        ESP_LOGE(TAG, "lvgl_port_add_disp failed");
    }
}

void CYDDisplayConsumer::buildUi() {
    if (lvgl_port_lock(0) == false) return;

    lv_obj_t* scr = lv_display_get_screen_active(_disp);

    _bg = lv_obj_create(scr);
    lv_obj_remove_style_all(_bg);
    lv_obj_set_size(_bg, CYD_LCD_W, CYD_LCD_H);
    lv_obj_set_pos(_bg, 0, 0);
    lv_obj_set_style_bg_color(_bg, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(_bg, LV_OPA_COVER, 0);
    lv_obj_clear_flag(_bg, LV_OBJ_FLAG_SCROLLABLE);

    _borderL = lv_obj_create(scr);
    lv_obj_remove_style_all(_borderL);
    lv_obj_set_size(_borderL, BORDER_THICK, CYD_LCD_H);
    lv_obj_set_pos(_borderL, 0, 0);
    lv_obj_set_style_bg_opa(_borderL, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(_borderL, LV_OBJ_FLAG_SCROLLABLE);

    _borderR = lv_obj_create(scr);
    lv_obj_remove_style_all(_borderR);
    lv_obj_set_size(_borderR, BORDER_THICK, CYD_LCD_H);
    lv_obj_set_pos(_borderR, CYD_LCD_W - BORDER_THICK, 0);
    lv_obj_set_style_bg_opa(_borderR, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(_borderR, LV_OBJ_FLAG_SCROLLABLE);

    _title = lv_label_create(scr);
    lv_obj_set_style_text_font(_title, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(_title, lv_color_white(), 0);
    lv_label_set_text(_title, "");
    lv_obj_align(_title, LV_ALIGN_CENTER, 0, -20);

    _subtext = lv_label_create(scr);
    lv_obj_set_style_text_font(_subtext, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(_subtext, lv_color_white(), 0);
    lv_label_set_text(_subtext, "");
    lv_obj_align(_subtext, LV_ALIGN_CENTER, 0, 24);

    lvgl_port_unlock();
}

// ── IConsumer overrides ──────────────────────────────────────────────

void CYDDisplayConsumer::setColor(uint8_t r, uint8_t g, uint8_t b) {
    if (!_bg) return;
    if (lvgl_port_lock(0) == false) return;

    const uint8_t sr = scale_brightness(r);
    const uint8_t sg = scale_brightness(g);
    const uint8_t sb = scale_brightness(b);

    lv_obj_set_style_bg_color(_bg, lv_color_make(sr, sg, sb), 0);
    lv_obj_set_style_bg_opa(_bg, LV_OPA_COVER, 0);

    lv_color_t txt = contrastTextColor(sr, sg, sb);
    lv_obj_set_style_text_color(_title, txt, 0);
    lv_obj_set_style_text_color(_subtext, txt, 0);

    // Borders cleared whenever no alert is active (this method is called
    // by applyState() after an alert ends).
    if (_alertTask == nullptr) {
        lv_obj_set_style_bg_opa(_borderL, LV_OPA_TRANSP, 0);
        lv_obj_set_style_bg_opa(_borderR, LV_OPA_TRANSP, 0);
    }

    if (!_titleOverridden) applyTitleForState(_state);

    lvgl_port_unlock();
}

void CYDDisplayConsumer::setText(const char* text, uint8_t index, uint32_t timeout) {
    if (index >= 2) return;
    if (lvgl_port_lock(0) == false) return;

    lv_obj_t*    label  = (index == 0) ? _title         : _subtext;
    lv_timer_t** revert = (index == 0) ? &_titleRevert  : &_subtextRevert;
    bool*        overridden = (index == 0) ? &_titleOverridden : &_subtextOverridden;

    if (*revert) {
        lv_timer_del(*revert);
        *revert = nullptr;
    }

    lv_label_set_text(label, text ? text : "");
    *overridden = (text != nullptr && text[0] != '\0');

    if (timeout > 0 && *overridden) {
        *revert = lv_timer_create(
            (index == 0) ? revertTitleCb : revertSubtextCb,
            timeout,
            this);
        lv_timer_set_repeat_count(*revert, 1);
    }

    lvgl_port_unlock();
}

void CYDDisplayConsumer::revertTitleCb(lv_timer_t* t) {
    auto* self = static_cast<CYDDisplayConsumer*>(lv_timer_get_user_data(t));
    self->_titleOverridden = false;
    self->applyTitleForState(self->_state);
    self->_titleRevert = nullptr;
}

void CYDDisplayConsumer::revertSubtextCb(lv_timer_t* t) {
    auto* self = static_cast<CYDDisplayConsumer*>(lv_timer_get_user_data(t));
    self->_subtextOverridden = false;
    lv_label_set_text(self->_subtext, "");
    self->_subtextRevert = nullptr;
}

void CYDDisplayConsumer::applyTitleForState(TallyState s) {
    if (_title) lv_label_set_text(_title, stateName(s));
}

// ── Alerts ───────────────────────────────────────────────────────────

void CYDDisplayConsumer::setAlertStep(DeviceAlertAction action, DeviceAlertTarget target, uint8_t step) {
    const AlertPatternConfig* cfg = getAlertPattern(action);
    if (!cfg) return;
    if (lvgl_port_lock(0) == false) return;

    lv_obj_t* bars[ZONE_COUNT] = { _borderL, _borderR };

    for (uint8_t z = 0; z < ZONE_COUNT; z++) {
        const BorderZone& zone = ZONES[z];
        if (zone.target != DeviceAlertTarget::ALL
            && target != DeviceAlertTarget::ALL
            && zone.target != target) continue;

        uint8_t v = zone.patternVariant % cfg->variantCount;
        TallyState s = cfg->patterns[v][step % cfg->patternLen];

        if (s == TallyState::NONE) {
            lv_obj_set_style_bg_opa(bars[z], LV_OPA_TRANSP, 0);
        } else {
            uint8_t r, g, b;
            stateToColor(s, r, g, b);
            lv_obj_set_style_bg_color(bars[z],
                lv_color_make(scale_brightness(r), scale_brightness(g), scale_brightness(b)), 0);
            lv_obj_set_style_bg_opa(bars[z], LV_OPA_COVER, 0);
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

const CYDDisplayConsumer::AlertPatternConfig* CYDDisplayConsumer::getAlertPattern(DeviceAlertAction action) {

    static const TallyState IDENT[][5]  = {
        { TallyState::NONE,    TallyState::NONE,    TallyState::NONE,    TallyState::NONE },
        { TallyState::PREVIEW, TallyState::PROGRAM, TallyState::PREVIEW, TallyState::PROGRAM },
        { TallyState::PROGRAM, TallyState::PREVIEW, TallyState::PROGRAM, TallyState::PREVIEW },
    };
    static const TallyState INFO[][5]   = {
        { TallyState::NONE, TallyState::NONE, TallyState::NONE, TallyState::NONE },
        { TallyState::INFO, TallyState::NONE, TallyState::INFO, TallyState::NONE },
        { TallyState::NONE, TallyState::INFO, TallyState::NONE, TallyState::INFO },
    };
    static const TallyState NORMAL[][5] = {
        { TallyState::NONE,    TallyState::NONE,    TallyState::NONE,    TallyState::NONE },
        { TallyState::WARNING, TallyState::NONE,    TallyState::WARNING, TallyState::NONE },
        { TallyState::NONE,    TallyState::WARNING, TallyState::NONE,    TallyState::WARNING },
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

// ── Helpers ──────────────────────────────────────────────────────────

const char* CYDDisplayConsumer::stateName(TallyState s) {
    switch (s) {
        case TallyState::PROGRAM: return "PROGRAM";
        case TallyState::PREVIEW: return "PREVIEW";
        case TallyState::INFO:    return "INFO";
        case TallyState::WARNING: return "WARNING";
        case TallyState::DANGER:  return "DANGER";
        default:                  return "";
    }
}

lv_color_t CYDDisplayConsumer::contrastTextColor(uint8_t r, uint8_t g, uint8_t b) {
    uint16_t y = static_cast<uint16_t>(r * 299 + g * 587 + b * 114) / 1000;
    return (y > 140) ? lv_color_black() : lv_color_white();
}
