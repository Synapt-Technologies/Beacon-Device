#pragma once

#include "types/AlertTypes.hpp"


class TallyHandler {
public:
    static constexpr uint8_t MAX_CONSUMERS = 8;

    ~TallyHandler() = default;

    void addConsumer(IConsumer* consumer) {
        if (_consumerCount >= MAX_CONSUMERS) {
            ESP_LOGW("TallyHandler", "Max consumers reached, cannot add more");
            return;    
        }
        _consumers[_consumerCount++] = consumer;
    }

    void setState(const TallyState state) {
        _state = state;

        // if alert task active, store without rendering. AlertTask will call setAlertStep.
        if (_alertTask) {
            applyState(state, false); 
            xTaskNotify(_alertTask, 2, eSetBits);
        }
        else {
            applyState(state);
        }


    }

    void setColorAlert(DeviceAlertAction action, DeviceAlertTarget target, uint32_t timeout) {
        if (action == DeviceAlertAction::CLEAR) {
            this->stopAlertTask();
        } else {
            this->startAlertTask(action, target, timeout);
        }
    }


protected:
    IConsumer* _consumers[MAX_CONSUMERS]    = {};
    uint8_t    _consumerCount               = 0;


    TallyState      _state      = TallyState::NONE;
    TaskHandle_t    _alertTask  = {};

    void applyState(TallyState state, bool apply = true) {
        
        for (uint8_t i = 0; i < _consumerCount; i++)
            _consumers[i]->setState(state, apply);
    };

    static const ColorAlertPattern* getAlertPattern(DeviceAlertAction action);

    static uint32_t getAlertStepLength(DeviceAlertAction action) {
        const ColorAlertPattern* p = getAlertPattern(action);
        return p ? p->speedMs : 400;
    }
    static uint8_t getAlertStepCount(DeviceAlertAction action) {
        const ColorAlertPattern* p = getAlertPattern(action);
        return p ? p->patternLen : 1;
    }

    void setAlertStep(DeviceAlertAction action, DeviceAlertTarget target, uint8_t step) {
        
        const ColorAlertPattern* pattern = getAlertPattern(action);
        if (!pattern) return;

        TallyState step_variants[8]; // TODO check if this is the right limit. using pattern->varientCount here is not allowed in standard c++
        for (uint8_t v = 0; v < pattern->variantCount; v++) {
            TallyState s = pattern->patterns[v][step % pattern->patternLen];
            step_variants[v] = (s == TallyState::NONE) ? _state : s;
        }

        for (uint8_t i = 0; i < _consumerCount; i++)
            _consumers[i]->setAlertStep(target, step_variants, pattern->variantCount);
    }

    void startAlertTask(DeviceAlertAction action, DeviceAlertTarget target, uint32_t timeout) {
        if (_alertTask) {
            TaskHandle_t h = _alertTask;
            _alertTask = nullptr;
            xTaskNotify(h, 1, eSetBits);
            applyState(_state);
        }

        auto* arg    = new AlertTaskArg;
        arg->self    = this;
        arg->action  = action;
        arg->target  = target;
        arg->timeout = timeout;

        xTaskCreate(alertTask, "led_pat", 4096, arg, 18, &_alertTask);
    }

    void stopAlertTask() {
        if (!_alertTask) return;
        TaskHandle_t h = _alertTask;
        _alertTask = nullptr;
        xTaskNotify(h, 1, eSetBits);
    }

    struct AlertTaskArg {
        TallyHandler* self;
        DeviceAlertAction action;
        DeviceAlertTarget target;
        uint32_t timeout;
    };

    static void alertTask(void* arg) {
        auto* a = static_cast<AlertTaskArg*>(arg);
        TallyHandler* self = a->self;

        const bool infinite_timeout = (a->timeout == 0);
        TickType_t parsed_timeout = pdMS_TO_TICKS(a->timeout);
        TickType_t start_time = xTaskGetTickCount();

        uint8_t step = 0;

        TickType_t step_ticks = pdMS_TO_TICKS(self->getAlertStepLength(a->action));
        uint8_t step_count = self->getAlertStepCount(a->action);

        bool stop = false;
        while (!stop && (xTaskGetTickCount() < start_time + parsed_timeout || infinite_timeout)) {
            self->setAlertStep(a->action, a->target, step);

            TickType_t step_deadline = xTaskGetTickCount() + step_ticks;
            if (!infinite_timeout && step_deadline > start_time + parsed_timeout)
                step_deadline = start_time + parsed_timeout;

            while (xTaskGetTickCount() < step_deadline) {
                uint32_t notif = 0;
                xTaskNotifyWait(0, ULONG_MAX, &notif, step_deadline - xTaskGetTickCount());
                if (notif & 1) { stop = true; break; }
                if (notif & 2) self->setAlertStep(a->action, a->target, step);
            }

            step = (step + 1) % step_count;
        }

        bool replaced = (self->_alertTask != nullptr) && (self->_alertTask != xTaskGetCurrentTaskHandle());
        if (!replaced) {
            self->_alertTask = nullptr;
            self->applyState(self->_state);
        }
        delete a;
        vTaskDelete(nullptr);
    }
};