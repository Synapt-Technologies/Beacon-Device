#pragma once
#include <stdint.h>

// A single step in a LED pattern.
// useTallyColor=true: ignore r/g/b and use the live tally state color instead (null).
struct PatternStep {
    uint8_t  r, g, b;
    uint16_t hold_ms;
    bool     useTallyColor = false;
};

constexpr PatternStep TallyStep(uint16_t hold_ms)
{
    return {0, 0, 0, hold_ms, true};
}

// White strobe — finite, no repeat (IDENT)
constexpr PatternStep PATTERN_IDENT[] = {
    {255,255,255, 80}, {0,0,0, 80},
    {255,255,255, 80}, {0,0,0, 80},
    {255,255,255, 80}, {0,0,0, 80},
    {255,255,255, 80}, {0,0,0, 80},
    {255,255,255, 80}, {0,0,0,400},
};

// Tally color flash — repeating (PRIO)
constexpr PatternStep PATTERN_PRIO[] = {
    TallyStep(100),
    {0, 0, 0, 100},
};

// Slow blue pulse — repeating (INFO)
constexpr PatternStep PATTERN_INFO[] = {
    {0, 150, 255, 500},
    {0,   0,   0, 500},
};

template<typename T, int N>
constexpr int countof(T(&)[N]) { return N; }
