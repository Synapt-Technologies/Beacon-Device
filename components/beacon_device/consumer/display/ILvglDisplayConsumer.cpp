#include "consumer/display/ILvglDisplayConsumer.hpp"

#include "esp_lvgl_port.h"
#include "esp_log.h"
#include "misc/lv_text.h"

#if HELVATICA_FONT_140
LV_FONT_DECLARE(helvatica_140);
#endif
#if HELVATICA_FONT_130
LV_FONT_DECLARE(helvatica_130);
#endif
#if HELVATICA_FONT_120
LV_FONT_DECLARE(helvatica_120);
#endif
#if HELVATICA_FONT_100
LV_FONT_DECLARE(helvatica_100);
#endif
#if HELVATICA_FONT_80
LV_FONT_DECLARE(helvatica_80);
#endif
#if HELVATICA_FONT_60
LV_FONT_DECLARE(helvatica_60);
#endif

namespace {
constexpr const char* TAG = "LvglDisplay";

// TODO look at the fonts used. Small height fonts don't look good on lowres displays. Maybe do UNSCII 8 and 16 for all fonts smaller than 20?
struct AutoFontEntry { uint8_t size; const lv_font_t* font; };
static const AutoFontEntry k_autoFonts[] = {
#if HELVATICA_FONT_140
    { 140, &helvatica_140 },
#endif
#if HELVATICA_FONT_130
    { 130, &helvatica_130 },
#endif
#if HELVATICA_FONT_120
    { 120, &helvatica_120 },
#endif
#if HELVATICA_FONT_100
    { 100, &helvatica_100 },
#endif
#if HELVATICA_FONT_80
    {  80, &helvatica_80  },
#endif
#if HELVATICA_FONT_60
    {  60, &helvatica_60  },
#endif
#if LV_FONT_MONTSERRAT_48
    {  48, &lv_font_montserrat_48 },
#endif
#if LV_FONT_MONTSERRAT_44
    {  44, &lv_font_montserrat_44 },
#endif
#if LV_FONT_MONTSERRAT_40
    {  40, &lv_font_montserrat_40 }, 
#endif
#if LV_FONT_MONTSERRAT_36
    {  36, &lv_font_montserrat_36 },
#endif
#if LV_FONT_MONTSERRAT_32
    {  32, &lv_font_montserrat_32 },
#endif
#if LV_FONT_MONTSERRAT_28
    {  28, &lv_font_montserrat_28 },
#endif
#if LV_FONT_MONTSERRAT_24
    {  24, &lv_font_montserrat_24 },
#endif
#if LV_FONT_MONTSERRAT_20
    {  20, &lv_font_montserrat_20 },
#endif
#if LV_FONT_MONTSERRAT_16
    {  16, &lv_font_montserrat_16 },
#endif
#if LV_FONT_MONTSERRAT_12
    {  12, &lv_font_montserrat_12 },
#endif
#if LV_FONT_UNSCII_8
    {   8, &lv_font_unscii_8 },
#endif
};
constexpr size_t k_autoFontCount = sizeof(k_autoFonts) / sizeof(k_autoFonts[0]);
}

static bool s_lvgl_inited = false;

// ── AutoTextConfig ───────────────────────────────────────────────────

