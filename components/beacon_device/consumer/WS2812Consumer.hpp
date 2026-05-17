#pragma once

#include "led_strip.h"
#include "consumer/ILutConsumer.hpp"
#include "consumer/ConsumerGroup.hpp"
#include "freertos/semphr.h"

struct StripSection {
    uint8_t           startLed;
    uint8_t           alertPattern;   // variant index into the alert pattern table
    DeviceAlertTarget target;
    TallyState        minState     = TallyState::NONE;
    bool              stateColored = true;
};

class WS2812Consumer : public ILutConsumer {
public:
    WS2812Consumer(led_strip_handle_t strip, uint8_t ledCount,
                   StripSection sections[], uint8_t sectionCount)
        : _strip(strip), _ledCount(ledCount), _sections(sections), _sectionCount(sectionCount)
    {
        _mutex = xSemaphoreCreateMutex();
    }

    ~WS2812Consumer() {
        if (_mutex) vSemaphoreDelete(_mutex);
    }

    void registerWith(ConsumerGroup& group) override {
        group.addSection(this);
    }

    void applyState(TallyState state) override {
        uint8_t r, g, b;
        stateToColor(state, r, g, b);

        xSemaphoreTake(_mutex, portMAX_DELAY);
        for (uint8_t s = 0; s < _sectionCount; s++) {
            const StripSection& sec = _sections[s];
            const uint8_t start = sec.startLed;
            const uint8_t count = sectionLedCount(s);
            const bool    show  = (state >= sec.minState && sec.stateColored);
            for (uint8_t i = start; i < start + count; i++)
                led_strip_set_pixel(_strip, i,
                    show ? scale_brightness(r) : 0,
                    show ? scale_brightness(g) : 0,
                    show ? scale_brightness(b) : 0);
        }
        led_strip_refresh(_strip);
        xSemaphoreGive(_mutex);
    }

    void applyAlertStep(DeviceAlertAction action, DeviceAlertTarget target,
                        uint8_t step, TallyState fallback) override {
        const AlertPattern* pattern = getAlertPattern(action);
        if (!pattern) return;

        xSemaphoreTake(_mutex, portMAX_DELAY);
        for (uint8_t s = 0; s < _sectionCount; s++) {
            const StripSection& sec = _sections[s];
            if (sec.target != DeviceAlertTarget::ALL
                && target  != DeviceAlertTarget::ALL
                && sec.target != target) continue;

            const uint8_t    start      = sec.startLed;
            const uint8_t    count      = sectionLedCount(s);
            const uint8_t    variantIdx = sec.alertPattern % pattern->variantCount;
            const TallyState patState   = pattern->patterns[variantIdx][step % pattern->patternLen];

            if (patState == TallyState::NONE) {
                if (sec.stateColored && fallback >= sec.minState) {
                    uint8_t r, g, b;
                    stateToColor(fallback, r, g, b);
                    setColorRange(start, count, r, g, b);
                } else {
                    for (uint8_t i = start; i < start + count; i++)
                        led_strip_set_pixel(_strip, i, 0, 0, 0);
                }
            } else {
                uint8_t r, g, b;
                stateToColor(patState, r, g, b);
                setColorRange(start, count, r, g, b);
            }
        }
        led_strip_refresh(_strip);
        xSemaphoreGive(_mutex);
    }

private:
    led_strip_handle_t _strip;
    int                _ledCount;
    SemaphoreHandle_t  _mutex = nullptr;
    StripSection*      _sections;
    uint8_t            _sectionCount;

    uint8_t sectionLedCount(uint8_t s) const {
        return (s + 1 < _sectionCount)
            ? _sections[s + 1].startLed - _sections[s].startLed
            : static_cast<uint8_t>(_ledCount) - _sections[s].startLed;
    }

    void setColorRange(uint8_t start, uint8_t count, uint8_t r, uint8_t g, uint8_t b) {
        for (uint8_t i = start; i < start + count; i++)
            led_strip_set_pixel(_strip, i,
                scale_brightness(r), scale_brightness(g), scale_brightness(b));
    }
};
