#pragma once

#include "consumer/IConsumer.hpp"
#include "driver/spi_common.h"
#include "esp_lcd_types.h"
#include "lvgl.h"

class CYDDisplayConsumer : public ISmartConsumer {
public:
    // CYD (ESP32-2432S028) ILI9341 pins. Edit here to retarget hardware.
    static constexpr int CYD_PIN_MOSI = 13;
    static constexpr int CYD_PIN_SCLK = 14;
    static constexpr int CYD_PIN_CS   = 15;
    static constexpr int CYD_PIN_DC   = 2;
    static constexpr int CYD_PIN_RST  = 12;
    static constexpr int CYD_PIN_BL   = 21;
    static constexpr spi_host_device_t CYD_SPI_HOST = SPI3_HOST;
    static constexpr int CYD_LCD_W    = 320;
    static constexpr int CYD_LCD_H    = 240;

    CYDDisplayConsumer();
    ~CYDDisplayConsumer() override;

protected:
    void setColor(uint8_t r, uint8_t g, uint8_t b) override;
    uint32_t getAlertStepLength(DeviceAlertAction action) override;
    uint8_t  getAlertStepCount(DeviceAlertAction action) override;
    void setAlertStep(DeviceAlertAction action, DeviceAlertTarget target, uint8_t step) override;
    void onTextChanged(uint8_t index, const char* text) override;

private:
    // ── Display zones ─────────────────────────────────────────────────
    // Each zone is one lv_obj rectangle. stateColored=true means it tracks
    // the tally color; false means it is transparent unless an alert fires.
    struct Zone {
        int16_t           x, y, w, h;
        uint8_t           alertVariant;
        DeviceAlertTarget target;
        bool              stateColored;
    };
    static constexpr Zone ZONES[] = {
        {   0,  0, 320, 240,  0, DeviceAlertTarget::ALL,      true  }, // background
        {   0,  0,  14, 240,  1, DeviceAlertTarget::OPERATOR, false }, // left alert bar
        { 306,  0,  14, 240,  2, DeviceAlertTarget::TALENT,   false }, // right alert bar
    };
    static constexpr uint8_t ZONE_COUNT = sizeof(ZONES) / sizeof(ZONES[0]);

    // ── Alert patterns (same shape as WS2812Consumer) ─────────────────
    struct AlertPatternConfig {
        uint32_t speedMs;
        const TallyState (*patterns)[5];
        uint8_t patternLen;
        uint8_t variantCount;
    };
    static const AlertPatternConfig* getAlertPattern(DeviceAlertAction action);

    // ── Init / UI ─────────────────────────────────────────────────────
    void initLvgl();
    void buildUi();
    void applySlot(uint8_t index);  // call with LVGL lock held

    static const char*  stateName(TallyState s);
    static lv_color_t   contrastTextColor(uint8_t r, uint8_t g, uint8_t b);

    // ── Hardware handles ──────────────────────────────────────────────
    esp_lcd_panel_io_handle_t _ioHandle    = nullptr;
    esp_lcd_panel_handle_t    _panelHandle = nullptr;

    // ── LVGL objects ──────────────────────────────────────────────────
    lv_display_t* _disp               = nullptr;
    lv_obj_t*     _zoneObjs[ZONE_COUNT] = {};  // one per ZONES entry
    lv_obj_t*     _labels[2]           = {};   // [0] title, [1] subtext
};
