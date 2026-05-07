#pragma once

#include "types/TallyTypes.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdint.h>
#include <cmath>

class IConsumer {
public:
    virtual ~IConsumer() = default;

    // Sets all LEDs regardless of target
    virtual void setState(const TallyState state) {
        this->_state = state;

        if (_alertTask) return;
        this->applyState(state);
    }

    virtual void setAlert(DeviceAlertAction action, DeviceAlertTarget target, uint32_t timeout) {
        if (action == DeviceAlertAction::CLEAR) {
            this->stopAlertTask();
        } else {
            this->startAlertTask(action, target, timeout);
        }
    };

    virtual void setBrightness(uint8_t brightness) {
        _brightness = brightness;
        rebuildLut();
        this->applyState(this->_state);
    }

protected:
    uint8_t _brightness = 255;
    TallyState _state = TallyState::NONE;

    TaskHandle_t _alertTask = {};
    uint8_t _lut[256] = {};

    void rebuildLut() {
        _lut[0] = 0;
        for (int i = 1; i < 256; i++) {
            float t = (i - 1) / 254.0f;
            float out = 255.0f * powf(t, 2.8f) * _brightness / 255.0f;
            _lut[i] = static_cast<uint8_t>(out + 0.5f);
        }
    }

    uint8_t scale_brightness(uint8_t value) const {
        return _lut[value];
    }

    virtual void setColor(uint8_t r, uint8_t g, uint8_t b) = 0;
    
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

    void applyState(TallyState state) {
        uint8_t r, g, b;
        stateToColor(state, r, g, b);
        this->setColor(r, g, b);
    }

    struct AlertTaskArg {
        IConsumer* self;
        DeviceAlertAction action;
        DeviceAlertTarget target;
        uint32_t timeout;
    };

    static void alertTask(void* arg) {
        auto* a = static_cast<AlertTaskArg*>(arg);
        IConsumer* self = a->self;

        const bool infinite_timeout = (a->timeout == 0);
        TickType_t parsed_timeout = pdMS_TO_TICKS(a->timeout);
        TickType_t start_time = xTaskGetTickCount();

        uint8_t step = 0;

        TickType_t step_ticks = pdMS_TO_TICKS(self->getAlertStepLength(a->action));
        uint8_t step_count = self->getAlertStepCount(a->action);

        while (xTaskGetTickCount() < start_time + parsed_timeout || infinite_timeout) {
            self->setAlertStep(a->action, a->target, step++);

            if (step >= step_count) step = 0;

            TickType_t parsed_delay =
                (xTaskGetTickCount() + step_ticks > start_time + parsed_timeout) && !infinite_timeout ?
                (start_time + parsed_timeout - xTaskGetTickCount()) :
                step_ticks;

            if (ulTaskNotifyTake(pdTRUE, parsed_delay)) 
                break;
        }

        self->_alertTask = nullptr;
        self->applyState(self->_state);
        delete a;
        vTaskDelete(nullptr);
    }

    virtual uint32_t getAlertStepLength(DeviceAlertAction action) = 0;
    virtual uint8_t getAlertStepCount(DeviceAlertAction action) = 0;
    virtual void setAlertStep(DeviceAlertAction action, DeviceAlertTarget target, uint8_t step) = 0;

    void startAlertTask(DeviceAlertAction action, DeviceAlertTarget target, uint32_t timeout) {

        if (_alertTask) {
            TaskHandle_t h = _alertTask;
            _alertTask = nullptr;
            xTaskNotifyGive(h);
        }

        auto* arg    = new AlertTaskArg;
        arg->self    = this;
        arg->action  = action;
        arg->target  = target;
        arg->timeout = timeout;

        xTaskCreate(alertTask, "led_pat", 4096, arg, 18, &_alertTask);
    };

    void stopAlertTask() {

        if (!_alertTask) return;

        TaskHandle_t h = _alertTask;
        _alertTask = nullptr;
        xTaskNotifyGive(h);
    };



};

class ISmartConsumer : public IConsumer {
public:
    virtual ~ISmartConsumer() = default;

    virtual void setText(const char* text, const uint8_t index, const uint32_t timeout = 0) = 0;
};