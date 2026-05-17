#pragma once

#include "consumer/IConsumer.hpp"
#include "consumer/ITextRenderer.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdint.h>

// Owns canonical tally state and runs a single alert task shared across all registered
// consumers, keeping their outputs in sync. Display consumers also register as
// ITextRenderer to receive alert text dispatches.
class ConsumerGroup {
public:
    static constexpr uint8_t MAX_SECTIONS      = 8;
    static constexpr uint8_t MAX_TEXT_RENDERERS = 4;
    static constexpr uint8_t MAX_CONSUMERS     = 8;

    ConsumerGroup() = default;
    ~ConsumerGroup();

    // Register a consumer: calls consumer->registerWith(*this) then stores it for init().
    void addConsumer(IConsumer* consumer);

    // Called from IConsumer::registerWith implementations.
    void addSection(ISection* section);
    void addTextRenderer(ITextRenderer* renderer);

    void init();

    // ── State ──────────────────────────────────────────────────────────
    void setState(TallyState state);
    void setMasterBrightness(uint8_t brightness);

    // ── Color alerts (independent of text alerts) ──────────────────────
    void setColorAlert(DeviceAlertAction action, DeviceAlertTarget target, uint32_t timeoutMs);
    void clearColorAlert();

    // ── Text alerts (independent of color alerts) ──────────────────────
    // Dispatches setAlertText to targeted ITextRenderers; they handle slot + revert internally.
    // TODO Allow text index.
    void setTextAlert(const char* text, DeviceAlertTarget target, uint32_t timeoutMs);
    void clearTextAlert(DeviceAlertTarget target = DeviceAlertTarget::ALL);

    // ── Slot text (device name, labels, etc.) ──────────────────────────
    void setText(uint8_t slot, const char* text, uint32_t timeoutMs = 0);
    void clearText(uint8_t slot);

private:
    ISection*       _sections[MAX_SECTIONS]           = {};
    ITextRenderer*  _textRenderers[MAX_TEXT_RENDERERS] = {};
    IConsumer*      _allConsumers[MAX_CONSUMERS]       = {};
    uint8_t         _sectionCount     = 0;
    uint8_t         _textCount        = 0;
    uint8_t         _consumerCount    = 0;

    TallyState   _state            = TallyState::NONE;
    uint8_t      _masterBrightness = 255;
    TaskHandle_t _colorAlertTask   = nullptr;

    void applyCurrentState();

    // ── Color alert task ───────────────────────────────────────────────
    struct AlertTaskArg {
        ConsumerGroup*    group;
        DeviceAlertAction action;
        DeviceAlertTarget target;
        uint32_t          timeoutMs;
    };

    static void colorAlertTask(void* arg);
    void startColorAlertTask(DeviceAlertAction action, DeviceAlertTarget target, uint32_t timeoutMs);

    static bool targetMatches(DeviceAlertTarget rendererTarget, DeviceAlertTarget alertTarget) {
        return rendererTarget == DeviceAlertTarget::ALL
            || alertTarget    == DeviceAlertTarget::ALL
            || rendererTarget == alertTarget;
    }
};
