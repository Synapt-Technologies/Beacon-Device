#include "consumer/display/ILvglDisplayConsumer.hpp"

#include "esp_lvgl_port.h"
#include "esp_log.h"

namespace {
constexpr const char* TAG = "LvglDisplay";
}

static bool s_lvgl_inited = false;

// ── Construction / destruction ───────────────────────────────────────

ILvglDisplayConsumer::ILvglDisplayConsumer(const IDisplayConsumer::Zone* zones, uint8_t zoneCount,
                                           const TextConfig* textConfigs, uint8_t textCount)
    : _displayZones(zones), _zoneCount(zoneCount),
      _textConfigs(textConfigs), _textCount(textCount)
{
    _labels       = new lv_obj_t*[_textCount]();
    _strokeLabels = new lv_obj_t*[_textCount * 4]();
    _zoneObjs     = new lv_obj_t*[_zoneCount]();
}

ILvglDisplayConsumer::~ILvglDisplayConsumer() {
    // _disp must already be removed by the derived destructor (to preserve hardware order).
    delete[] _labels;
    delete[] _strokeLabels;
    delete[] _zoneObjs;
}

// ── Public init ──────────────────────────────────────────────────────

void ILvglDisplayConsumer::init() {
    _disp = initHardware();
    buildUi();
}

// ── Protected helpers ────────────────────────────────────────────────

void ILvglDisplayConsumer::ensureLvglPortInited() {
    if (s_lvgl_inited) return;
    lvgl_port_cfg_t port = ESP_LVGL_PORT_INIT_CONFIG();
    port.task_priority   = 14;
    port.task_stack      = 8 * 1024;
    port.timer_period_ms = 5;
    ESP_ERROR_CHECK(lvgl_port_init(&port));
    s_lvgl_inited = true;
}

// ── UI construction ──────────────────────────────────────────────────

void ILvglDisplayConsumer::buildUi() {
    if (!lvgl_port_lock(portMAX_DELAY)) return;

    if (!_disp) {
        ESP_LOGE(TAG, "Skipping UI build because display initialization failed");
        lvgl_port_unlock();
        return;
    }
    lv_obj_t* scr = lv_display_get_screen_active(_disp);
    if (!scr) {
        ESP_LOGE(TAG, "Skipping UI build because active screen is unavailable");
        lvgl_port_unlock();
        return;
    }

    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

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

    for (uint8_t i = 0; i < _textCount; i++) {
        const TextConfig& cfg = _textConfigs[i];
        if (cfg.strokeWidth == 0) continue;
        const int32_t sw    = cfg.strokeWidth;
        const int32_t dx[4] = { -sw,  sw,   0,   0 };
        const int32_t dy[4] = {   0,   0, -sw,  sw };
        for (uint8_t j = 0; j < 4; j++) {
            lv_obj_t* sl = lv_label_create(scr);
            lv_obj_set_style_text_font(sl, cfg.font, 0);
            lv_obj_set_style_text_color(sl, lv_color_black(), 0);
            lv_label_set_text(sl, "");
            lv_obj_align(sl, cfg.align, cfg.x_ofs + dx[j], cfg.y_ofs + dy[j]);
            _strokeLabels[i * 4 + j] = sl;
        }
    }

    for (uint8_t i = 0; i < _textCount; i++) {
        const TextConfig& cfg = _textConfigs[i];
        _labels[i] = lv_label_create(scr);
        lv_obj_set_style_text_font(_labels[i], cfg.font, 0);
        lv_obj_set_style_text_color(_labels[i], lv_color_white(), 0);
        lv_label_set_text(_labels[i], "");
        lv_obj_align(_labels[i], cfg.align, cfg.x_ofs, cfg.y_ofs);
    }

    lv_obj_invalidate(lv_display_get_screen_active(_disp));
    lv_refr_now(_disp);

    lvgl_port_unlock();
}

// ── IConsumer overrides ──────────────────────────────────────────────

