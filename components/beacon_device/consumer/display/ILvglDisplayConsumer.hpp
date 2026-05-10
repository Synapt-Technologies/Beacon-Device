#pragma once

#include "consumer/IDisplayConsumer.hpp"
#include "lvgl.h"

class ILvglDisplayConsumer : public IDisplayConsumer {
public:
    ~ILvglDisplayConsumer() override;

protected:
    ILvglDisplayConsumer(const IDisplayConsumer::Zone* zones, uint8_t zoneCount,
                         const lv_font_t* titleFont, const lv_font_t* subtextFont);

    // Call from derived constructor after hardware init returns a valid lv_display_t*.
    void finishInit(lv_display_t* disp);

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
    uint8_t          _zoneCount;
    const lv_font_t* _titleFont;
    const lv_font_t* _subtextFont;

    lv_obj_t**  _zoneObjs  = nullptr;
    lv_obj_t*   _labels[2] = {};
};
