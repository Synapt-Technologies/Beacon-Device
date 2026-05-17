#pragma once

#include "types/AlertTypes.hpp"
#include "types/TallyTypes.hpp"

class IColorAlertPatternMap {
public:
    virtual ~IColorAlertPatternMap() = default;

    virtual const ColorAlertPattern* getPattern(DeviceAlertAction action) const = 0;

    uint32_t getAlertStepLength(DeviceAlertAction action) const {
        const ColorAlertPattern* p = getPattern(action);
        return p ? p->speedMs : 400;
    }

    uint8_t getAlertStepCount(DeviceAlertAction action) const {
        const ColorAlertPattern* p = getPattern(action);
        return p ? p->patternLen : 1; // TODO: Right fallback?
    }

};
