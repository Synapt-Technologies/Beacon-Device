#pragma once

#include "consumer/IConsumer.hpp"
#include <cmath>

// Capability mixin for consumers that apply per-pixel software brightness via a gamma LUT.
// Use for LED consumers (WS2812, SimpleRGB). LVGL displays use hardware dimming instead.
class ILutConsumer : public IConsumer {
protected:
    ILutConsumer() { rebuildLut(); }

    void applyBrightness() override { rebuildLut(); }

    void rebuildLut() {
        _lut[0] = 0;
        for (int i = 1; i < 256; i++) {
            float t = (i - 1) / 254.0f;
            float out = 255.0f * powf(t, 2.8f) * _brightness / 255.0f;
            _lut[i] = static_cast<uint8_t>(out + 0.5f);
        }
    }

    uint8_t scale_brightness(uint8_t value) const { return _lut[value]; }

private:
    uint8_t _lut[256] = {};
};
