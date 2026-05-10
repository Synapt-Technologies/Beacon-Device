#pragma once

#include "consumer/IConsumer.hpp"
#include "freertos/timers.h"
#include <cstring>

class IDisplayConsumer : public IConsumer {
public:
    static constexpr uint8_t TEXT_COUNT = 8;
    static constexpr size_t  TEXT_BUF   = 64;

    // A rectangular zone on the display.
    struct Zone {
        int16_t           x, y, w, h;       // position and size in pixels
        uint8_t           alertVariant;      // alert pattern variant (0 = no flash)
        DeviceAlertTarget alertTarget;       // which alert targets activate this zone
        TallyState        minState;          // visible when _state >= minState (NONE = always)
        bool              stateColored;      // true = tracks tally color; false = transparent when idle
    };

    virtual ~IDisplayConsumer();

    IDisplayConsumer* asDisplay() override { return this; }

    // Non-virtual. timeout==0 stores as permanent base; timeout>0 shows temporarily then reverts.
    void setText(const char* text, uint8_t index, uint32_t timeout = 0);

protected:
    IDisplayConsumer() = default;

    // Called when the text to display for a slot changes (on set OR on revert to base).
    // Implementors must acquire any necessary rendering lock inside this method.
    virtual void onTextChanged(uint8_t index, const char* text) = 0;

    const char* getBaseText(uint8_t index) const     { return _texts[index].base; }
    bool        isRevertPending(uint8_t index) const { return _texts[index].revert != nullptr; }

    // Common utility: canonical name for a tally state, "" for NONE.
    static const char* stateName(TallyState s) {
        switch (s) {
            case TallyState::PROGRAM: return "PROGRAM";
            case TallyState::PREVIEW: return "PREVIEW";
            case TallyState::INFO:    return "INFO";
            case TallyState::WARNING: return "WARNING";
            case TallyState::DANGER:  return "DANGER";
            default:                  return "";
        }
    }

private:
    struct TextSlot {
        char          base[TEXT_BUF] = {};
        TimerHandle_t revert         = nullptr;
    };
    TextSlot _texts[TEXT_COUNT];

    struct RevertCtx { IDisplayConsumer* self; uint8_t index; };
    static void revertTimerCb(TimerHandle_t h);
};
