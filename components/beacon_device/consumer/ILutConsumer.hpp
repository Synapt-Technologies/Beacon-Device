#pragma once

#include "consumer/IConsumer.hpp"
#include <cmath>

// Base class for consumers that apply per-pixel software brightness via a gamma LUT.
// Use this for LED consumers (WS2812, SimpleRGB) where no hardware dimming is available.
// LVGL display consumers (Hub75, ILI9341) use hardware dimming and should NOT inherit this.
class ILutConsumer : public IConsumer {
protected:
    ILutConsumer() { rebuildLut(); }

    void rebuildLut() {
        _lut[0] = 0;
        for (int i = 1; i < 256; i++) {
            float t = (i - 1) / 254.0f;
            float out = 255.0f * powf(t, 2.8f) * _brightness / 255.0f;
            _lut[i] = static_cast<uint8_t>(out + 0.5f);
        }
    }

    uint8_t scale_brightness(uint8_t value) const { return _lut[value]; }

    void setBrightness(uint8_t brightness) override {
        _brightness = brightness;
        rebuildLut();
        applyState(_state);
    }

private:
    uint8_t _lut[256] = {};
};
