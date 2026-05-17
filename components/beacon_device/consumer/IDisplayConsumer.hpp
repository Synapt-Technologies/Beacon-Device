#pragma once

#include "consumer/IConsumer.hpp"
#include "consumer/ITextRenderer.hpp"
#include "freertos/timers.h"
#include <cstring>

class IDisplayConsumer : public IConsumer, public ITextRenderer {
public:
    static constexpr uint8_t TEXT_COUNT = 8;
    static constexpr size_t  TEXT_BUF   = 64;

    // A rectangular zone on the display.
    struct Zone {
        int16_t           x, y, w, h;
        uint8_t           alertVariant;
        DeviceAlertTarget alertTarget;
        TallyState        minState;
        bool              stateColored;
    };

    virtual ~IDisplayConsumer();

    // TODO: More logical fname.
    virtual uint8_t labelCount() const { return 1; }

    // Set base text for a slot
    void setText(const char* text, uint8_t index) {
        applyText(text, index, 0);
    }

    void clearText(uint8_t index) { 
        applyText("", index, 0  ); 
    }

    // ── ITextRenderer ─────────────────────────────────────────────────
    void setAlertText(const char* text, uint32_t timeout) override {
        setText(text, _alertSlot, timeout);
    }
    void clearAlertText() override {
        clearText(_alertSlot);
    }
    DeviceAlertTarget alertTextTarget() const override { return _alertTextTarget; }

protected:
    IDisplayConsumer() = default;

    void applyText(uint8_t index, const char* text, uint32_t timeout = 0);
    virtual void onTextChanged(uint8_t index, const char* text) = 0;

    const char* getBaseText(uint8_t index) const { return _texts[index].base; }
    bool        isRevertPending(uint8_t index) const    { return _texts[index].revert != nullptr; }

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

    uint8_t           _alertSlot       = 0;
    DeviceAlertTarget _alertTextTarget = DeviceAlertTarget::ALL;

private:
    struct TextSlot {
        char          base[TEXT_BUF] = {};
        TimerHandle_t revert         = nullptr;
    };
    TextSlot _texts[TEXT_COUNT];

    struct RevertCtx { IDisplayConsumer* self; };
    static void revertTimerCb(TimerHandle_t h);
};
