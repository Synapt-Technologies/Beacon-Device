#pragma once
#include <stdint.h>

enum class LedTarget : uint8_t { NONE = 0, OPERATOR = 1, TALENT = 2, ALL = 3 };

struct LedSection {
    bool      isFixed;       // true = the fixed RGB LED; false = strip segment
    uint8_t   start, count;  // strip segment range (ignored when isFixed)
    LedTarget target;
};

// Parses and stores the compact LED layout descriptor.
//
// Format:  F=<target>[;S=<start>,<count>,<target>]...
// Example: "F=OPERATOR;S=0,11,TALENT;S=11,10,OPERATOR"
// Targets: NONE | OPERATOR | TALENT | ALL
//
// Default when descriptor is empty or null:
//   one strip section covering all LEDs with target ALL
//   one fixed LED with target ALL
class LedLayout {
public:
    static constexpr int MAX_SECTIONS = 8;

    LedLayout();

    // Parse descriptor; returns true on success. Resets to default on failure.
    bool parse(const char* descriptor, int defaultLedCount = 21);

    int              sectionCount()  const { return m_count; }
    const LedSection& section(int i) const { return m_sections[i]; }

    // Serialise back to the compact format (for display / round-trip).
    // Writes into buf (maxLen bytes) and returns buf.
    const char* toString(char* buf, size_t maxLen) const;

private:
    LedSection m_sections[MAX_SECTIONS];
    int        m_count = 0;

    static LedTarget parseTarget(const char* str);
    static const char* targetName(LedTarget t);
    void setDefault(int ledCount);
};
