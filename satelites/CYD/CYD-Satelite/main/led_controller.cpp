#include "led_controller.h"
#include "esp_log.h"

static const char* TAG = "LedCtrl";

CompositeLedController::CompositeLedController(led_strip_handle_t strip,
                                               gpio_num_t rPin,
                                               gpio_num_t gPin,
                                               gpio_num_t bPin,
                                               int ledCount,
                                               LedLayout& layout,
                                               uint8_t brightness)
    : m_strip(strip), m_rPin(rPin), m_gPin(gPin), m_bPin(bPin),
      m_ledCount(ledCount), m_layout(layout), m_brightness(brightness)
{
    gpio_set_direction(m_rPin, GPIO_MODE_OUTPUT);
    gpio_set_direction(m_gPin, GPIO_MODE_OUTPUT);
    gpio_set_direction(m_bPin, GPIO_MODE_OUTPUT);
    setColor(0, 0, 0);
}

// ── Public interface ──────────────────────────────────────────────────────────

void CompositeLedController::setColor(uint8_t r, uint8_t g, uint8_t b)
{
    setColorForTarget(LedTarget::ALL, r, g, b);
}

void CompositeLedController::setColorForTarget(LedTarget target,
                                               uint8_t r, uint8_t g, uint8_t b)
{
    for (int i = 0; i < m_layout.sectionCount(); i++) {
        const LedSection& s = m_layout.section(i);
        // A section matches if its target is the requested target, or either side is ALL
        if (s.target == target || s.target == LedTarget::ALL || target == LedTarget::ALL)
            applySection(s, r, g, b);
    }
}

void CompositeLedController::runPatternForTarget(LedTarget target,
                                                 const PatternStep* steps, int count,
                                                 bool repeat,
                                                 uint8_t tallyR, uint8_t tallyG, uint8_t tallyB)
{
    stopTaskForTarget(target);

    auto* arg       = new PatternTaskArg;
    arg->ctrl       = this;
    arg->target     = target;
    arg->steps      = steps;
    arg->count      = count;
    arg->repeat     = repeat;
    arg->tallyR     = tallyR;
    arg->tallyG     = tallyG;
    arg->tallyB     = tallyB;

    TaskHandle_t h = nullptr;
    xTaskCreate(patternTaskFn, "led_pat", 2048, arg, 18, &h);
    m_patternTask[static_cast<int>(target)] = h;
}

void CompositeLedController::updatePatternTallyColor(uint8_t r, uint8_t g, uint8_t b)
{
    m_tallyR = r;
    m_tallyG = g;
    m_tallyB = b;
    // Each running task reads from its own arg struct; update all of them
    for (int t = 1; t <= 3; t++) {
        // We can't safely write to the task's arg directly without a pointer.
        // Instead, tasks read from m_tallyR/G/B via ctrl pointer — see patternTaskFn.
    }
    // (tasks read ctrl->m_tallyR/G/B directly for useTallyColor steps)
}

void CompositeLedController::stopPattern(LedTarget target)
{
    if (target == LedTarget::ALL) {
        stopTaskForTarget(LedTarget::OPERATOR);
        stopTaskForTarget(LedTarget::TALENT);
        stopTaskForTarget(LedTarget::ALL);
    } else {
        stopTaskForTarget(target);
    }
}

void CompositeLedController::setBrightness(uint8_t brightness)
{
    m_brightness = brightness;
}

// ── Private helpers ───────────────────────────────────────────────────────────

void CompositeLedController::applySection(const LedSection& s,
                                          uint8_t r, uint8_t g, uint8_t b)
{
    const uint8_t sr = scale(r);
    const uint8_t sg = scale(g);
    const uint8_t sb = scale(b);

    if (s.isFixed) {
        // Active-low: low = on
        gpio_set_level(m_rPin, sr > 0 ? 0 : 1);
        gpio_set_level(m_gPin, sg > 0 ? 0 : 1);
        gpio_set_level(m_bPin, sb > 0 ? 0 : 1);
    } else {
        int end = s.start + s.count;
        if (end > m_ledCount) end = m_ledCount;
        if (sr == 0 && sg == 0 && sb == 0) {
            // Clear only the affected pixels then refresh
            for (int i = s.start; i < end; i++)
                led_strip_set_pixel(m_strip, i, 0, 0, 0);
        } else {
            for (int i = s.start; i < end; i++)
                led_strip_set_pixel(m_strip, i, sr, sg, sb);
        }
        led_strip_refresh(m_strip);
    }
}

uint8_t CompositeLedController::scale(uint8_t value) const
{
    return static_cast<uint8_t>((static_cast<uint16_t>(value) * m_brightness) / 255u);
}

void CompositeLedController::stopTaskForTarget(LedTarget t)
{
    int idx = static_cast<int>(t);
    if (idx < 0 || idx > 3) return;
    if (!m_patternTask[idx]) return;

    xTaskNotifyGive(m_patternTask[idx]);
    // Give the task a moment to exit; it will clear the handle itself
    vTaskDelay(pdMS_TO_TICKS(20));
    m_patternTask[idx] = nullptr;
}

void CompositeLedController::patternTaskFn(void* arg)
{
    auto* a = static_cast<PatternTaskArg*>(arg);
    CompositeLedController* ctrl = a->ctrl;

    do {
        for (int i = 0; i < a->count; i++) {
            const PatternStep& step = a->steps[i];

            uint8_t r, g, b;
            if (step.useTallyColor) {
                r = ctrl->m_tallyR;
                g = ctrl->m_tallyG;
                b = ctrl->m_tallyB;
            } else {
                r = step.r; g = step.g; b = step.b;
            }
            ctrl->setColorForTarget(a->target, r, g, b);

            // Wait for the step duration; break out early if notified to stop
            if (ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(step.hold_ms)) > 0)
                goto done;
        }
    } while (a->repeat);

done:
    // Restore off when pattern ends naturally or is cancelled
    ctrl->setColorForTarget(a->target, 0, 0, 0);

    int idx = static_cast<int>(a->target);
    if (idx >= 0 && idx <= 3) ctrl->m_patternTask[idx] = nullptr;

    delete a;
    vTaskDelete(nullptr);
}
