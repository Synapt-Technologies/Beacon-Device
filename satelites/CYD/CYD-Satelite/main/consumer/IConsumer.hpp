#pragma once

#include "types/TallyTypes.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdint.h>

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
        this->_brightness = brightness;
        this->applyState(this->_state);
    }

protected:
    uint8_t _brightness = 255;
    TallyState _state = TallyState::NONE;

    TaskHandle_t _alertTask = {}; 

    uint8_t scale_brightness(uint8_t value) const
    {
        return static_cast<uint8_t>((static_cast<uint16_t>(value) * _brightness) / 255u);
    }

    virtual void setColor(uint8_t r, uint8_t g, uint8_t b) = 0;
    
    // Sets the state without saving it to _state
    void applyState(TallyState state) {
        switch (state)
        {

            case TallyState::DANGER:
            case TallyState::WARNING:
                this->setColor(255, 255, 0);
                break;
            case TallyState::INFO:
                this->setColor(0, 0, 255);
                break;
            case TallyState::PREVIEW:
                this->setColor(0, 255, 0);
                break;
            case TallyState::PROGRAM:
                this->setColor(255, 0, 0);
                break;
            case TallyState::NONE:
            default:
                this->setColor(0, 0, 0);
                break;
        }
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

            if (ulTaskNotifyTake(pdTRUE, parsed_delay)) break;
        }

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

        xTaskCreate(alertTask, "led_pat", 2048, arg, 18, &_alertTask);
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