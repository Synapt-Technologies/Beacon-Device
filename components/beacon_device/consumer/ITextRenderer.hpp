#pragma once

#include "types/TallyTypes.hpp"
#include <stdint.h>

// Interface for display consumers that can render alert text.
// ConsumerGroup dispatches alert text commands; the consumer manages slot state internally.
class ITextRenderer {
public:
    // Alert text — written to the consumer's configured alertSlot for the given duration.
    // timeout==0 means show indefinitely until clearAlertText() is called.
    virtual void setAlertText(const char* text, uint32_t timeout) = 0;
    virtual void clearAlertText() = 0;
    virtual DeviceAlertTarget alertTextTarget() const = 0;

    // General-purpose slot text — permanent (no timeout).
    virtual void setSlotText(uint8_t slot, const char* text) = 0;

    virtual ~ITextRenderer() = default;
};
