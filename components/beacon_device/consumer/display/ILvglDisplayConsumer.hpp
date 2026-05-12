#pragma once

#include "consumer/IDisplayConsumer.hpp"
#include "lvgl.h"

class ILvglDisplayConsumer : public IDisplayConsumer {
public:
    struct TextConfig {
        const lv_font_t* font;
        uint8_t    brightness  = 255;
        lv_align_t align       = LV_ALIGN_CENTER;
        int32_t    x_ofs       = 0;
        int32_t    y_ofs       = 0;
        uint8_t    strokeWidth = 0;
    };

    ~ILvglDisplayConsumer() override;

    void init() override;

    uint8_t labelCount() const override { return _textCount; }

protected:
    ILvglDisplayConsumer(const IDisplayConsumer::Zone* zones, uint8_t zoneCount,
                         const TextConfig* textConfigs, uint8_t textCount);

    virtual lv_display_t* initHardware() = 0;

    // Call before lvgl_port_add_disp() to ensure the port is initialised exactly once.
    static void ensureLvglPortInited();

    // Owned by the derived class for cleanup ordering — set to nullptr after removal.
    lv_display_t* _disp = nullptr;

    // IConsumer overrides — shared LVGL rendering
    void applyState(TallyState state) override;
    void setAlertStep(DeviceAlertAction action, DeviceAlertTarget target, uint8_t step) override;

    // IDisplayConsumer override
    void onTextChanged(uint8_t index, const char* text) override;

private:
    void buildUi();
    void applySlot(uint8_t index);
    static lv_color_t contrastTextColor(uint8_t r, uint8_t g, uint8_t b, uint8_t brightness = 255);
    static lv_color_t contrastStrokeColor(uint8_t r, uint8_t g, uint8_t b, uint8_t brightness = 255);

    const IDisplayConsumer::Zone* _displayZones;
    uint8_t           _zoneCount;
    const TextConfig* _textConfigs;   // non-owning
    uint8_t           _textCount;

    lv_obj_t** _zoneObjs     = nullptr;
    lv_obj_t** _labels       = nullptr;   // dynamic, size = _textCount
    lv_obj_t** _strokeLabels = nullptr;   // dynamic, size = _textCount * 4, nullptr where strokeWidth == 0
};
