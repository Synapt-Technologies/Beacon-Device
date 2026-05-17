#pragma once

#include "types/TallyTypes.hpp"
#include "types/ColorTypes.hpp"
#include "mapper/ITallyColorMapper.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdint.h>

class IDisplayConsumer; // forward declaration for asDisplay()

class IConsumer {
public:
    // TODO: Change the way colormapper is included. Should probably be default, and overwriteble.
    IConsumer(ITallyColorMapper& colorMapper) : _colorMapper(colorMapper) {}
    virtual ~IConsumer() = default;

    virtual IDisplayConsumer* asDisplay() { return nullptr; }

    virtual void init() {}

    void setState(const TallyState state, bool apply = true) {
        _state = state;
        if (apply) {
            applyState(state);
        }
    }

    // TODO: add zones with callback and imp here, or add setZone, or similar.
    virtual void setAlertStep(DeviceAlertTarget target, const TallyState* step_variants, uint8_t variantCount) = 0;


    void setBrightness(uint8_t brightness) {
        _brightness = brightness;
        applyBrightness(brightness);
        this->applyState(this->_state);
    }

protected:
    uint8_t    _brightness = 255;
    TallyState _state      = TallyState::NONE;
    ITallyColorMapper& _colorMapper;
    // TODO: Wrapper functions around colormapper?

    virtual void applyState(TallyState state) = 0;

    virtual void applyBrightness(uint8_t /*brightness*/) {}


    bool doesTargetMatch(DeviceAlertTarget consumerTarget, DeviceAlertTarget alertTarget) const {
        return consumerTarget == DeviceAlertTarget::ALL
            || alertTarget == DeviceAlertTarget::ALL
            || consumerTarget == alertTarget;
    }
};
