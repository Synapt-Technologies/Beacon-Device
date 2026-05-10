#pragma once

#include "led_strip.h"
#include "consumer/IConsumer.hpp"
#include "driver/gpio.h"
#include "freertos/semphr.h"


struct StripSection { // Todo: Brightness section? -> Then the way they are iterated would change..
    uint8_t             startLed;
    uint8_t             alertPattern;
    DeviceAlertTarget   target;
};

class WS2812Consumer : public IConsumer {

public:
    WS2812Consumer(led_strip_handle_t strip, uint8_t ledCount, StripSection sections[], uint8_t sectionCount) {
        _strip = strip;
        _ledCount = ledCount;
        _sections = sections;
        _sectionCount = sectionCount;
        _mutex = xSemaphoreCreateMutex();
        rebuildLut();
    }
    ~WS2812Consumer() {
        if (_mutex) vSemaphoreDelete(_mutex);
    }

private:

    led_strip_handle_t  _strip;
    int                 _ledCount;
    SemaphoreHandle_t   _mutex;

    StripSection* _sections;
    uint8_t _sectionCount;


    void setColor(uint8_t r, uint8_t g, uint8_t b) {
        const uint8_t sr = scale_brightness(r);
        const uint8_t sg = scale_brightness(g);
        const uint8_t sb = scale_brightness(b);

        xSemaphoreTake(_mutex, portMAX_DELAY);
        for (int i = 0; i < _ledCount; i++)
            led_strip_set_pixel(_strip, i, sr, sg, sb);
        led_strip_refresh(_strip);
        xSemaphoreGive(_mutex);
    }

    void setColorRange(uint8_t start, uint8_t count, uint8_t r, uint8_t g, uint8_t b) {
        const uint8_t sr = scale_brightness(r);
        const uint8_t sg = scale_brightness(g);
        const uint8_t sb = scale_brightness(b);
        for (uint8_t i = start; i < start + count; i++)
            led_strip_set_pixel(_strip, i, sr, sg, sb);
    }


    void setAlertStep(DeviceAlertAction action, DeviceAlertTarget target, uint8_t step) override {
        const AlertPatternConfig* config = getAlertPattern(action);
        if (!config) return;

        xSemaphoreTake(_mutex, portMAX_DELAY);
        for (uint8_t s = 0; s < _sectionCount; s++) {
            const StripSection& sec = _sections[s];
            if (sec.target != DeviceAlertTarget::ALL && target != DeviceAlertTarget::ALL && sec.target != target)
                continue;

            uint8_t start = sec.startLed;
            uint8_t count = (s + 1 < _sectionCount)
                ? _sections[s + 1].startLed - start
                : _ledCount - start;

            uint8_t variantIdx = sec.alertPattern % config->variantCount;
            TallyState state = config->patterns[variantIdx][step % config->patternLen];

            if (state == TallyState::NONE) {
                state = _state;
            }

            uint8_t r, g, b;
            stateToColor(state, r, g, b);
            setColorRange(start, count, r, g, b);
        }
        led_strip_refresh(_strip);
        xSemaphoreGive(_mutex);
    }

    struct AlertPatternConfig {
        uint32_t speedMs;
        const TallyState (*patterns)[5];
        uint8_t patternLen;
        uint8_t variantCount;
    };

    uint32_t getAlertStepLength(DeviceAlertAction action) override {
        return this->getAlertPattern(action)->speedMs;
    }

    uint8_t getAlertStepCount(DeviceAlertAction action) override {
        return this->getAlertPattern(action)->patternLen;
    }

    // Returns nullptr for CLEAR (no pattern). TallyState::NONE = LED off.
    static const AlertPatternConfig* getAlertPattern(DeviceAlertAction action) {

        static const TallyState IDENT[][5]  = {
            { TallyState::NONE,     TallyState::NONE,       TallyState::NONE,       TallyState::NONE },
            { TallyState::PREVIEW,  TallyState::PROGRAM,    TallyState::PREVIEW,    TallyState::PROGRAM },
            { TallyState::PROGRAM,  TallyState::PREVIEW,    TallyState::PROGRAM,    TallyState::PREVIEW },
            { TallyState::PROGRAM,  TallyState::NONE,       TallyState::PREVIEW,    TallyState::NONE },
            { TallyState::NONE,     TallyState::PROGRAM,    TallyState::NONE,       TallyState::PREVIEW },
        };
        static const TallyState INFO[][5]   = {
            { TallyState::NONE,     TallyState::NONE,       TallyState::NONE,       TallyState::NONE },
            { TallyState::INFO,     TallyState::NONE,       TallyState::INFO,       TallyState::NONE },
            { TallyState::INFO,     TallyState::NONE,       TallyState::INFO,       TallyState::NONE },
        };
        static const TallyState NORMAL[][5] = {
            { TallyState::NONE,     TallyState::NONE,       TallyState::NONE,       TallyState::NONE },
            { TallyState::WARNING,  TallyState::NONE,       TallyState::WARNING,    TallyState::NONE },
            { TallyState::WARNING,  TallyState::NONE,       TallyState::WARNING,    TallyState::NONE },
        };
        static const TallyState PRIO[][5]   = {
            { TallyState::NONE,     TallyState::NONE,       TallyState::NONE,       TallyState::NONE },
            { TallyState::PROGRAM,  TallyState::WARNING,    TallyState::PROGRAM,    TallyState::WARNING },
            { TallyState::WARNING,  TallyState::PROGRAM,    TallyState::WARNING,    TallyState::PROGRAM },
            { TallyState::PROGRAM,  TallyState::NONE,       TallyState::WARNING,    TallyState::NONE },
            { TallyState::NONE,     TallyState::PROGRAM,    TallyState::NONE,       TallyState::WARNING },
        };

        static const AlertPatternConfig PATTERNS[] = {
            { 400, IDENT,  4, 5 },
            { 300, INFO,   4, 3 },
            { 400, NORMAL, 4, 3 },
            { 150, PRIO,   4, 5 },
        };

        switch (action) {
            case DeviceAlertAction::IDENT:  return &PATTERNS[0];
            case DeviceAlertAction::INFO:   return &PATTERNS[1];
            case DeviceAlertAction::NORMAL: return &PATTERNS[2];
            case DeviceAlertAction::PRIO:   return &PATTERNS[3];
            default:                        return nullptr;
        }
    }
};
