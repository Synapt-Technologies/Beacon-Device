#pragma once

#include "consumer/IConsumer.hpp"
#include "driver/spi_common.h"
#include "esp_lcd_types.h"
#include "lvgl.h"

class CYDDisplayConsumer : public ISmartConsumer {
public:
    // CYD (ESP32-2432S028) ILI9341 pins. Edit here to retarget hardware.
    static constexpr int CYD_PIN_MOSI   = 13;
    static constexpr int CYD_PIN_SCLK   = 14;
    static constexpr int CYD_PIN_CS     = 15;
    static constexpr int CYD_PIN_DC     = 2;
    static constexpr int CYD_PIN_RST    = 12;
    static constexpr int CYD_PIN_BL     = 21;
    static constexpr spi_host_device_t CYD_SPI_HOST = SPI3_HOST;
    static constexpr int CYD_LCD_W      = 320;
    static constexpr int CYD_LCD_H      = 240;
    static constexpr int BORDER_THICK   = 14;

    CYDDisplayConsumer();
    ~CYDDisplayConsumer() override;

    void setText(const char* text, uint8_t index, uint32_t timeout = 0) override;

protected:
    void setColor(uint8_t r, uint8_t g, uint8_t b) override;
    uint32_t getAlertStepLength(DeviceAlertAction action) override;
    uint8_t  getAlertStepCount(DeviceAlertAction action) override;
    void setAlertStep(DeviceAlertAction action, DeviceAlertTarget target, uint8_t step) override;

private:
    // Internal alert zones (configure here, no external object).
    // Zone 0 = left bar (OPERATOR), Zone 1 = right bar (TALENT).
    struct BorderZone {
        uint8_t           patternVariant;
        DeviceAlertTarget target;
    };
    static constexpr BorderZone ZONES[] = {
        { 1, DeviceAlertTarget::OPERATOR },
        { 2, DeviceAlertTarget::TALENT   },
    };
    static constexpr uint8_t ZONE_COUNT = sizeof(ZONES) / sizeof(ZONES[0]);

    struct AlertPatternConfig {
        uint32_t speedMs;
        const TallyState (*patterns)[5];
        uint8_t patternLen;
        uint8_t variantCount;
    };
    static const AlertPatternConfig* getAlertPattern(DeviceAlertAction action);

    void initLvgl();
    void buildUi();
    void applyTitleForState(TallyState s);
    static const char* stateName(TallyState s);
    static lv_color_t  contrastTextColor(uint8_t r, uint8_t g, uint8_t b);

    static void revertTitleCb(lv_timer_t* t);
    static void revertSubtextCb(lv_timer_t* t);

    esp_lcd_panel_io_handle_t _ioHandle    = nullptr;
    esp_lcd_panel_handle_t    _panelHandle = nullptr;
    lv_display_t* _disp  = nullptr;
    lv_obj_t*  _bg       = nullptr;
    lv_obj_t*  _title    = nullptr;
    lv_obj_t*  _subtext  = nullptr;
    lv_obj_t*  _borderL  = nullptr;
    lv_obj_t*  _borderR  = nullptr;

    lv_timer_t* _titleRevert   = nullptr;
    lv_timer_t* _subtextRevert = nullptr;
    bool _titleOverridden   = false;
    bool _subtextOverridden = false;
};