void ILvglDisplayConsumer::applyState(TallyState state) {
    if (!_zoneObjs || !_labels[0] || !lvgl_port_lock(portMAX_DELAY)) return;

    uint8_t r, g, b;
    stateToColor(state, r, g, b);

    const lv_color_t col = lv_color_make(r, g, b);  // raw — hardware dims the entire framebuffer

    for (uint8_t i = 0; i < _zoneCount; i++) {
        const IDisplayConsumer::Zone& z = _displayZones[i];
        if (state < z.minState) {
            lv_obj_set_style_bg_opa(_zoneObjs[i], LV_OPA_TRANSP, 0);
        } else if (z.stateColored) {
            lv_obj_set_style_bg_color(_zoneObjs[i], col, 0);
            lv_obj_set_style_bg_opa(_zoneObjs[i], LV_OPA_COVER, 0);
        } else if (!_alertTask) {
            lv_obj_set_style_bg_opa(_zoneObjs[i], LV_OPA_TRANSP, 0);
        }
    }
    for (uint8_t i = 0; i < _textCount; i++) {
        lv_obj_set_style_text_color(_labels[i],
            contrastTextColor(r, g, b, _textConfigs[i].brightness), 0);
        if (_textConfigs[i].strokeWidth > 0) {
            lv_color_t sc = contrastStrokeColor(r, g, b, _textConfigs[i].brightness);
            for (uint8_t j = 0; j < 4; j++) {
                if (_strokeLabels[i * 4 + j])
                    lv_obj_set_style_text_color(_strokeLabels[i * 4 + j], sc, 0);
            }
        }
    }
    for (uint8_t i = 0; i < _textCount; i++) {
        if (!isRevertPending(i)) applySlot(i);
    }

    lv_refr_now(_disp);

    lvgl_port_unlock();
}

void ILvglDisplayConsumer::setAlertStep(DeviceAlertAction action,
                                        DeviceAlertTarget target, uint8_t step) {
    const AlertPattern* cfg = getAlertPattern(action);
    if (!cfg || !_zoneObjs || !lvgl_port_lock(portMAX_DELAY)) return;

    for (uint8_t i = 0; i < _zoneCount; i++) {
        const IDisplayConsumer::Zone& z = _displayZones[i];

        if (z.alertTarget != DeviceAlertTarget::ALL
            && target != DeviceAlertTarget::ALL
            && z.alertTarget != target) continue;

        uint8_t v = z.alertVariant % cfg->variantCount;
        TallyState s = cfg->patterns[v][step % cfg->patternLen];

        if (s == TallyState::NONE) {
            if (z.stateColored && _state >= z.minState) {
                uint8_t r, g, b;
                stateToColor(_state, r, g, b);
                lv_obj_set_style_bg_color(_zoneObjs[i], lv_color_make(r, g, b), 0);
                lv_obj_set_style_bg_opa(_zoneObjs[i], LV_OPA_COVER, 0);
            } else {
                lv_obj_set_style_bg_opa(_zoneObjs[i], LV_OPA_TRANSP, 0);
            }
        } else {
            uint8_t r, g, b;
            stateToColor(s, r, g, b);
            lv_obj_set_style_bg_color(_zoneObjs[i], lv_color_make(r, g, b), 0);
            lv_obj_set_style_bg_opa(_zoneObjs[i], LV_OPA_COVER, 0);
        }
    }

    lv_refr_now(_disp);

    lvgl_port_unlock();
}

// ── IDisplayConsumer override ────────────────────────────────────────

void ILvglDisplayConsumer::onTextChanged(uint8_t index, const char* text) {
    if (index >= _textCount || !_labels[index]) return;
    if (!lvgl_port_lock(portMAX_DELAY)) return;
    lv_label_set_text(_labels[index], text);
    for (uint8_t j = 0; j < 4; j++) {
        if (_strokeLabels[index * 4 + j])
            lv_label_set_text(_strokeLabels[index * 4 + j], text);
    }
    lvgl_port_unlock();
}

// ── Private helpers ──────────────────────────────────────────────────

void ILvglDisplayConsumer::applySlot(uint8_t index) {
    if (index >= _textCount || !_labels[index]) return;
    const char* text = getBaseText(index);
    lv_label_set_text(_labels[index], text);
    for (uint8_t j = 0; j < 4; j++) {
        if (_strokeLabels[index * 4 + j])
            lv_label_set_text(_strokeLabels[index * 4 + j], text);
    }
}

// TODO Improve. Take into account the background color in any case.
lv_color_t ILvglDisplayConsumer::contrastTextColor(uint8_t r, uint8_t g, uint8_t b, uint8_t brightness) {
    uint16_t y = (r * 299u + g * 587u + b * 114u) / 1000u;
    return (y > 180) ? lv_color_make(0, 0, 0) : lv_color_make(brightness, brightness, brightness);
}

lv_color_t ILvglDisplayConsumer::contrastStrokeColor(uint8_t r, uint8_t g, uint8_t b, uint8_t brightness) {
    uint16_t y = (r * 299u + g * 587u + b * 114u) / 1000u;
    return (y > 180) ? lv_color_make(brightness, brightness, brightness) : lv_color_make(0, 0, 0);
}

