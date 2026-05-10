#pragma once

#include "consumer/IDisplayConsumer.hpp"
#include "driver/spi_common.h"
#include "esp_lcd_types.h"
#include "lvgl.h"

class CYDDisplayConsumer : public IDisplayConsumer {
public:
    // CYD (ESP32-2432S028) ILI9341 pins — edit here to retarget hardware.
    static constexpr int CYD_PIN_MOSI = 13;
    static constexpr int CYD_PIN_SCLK = 14;
    static constexpr int CYD_PIN_CS   = 15;
    static constexpr int CYD_PIN_DC   = 2;
    static constexpr int CYD_PIN_RST  = 12;
    static constexpr int CYD_PIN_BL   = 21;
    static constexpr spi_host_device_t CYD_SPI_HOST = SPI3_HOST;
    static constexpr int CYD_LCD_W    = 320;
    static constexpr int CYD_LCD_H    = 240;

    // Zones are owned by the caller and must outlive this object (same as WS2812Consumer).
    CYDDisplayConsumer(const IDisplayConsumer::Zone* zones, uint8_t zoneCount);
    ~CYDDisplayConsumer() override;

protected:
    void setColor(uint8_t r, uint8_t g, uint8_t b) override;
    uint32_t getAlertStepLength(DeviceAlertAction action) override;
    uint8_t  getAlertStepCount(DeviceAlertAction action) override;
    void setAlertStep(DeviceAlertAction action, DeviceAlertTarget target, uint8_t step) override;
    void onTextChanged(uint8_t index, const char* text) override;

private:
    struct AlertPatternConfig {
        uint32_t speedMs;
        const TallyState (*patterns)[5];
        uint8_t patternLen;
        uint8_t variantCount;
    };
    static const AlertPatternConfig* getAlertPattern(DeviceAlertAction action);

    void initLvgl();
    void buildUi();
    void applySlot(uint8_t index);   // call with LVGL lock held
    static lv_color_t contrastTextColor(uint8_t r, uint8_t g, uint8_t b, uint8_t brightness = 255);

    const IDisplayConsumer::Zone* _displayZones;
    uint8_t            _zoneCount;

    esp_lcd_panel_io_handle_t _ioHandle    = nullptr;
    esp_lcd_panel_handle_t    _panelHandle = nullptr;
    lv_display_t*  _disp      = nullptr;
    lv_obj_t**     _zoneObjs  = nullptr;   // heap-allocated, _zoneCount entries
    lv_obj_t*      _labels[2] = {};        // [0] title, [1] subtext
};
