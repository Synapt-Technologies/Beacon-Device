#pragma once

#include "consumer/IConsumer.hpp"
#include "driver/gpio.h"


class SimpleRGBConsumer : public IConsumer {

public:
    SimpleRGBConsumer(gpio_num_t rPin, gpio_num_t gPin, gpio_num_t bPin, DeviceAlertTarget target) {
        _rPin = rPin;
        _gPin = gPin;
        _bPin = bPin;

        _target = target;

        gpio_set_direction(_rPin, GPIO_MODE_OUTPUT);
        gpio_set_direction(_gPin, GPIO_MODE_OUTPUT);
        gpio_set_direction(_bPin, GPIO_MODE_OUTPUT);
    }
    ~SimpleRGBConsumer() {

        gpio_set_direction(_rPin, GPIO_MODE_DISABLE);
        gpio_set_direction(_gPin, GPIO_MODE_DISABLE);
        gpio_set_direction(_bPin, GPIO_MODE_DISABLE);
    }

private:

    gpio_num_t        _rPin, _gPin, _bPin;
    DeviceAlertTarget _target;


    void setColor(uint8_t r, uint8_t g, uint8_t b) override { // TODO PWM
        gpio_set_level(_rPin, r > 0 ? 0 : 1);
        gpio_set_level(_gPin, g > 0 ? 0 : 1);
        gpio_set_level(_bPin, b > 0 ? 0 : 1);
    }


    void setAlertStep(DeviceAlertAction action, DeviceAlertTarget target, uint8_t step) override {
        if (target != _target) return; // This consumer only reacts to its configured target

        const AlertPatternConfig* pattern = getAlertPattern(action);
        if (!pattern) return; // No pattern for this action (e.g. CLEAR)

        TallyState state = pattern->pattern[step % pattern->patternLen];
        if (state == TallyState::NONE) {
            this->applyState(this->_state);
        }
        else {
            this->applyState(state);
        }
    }

    struct AlertPatternConfig {
        uint32_t speedMs;
        const TallyState* pattern;
        uint8_t patternLen;
    };

    uint32_t getAlertStepLength(DeviceAlertAction action) override {
        return this->getAlertPattern(action)->speedMs;
    }

    uint8_t getAlertStepCount(DeviceAlertAction action) override {
        return this->getAlertPattern(action)->patternLen;
    }

    // Returns nullptr for CLEAR (no pattern). TallyState::NONE = LED off.
    static const AlertPatternConfig* getAlertPattern(DeviceAlertAction action) {

        static const TallyState IDENT[]  = { TallyState::PREVIEW,   TallyState::PROGRAM };
        static const TallyState INFO[]   = { TallyState::INFO,      TallyState::NONE };
        static const TallyState NORMAL[] = { TallyState::WARNING,   TallyState::NONE };
        static const TallyState PRIO[]   = { TallyState::PROGRAM,   TallyState::WARNING };

        static const AlertPatternConfig PATTERNS[] = {
            { 400, IDENT,  2 },
            { 300, INFO,   2 },
            { 400, NORMAL, 2 },
            { 150, PRIO,   2 },
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
