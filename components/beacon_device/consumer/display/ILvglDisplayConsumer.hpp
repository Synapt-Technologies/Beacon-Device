#pragma once

#include "consumer/IDisplayConsumer.hpp"
#include "consumer/ConsumerGroup.hpp"
#include "lvgl.h"

class ILvglDisplayConsumer : public IDisplayConsumer {
public:
    struct TextConfig {
        uint8_t    brightness  = 255;
        lv_align_t align       = LV_ALIGN_CENTER;
        int32_t    x_ofs       = 0;
        int32_t    y_ofs       = 0;
        uint8_t    strokeWidth = 0;
        int32_t    boundW      = 0;     // 0 = full display width
        int32_t    boundH      = 0;     // 0 = full display height
        bool       wrap        = false;

        virtual const lv_font_t* resolveFont(const char* text, lv_display_t* disp) const = 0;
        virtual ~TextConfig() = default;

    protected:
        TextConfig() = default;
        TextConfig(uint8_t brightness_, lv_align_t align_, int32_t x_ofs_, int32_t y_ofs_,
                   uint8_t strokeWidth_, int32_t boundW_ = 0, int32_t boundH_ = 0, bool wrap_ = false)
            : brightness(brightness_), align(align_), x_ofs(x_ofs_), y_ofs(y_ofs_),
              strokeWidth(strokeWidth_), boundW(boundW_), boundH(boundH_), wrap(wrap_)
        {}
    };

    struct FixedTextConfig : TextConfig {
        const lv_font_t* font;

        FixedTextConfig(const lv_font_t* f,
                        uint8_t brightness_ = 255,
                        lv_align_t align_   = LV_ALIGN_CENTER,
                        int32_t x_ofs_      = 0,
                        int32_t y_ofs_      = 0,
                        uint8_t strokeWidth_= 0,
                        int32_t boundW_     = 0,
                        int32_t boundH_     = 0,
                        bool    wrap_       = false)
            : TextConfig(brightness_, align_, x_ofs_, y_ofs_, strokeWidth_, boundW_, boundH_, wrap_), font(f)
        {}

        const lv_font_t* resolveFont(const char*, lv_display_t*) const override { return font; }
    };

    struct AutoTextConfig : TextConfig {
        uint8_t maxSize = 0;
        uint8_t minSize = 0;

        AutoTextConfig(uint8_t brightness_  = 255,
                       lv_align_t align_    = LV_ALIGN_CENTER,
                       int32_t x_ofs_       = 0,
                       int32_t y_ofs_       = 0,
                       uint8_t strokeWidth_ = 0,
                       uint8_t minSize_     = 0,
                       uint8_t maxSize_     = 0,
                       int32_t boundW_      = 0,
                       int32_t boundH_      = 0,
                       bool    wrap_        = false)
            : TextConfig(brightness_, align_, x_ofs_, y_ofs_, strokeWidth_, boundW_, boundH_, wrap_),
              maxSize(maxSize_), minSize(minSize_)
        {}

        const lv_font_t* resolveFont(const char* text, lv_display_t* disp) const override;
    };

    ~ILvglDisplayConsumer() override;

    void init() override;

    void registerWith(ConsumerGroup& group) override {
        group.addSection(this);
        group.addTextRenderer(this);
    }

    uint8_t labelCount() const override { return _textCount; }

    // ISection overrides — driven by ConsumerGroup
    void applyState(TallyState state) override;
    void applyAlertStep(DeviceAlertAction action, DeviceAlertTarget target,
                        uint8_t step, TallyState fallback) override;

protected:
    ILvglDisplayConsumer(const IDisplayConsumer::Zone* zones, uint8_t zoneCount,
                         const TextConfig* const* textConfigs, uint8_t textCount);

    virtual lv_display_t* initHardware() = 0;

    // Call before lvgl_port_add_disp() to ensure the port is initialised exactly once.
    static void ensureLvglPortInited();

    // Owned by the derived class for cleanup ordering — set to nullptr after removal.
    lv_display_t* _disp = nullptr;

    // IDisplayConsumer override
    void onTextChanged(uint8_t index, const char* text) override;

private:
    void buildUi();
    void applySlot(uint8_t index);
    static lv_color_t contrastTextColor(uint8_t r, uint8_t g, uint8_t b, uint8_t brightness = 255);
    static lv_color_t contrastStrokeColor(uint8_t r, uint8_t g, uint8_t b, uint8_t brightness = 255);

    const IDisplayConsumer::Zone* _displayZones;
    uint8_t                 _zoneCount;
    const TextConfig* const* _textConfigs;   // non-owning
    uint8_t           _textCount;

    lv_obj_t** _zoneObjs     = nullptr;
    lv_obj_t** _labels       = nullptr;   // dynamic, size = _textCount
    lv_obj_t** _strokeLabels = nullptr;   // dynamic, size = _textCount * 4, nullptr where strokeWidth == 0
};
