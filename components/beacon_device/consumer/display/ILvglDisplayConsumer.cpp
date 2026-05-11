#include "consumer/display/ILvglDisplayConsumer.hpp"

#include "esp_lvgl_port.h"
#include "esp_log.h"

namespace {
constexpr const char* TAG = "LvglDisplay";
}

static bool s_lvgl_inited = false;

// ── Construction / destruction ───────────────────────────────────────

ILvglDisplayConsumer::ILvglDisplayConsumer(const IDisplayConsumer::Zone* zones, uint8_t zoneCount,
                                           const lv_font_t* titleFont, const lv_font_t* subtextFont)
    : _displayZones(zones), _zoneCount(zoneCount),
      _titleFont(titleFont), _subtextFont(subtextFont)
{
    _zoneObjs = new lv_obj_t*[_zoneCount]();
    rebuildLut();
}

ILvglDisplayConsumer::~ILvglDisplayConsumer() {
    // _disp must already be removed by the derived destructor (to preserve hardware order).
    delete[] _zoneObjs;
}

// ── Protected helpers ────────────────────────────────────────────────

void ILvglDisplayConsumer::ensureLvglPortInited() {
    if (s_lvgl_inited) return;
    lvgl_port_cfg_t port = ESP_LVGL_PORT_INIT_CONFIG();
    port.task_priority   = 4;
    port.task_stack      = 8 * 1024;
    port.timer_period_ms = 5;
    ESP_ERROR_CHECK(lvgl_port_init(&port));
    s_lvgl_inited = true;
}

void ILvglDisplayConsumer::finishInit(lv_display_t* disp) {
    _disp = disp;
    buildUi();
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

    _labels[0] = lv_label_create(scr);
    lv_obj_set_style_text_font(_labels[0], _titleFont, 0);
    lv_obj_set_style_text_color(_labels[0], lv_color_white(), 0);
    lv_label_set_text(_labels[0], "");
    lv_obj_align(_labels[0], LV_ALIGN_CENTER, 0, 0);

    _labels[1] = lv_label_create(scr);
    lv_obj_set_style_text_font(_labels[1], _subtextFont, 0);
    lv_obj_set_style_text_color(_labels[1], lv_color_white(), 0);
    lv_label_set_text(_labels[1], "");
    lv_obj_align(_labels[1], LV_ALIGN_CENTER, 0, 70);

    lv_obj_invalidate(lv_display_get_screen_active(_disp));
    lv_refr_now(_disp);

    lvgl_port_unlock();
}

// ── IConsumer overrides ──────────────────────────────────────────────

void ILvglDisplayConsumer::setColor(uint8_t r, uint8_t g, uint8_t b) {
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

void ILvglDisplayConsumer::setAlertStep(DeviceAlertAction action,
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

// TODO: Move to IConsumer?
uint32_t ILvglDisplayConsumer::getAlertStepLength(DeviceAlertAction action) {
    const AlertPatternConfig* cfg = getAlertPattern(action);
    return cfg ? cfg->speedMs : 0;
}

uint8_t ILvglDisplayConsumer::getAlertStepCount(DeviceAlertAction action) {
    const AlertPatternConfig* cfg = getAlertPattern(action);
    return cfg ? cfg->patternLen : 1;
}

// ── IDisplayConsumer override ────────────────────────────────────────

void ILvglDisplayConsumer::onTextChanged(uint8_t index, const char* text) {
    if (index >= 2 || !_labels[index]) return;
    if (!lvgl_port_lock(portMAX_DELAY)) return;
    lv_label_set_text(_labels[index], text);
    lvgl_port_unlock();
}

// ── Private helpers ──────────────────────────────────────────────────

void ILvglDisplayConsumer::applySlot(uint8_t index) {
    lv_label_set_text(_labels[index], getBaseText(index));
}

lv_color_t ILvglDisplayConsumer::contrastTextColor(uint8_t r, uint8_t g, uint8_t b, uint8_t brightness) {
    uint16_t y = (r * 299u + g * 587u + b * 114u) / 1000u;
    return (y > 140) ? lv_color_make(0, 0, 0) : lv_color_make(brightness, brightness, brightness);
}

// ── Alert pattern table ──────────────────────────────────────────────

const ILvglDisplayConsumer::AlertPatternConfig*
ILvglDisplayConsumer::getAlertPattern(DeviceAlertAction action) {

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
