#pragma once

#include "consumer/ILutConsumer.hpp"
#include "consumer/ConsumerGroup.hpp"
#include "driver/gpio.h"

class SimpleRGBConsumer : public ILutConsumer {
public:
    SimpleRGBConsumer(gpio_num_t rPin, gpio_num_t gPin, gpio_num_t bPin,
                      DeviceAlertTarget target = DeviceAlertTarget::ALL)
        : _rPin(rPin), _gPin(gPin), _bPin(bPin), _target(target)
    {
        gpio_set_direction(_rPin, GPIO_MODE_OUTPUT);
        gpio_set_direction(_gPin, GPIO_MODE_OUTPUT);
        gpio_set_direction(_bPin, GPIO_MODE_OUTPUT);
    }

    ~SimpleRGBConsumer() {
        gpio_set_direction(_rPin, GPIO_MODE_DISABLE);
        gpio_set_direction(_gPin, GPIO_MODE_DISABLE);
        gpio_set_direction(_bPin, GPIO_MODE_DISABLE);
    }

    void registerWith(ConsumerGroup& group) override {
        group.addSection(this);
    }

    void applyState(TallyState state) override {
        uint8_t r, g, b;
        stateToColor(state, r, g, b);
        writeGpio(scale_brightness(r), scale_brightness(g), scale_brightness(b));
    }

    void applyAlertStep(DeviceAlertAction action, DeviceAlertTarget target,
                        uint8_t step, TallyState fallback) override {
        if (target != DeviceAlertTarget::ALL && _target != DeviceAlertTarget::ALL && _target != target)
            return;
        const AlertPattern* pattern = getAlertPattern(action);
        if (!pattern) return;
        // Variant 1: basic single-phase blink (variant 0 is always no-flash).
        TallyState s = pattern->patterns[1 % pattern->variantCount][step % pattern->patternLen];
        applyState(s == TallyState::NONE ? fallback : s);
    }

private:
    gpio_num_t        _rPin, _gPin, _bPin;
    DeviceAlertTarget _target;

    void writeGpio(uint8_t r, uint8_t g, uint8_t b) {
        // Binary GPIO (inverted logic). Replace with ledc for PWM.
        gpio_set_level(_rPin, r > 0 ? 0 : 1);
        gpio_set_level(_gPin, g > 0 ? 0 : 1);
        gpio_set_level(_bPin, b > 0 ? 0 : 1);
    }
};