const lv_font_t* ILvglDisplayConsumer::AutoTextConfig::resolveFont(const char* text, lv_display_t* disp) const {
    const int32_t effectiveW = boundW > 0 ? boundW : lv_display_get_horizontal_resolution(disp);
    const int32_t effectiveH = boundH > 0 ? boundH : lv_display_get_vertical_resolution(disp);
    const lv_font_t* fallback = k_autoFonts[k_autoFontCount - 1].font;

    for (size_t i = 0; i < k_autoFontCount; i++) {
        const uint8_t sz = k_autoFonts[i].size;
        if (maxSize > 0 && sz > maxSize) continue;
        if (minSize > 0 && sz < minSize) break;

        lv_point_t pt;
        if (wrap) {
            lv_text_get_size(&pt, text, k_autoFonts[i].font, 0, 0, effectiveW, LV_TEXT_FLAG_NONE);
            if (pt.y <= effectiveH) return k_autoFonts[i].font;
        } else {
            lv_text_get_size(&pt, text, k_autoFonts[i].font, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
            if (pt.x <= effectiveW) return k_autoFonts[i].font;
        }
        fallback = k_autoFonts[i].font;
    }
    return fallback;
}

// ── Construction / destruction ───────────────────────────────────────

ILvglDisplayConsumer::ILvglDisplayConsumer(const IDisplayConsumer::Zone* zones, uint8_t zoneCount,
                                           const TextConfig* const* textConfigs, uint8_t textCount)
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

    auto configureLabel = [&](lv_obj_t* obj, const TextConfig& cfg) {
        if (cfg.boundW > 0 || cfg.wrap) {
            const int32_t w = cfg.boundW > 0
                ? cfg.boundW
                : lv_display_get_horizontal_resolution(_disp);
            lv_obj_set_width(obj, w);
            lv_label_set_long_mode(obj, cfg.wrap ? LV_LABEL_LONG_WRAP : LV_LABEL_LONG_CLIP);

            lv_text_align_t ta;
            switch (cfg.align) {
                case LV_ALIGN_TOP_RIGHT:
                case LV_ALIGN_RIGHT_MID:
                case LV_ALIGN_BOTTOM_RIGHT:
                    ta = LV_TEXT_ALIGN_RIGHT; break;
                case LV_ALIGN_TOP_LEFT:
                case LV_ALIGN_LEFT_MID:
                case LV_ALIGN_BOTTOM_LEFT:
                case LV_ALIGN_DEFAULT:
                    ta = LV_TEXT_ALIGN_LEFT; break;
                default:
                    ta = LV_TEXT_ALIGN_CENTER; break;
            }
            lv_obj_set_style_text_align(obj, ta, 0);
        }
    };

    for (uint8_t i = 0; i < _textCount; i++) {
        const TextConfig& cfg = *_textConfigs[i];
        if (cfg.strokeWidth == 0) continue;
        const lv_font_t* font   = cfg.resolveFont("", _disp);
        const int32_t sw        = cfg.strokeWidth;
        const int32_t dx[4]     = { -sw,  sw,   0,   0 };
        const int32_t dy[4]     = {   0,   0, -sw,  sw };
        for (uint8_t j = 0; j < 4; j++) {
            lv_obj_t* sl = lv_label_create(scr);
            lv_obj_set_style_text_font(sl, font, 0);
            lv_obj_set_style_text_color(sl, lv_color_black(), 0);
            lv_label_set_text(sl, "");
            configureLabel(sl, cfg);
            lv_obj_align(sl, cfg.align, cfg.x_ofs + dx[j], cfg.y_ofs + dy[j]);
            _strokeLabels[i * 4 + j] = sl;
        }
    }

    for (uint8_t i = 0; i < _textCount; i++) {
        const TextConfig& cfg = *_textConfigs[i];
        _labels[i] = lv_label_create(scr);
        lv_obj_set_style_text_font(_labels[i], cfg.resolveFont("", _disp), 0);
        lv_obj_set_style_text_color(_labels[i], lv_color_white(), 0);
        lv_label_set_text(_labels[i], "");
        configureLabel(_labels[i], cfg);
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
        const TextConfig& cfg = *_textConfigs[i];
        lv_obj_set_style_text_color(_labels[i],
            contrastTextColor(r, g, b, cfg.brightness), 0);
        if (cfg.strokeWidth > 0) {
            lv_color_t sc = contrastStrokeColor(r, g, b, cfg.brightness);
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
    const lv_font_t* font = _textConfigs[index]->resolveFont(text, _disp);
    lv_obj_set_style_text_font(_labels[index], font, 0);
    lv_label_set_text(_labels[index], text);
    for (uint8_t j = 0; j < 4; j++) {
        if (_strokeLabels[index * 4 + j]) {
            lv_obj_set_style_text_font(_strokeLabels[index * 4 + j], font, 0);
            lv_label_set_text(_strokeLabels[index * 4 + j], text);
        }
    }
    lvgl_port_unlock();
}

// ── Private helpers ──────────────────────────────────────────────────

void ILvglDisplayConsumer::applySlot(uint8_t index) {
    if (index >= _textCount || !_labels[index]) return;
    const char* text = getBaseText(index);
    const lv_font_t* font = _textConfigs[index]->resolveFont(text, _disp);
    lv_obj_set_style_text_font(_labels[index], font, 0);
    lv_label_set_text(_labels[index], text);
    for (uint8_t j = 0; j < 4; j++) {
        if (_strokeLabels[index * 4 + j]) {
            lv_obj_set_style_text_font(_strokeLabels[index * 4 + j], font, 0);
            lv_label_set_text(_strokeLabels[index * 4 + j], text);
        }
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

