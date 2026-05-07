#include "led_layout.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

LedLayout::LedLayout()
{
    setDefault(21);
}

bool LedLayout::parse(const char* descriptor, int defaultLedCount)
{
    if (!descriptor || descriptor[0] == '\0') {
        setDefault(defaultLedCount);
        return true;
    }

    m_count = 0;

    // Work on a mutable copy
    char buf[128];
    strncpy(buf, descriptor, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    char* token = strtok(buf, ";");
    while (token && m_count < MAX_SECTIONS) {
        if (token[0] == 'F' && token[1] == '=') {
            LedSection s;
            s.isFixed = true;
            s.start   = 0;
            s.count   = 0;
            s.target  = parseTarget(token + 2);
            m_sections[m_count++] = s;
        } else if (token[0] == 'S' && token[1] == '=') {
            // S=start,count,target
            char inner[64];
            strncpy(inner, token + 2, sizeof(inner) - 1);
            inner[sizeof(inner) - 1] = '\0';

            char* p   = strtok(inner, ",");
            char* q   = p ? strtok(nullptr, ",") : nullptr;
            char* tgt = q ? strtok(nullptr, ",") : nullptr;
            if (!p || !q || !tgt) { token = strtok(nullptr, ";"); continue; }

            LedSection s;
            s.isFixed = false;
            s.start   = (uint8_t)atoi(p);
            s.count   = (uint8_t)atoi(q);
            s.target  = parseTarget(tgt);
            m_sections[m_count++] = s;
        }
        token = strtok(nullptr, ";");
    }

    if (m_count == 0) {
        setDefault(defaultLedCount);
        return false;
    }
    return true;
}

const char* LedLayout::toString(char* buf, size_t maxLen) const
{
    buf[0] = '\0';
    for (int i = 0; i < m_count; i++) {
        char part[32];
        const LedSection& s = m_sections[i];
        if (s.isFixed) {
            snprintf(part, sizeof(part), "%sF=%s", i ? ";" : "", targetName(s.target));
        } else {
            snprintf(part, sizeof(part), "%sS=%d,%d,%s",
                     i ? ";" : "", s.start, s.count, targetName(s.target));
        }
        strncat(buf, part, maxLen - strlen(buf) - 1);
    }
    return buf;
}

// ── private ──────────────────────────────────────────────────────────────────

LedTarget LedLayout::parseTarget(const char* str)
{
    if (!str) return LedTarget::ALL;
    if (strcmp(str, "OPERATOR") == 0) return LedTarget::OPERATOR;
    if (strcmp(str, "TALENT")   == 0) return LedTarget::TALENT;
    if (strcmp(str, "NONE")     == 0) return LedTarget::NONE;
    return LedTarget::ALL;
}

const char* LedLayout::targetName(LedTarget t)
{
    switch (t) {
    case LedTarget::OPERATOR: return "OPERATOR";
    case LedTarget::TALENT:   return "TALENT";
    case LedTarget::NONE:     return "NONE";
    default:                  return "ALL";
    }
}

void LedLayout::setDefault(int ledCount)
{
    m_count = 0;
    m_sections[m_count++] = {false, 0, (uint8_t)ledCount, LedTarget::ALL};
    m_sections[m_count++] = {true,  0, 0,                 LedTarget::ALL};
}
