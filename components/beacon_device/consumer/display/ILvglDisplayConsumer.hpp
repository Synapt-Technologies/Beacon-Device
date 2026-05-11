#pragma once

#include "consumer/IDisplayConsumer.hpp"
#include "lvgl.h"

class ILvglDisplayConsumer : public IDisplayConsumer {
public:
    struct FontConfig {
        const lv_font_t* font;
        uint8_t    brightness = 255;
        lv_align_t align      = LV_ALIGN_CENTER;
        int32_t    x_ofs      = 0;
        int32_t    y_ofs      = 0;
    };

    ~ILvglDisplayConsumer() override;

    void init() override;

    uint8_t labelCount() const override { return _labelCount; }

protected:
    ILvglDisplayConsumer(const IDisplayConsumer::Zone* zones, uint8_t zoneCount,
                         const FontConfig* labelConfigs, uint8_t labelCount);

    virtual lv_display_t* initHardware() = 0;

    // Call before lvgl_port_add_disp() to ensure the port is initialised exactly once.
    static void ensureLvglPortInited();

    // Owned by the derived class for cleanup ordering — set to nullptr after removal.
    lv_display_t* _disp = nullptr;

    // IConsumer overrides — shared LVGL rendering
    void setColor(uint8_t r, uint8_t g, uint8_t b) override;
    uint32_t getAlertStepLength(DeviceAlertAction action) override;
    uint8_t  getAlertStepCount(DeviceAlertAction action) override;
    void setAlertStep(DeviceAlertAction action, DeviceAlertTarget target, uint8_t step) override;

    // IDisplayConsumer override
    void onTextChanged(uint8_t index, const char* text) override;

private:
    struct AlertPatternConfig {
        uint32_t speedMs;
        const TallyState (*patterns)[5];
        uint8_t patternLen;
        uint8_t variantCount;
    };
    static const AlertPatternConfig* getAlertPattern(DeviceAlertAction action);

    void buildUi();
    void applySlot(uint8_t index);
    static lv_color_t contrastTextColor(uint8_t r, uint8_t g, uint8_t b, uint8_t brightness = 255);

    const IDisplayConsumer::Zone* _displayZones;
    uint8_t           _zoneCount;
    const FontConfig* _labelConfigs;   // non-owning
    uint8_t           _labelCount;

    lv_obj_t** _zoneObjs = nullptr;
    lv_obj_t** _labels   = nullptr;    // dynamic, size = _labelCount
};
