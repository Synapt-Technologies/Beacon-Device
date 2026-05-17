#pragma once

#include "consumer/display/ILvglDisplayConsumer.hpp"
#include "hub75.h"

class Hub75LvglDisplayConsumer : public ILvglDisplayConsumer {
public:
    // Zones are owned by the caller and must outlive this object.
    Hub75LvglDisplayConsumer(const Hub75Config& config,
                             const IDisplayConsumer::Zone* zones, uint8_t zoneCount,
                             const ILvglDisplayConsumer::TextConfig* const* textConfigs, uint8_t textCount);
    ~Hub75LvglDisplayConsumer() override;

    void applyBrightness() override;

protected:
    lv_display_t* initHardware() override;

private:
    Hub75Config _config;
    Hub75Driver _driver;

    static void flushCb(lv_display_t* disp, const lv_area_t* area, uint8_t* px_map);
};
