#pragma once

enum class DeviceAlertType {
    COLOR = 0,
    TEXT  = 1,
    BOTH  = 2,
};

struct ColorAlertPattern {
    uint32_t speedMs;
    const TallyState (*patterns)[8];  // [variant][step], up to 8 steps
    uint8_t variantCount;
    uint8_t patternLen;
};