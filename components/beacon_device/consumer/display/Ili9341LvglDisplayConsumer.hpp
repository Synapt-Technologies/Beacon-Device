#pragma once

#include "consumer/display/ILvglDisplayConsumer.hpp"
#include "driver/spi_common.h"
#include "esp_lcd_types.h"

class Ili9341LvglDisplayConsumer : public ILvglDisplayConsumer {
public:
    // ILI9341 on ESP32-2432S028 (CYD) — edit here to retarget hardware.
    static constexpr int PIN_MOSI = 13;
    static constexpr int PIN_SCLK = 14;
    static constexpr int PIN_CS   = 15;
    static constexpr int PIN_DC   = 2;
    static constexpr int PIN_RST  = 12;
    static constexpr int PIN_BL   = 21;
    static constexpr spi_host_device_t SPI_HOST = SPI3_HOST;
    static constexpr int LCD_W = 320;
    static constexpr int LCD_H = 240;

    // Zones are owned by the caller and must outlive this object.
    Ili9341LvglDisplayConsumer(const IDisplayConsumer::Zone* zones, uint8_t zoneCount,
                               const ILvglDisplayConsumer::TextConfig* const* textConfigs, uint8_t textCount);
    ~Ili9341LvglDisplayConsumer() override;

    void applyBrightness() override;

protected:
    lv_display_t* initHardware() override;

private:

    esp_lcd_panel_io_handle_t _ioHandle    = nullptr;
    esp_lcd_panel_handle_t    _panelHandle = nullptr;
};
