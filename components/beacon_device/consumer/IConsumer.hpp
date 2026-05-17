#pragma once

#include "consumer/ISection.hpp"
#include <stdint.h>

class ConsumerGroup; // forward declaration for registerWith

class IConsumer : public ISection {
public:
    virtual ~IConsumer() = default;

    virtual void init() {}

    // Called by ConsumerGroup::addConsumer(). Override to call group.addSection(this)
    // and optionally group.addTextRenderer(this) for display consumers.
    virtual void registerWith(ConsumerGroup& group) {}

    // Non-virtual: stores brightness and delegates to applyBrightness().
    void setBrightness(uint8_t brightness) override {
        _brightness = brightness;
        applyBrightness();
    }

    struct AlertPattern {
        uint32_t speedMs;
        const TallyState (*patterns)[8];  // [variant][step], up to 8 steps
        uint8_t variantCount;
        uint8_t patternLen;
    };

    static const AlertPattern* getAlertPattern(DeviceAlertAction action);

    static void stateToColor(TallyState state, uint8_t& r, uint8_t& g, uint8_t& b) {
        switch (state) {
            case TallyState::DANGER:
            case TallyState::WARNING: r = 255; g = 255; b =   0; return;
            case TallyState::INFO:    r =   0; g =   0; b = 255; return;
            case TallyState::PREVIEW: r =   0; g = 255; b =   0; return;
            case TallyState::PROGRAM: r = 255; g =   0; b =   0; return;
            default:                  r =   0; g =   0; b =   0; return;
        }
    }

    // Default no-op implementations — concrete consumers override only what applies.
    void applyState(TallyState /*state*/) override {}
    void applyAlertStep(DeviceAlertAction /*action*/, DeviceAlertTarget /*target*/,
                        uint8_t /*step*/, TallyState /*fallback*/) override {}

protected:
    uint8_t _brightness = 255;

    // Override to react to brightness changes (hardware PWM, LUT rebuild, etc.).
    virtual void applyBrightness(uint8_t /*brightness*/) {}
};
