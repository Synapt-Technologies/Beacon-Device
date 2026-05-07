#pragma once
#include "interfaces.h"
#include "led_layout.h"
#include "led_pattern.h"
#include "driver/gpio.h"
#include "led_strip.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

class CompositeLedController : public ILedController {
public:
    CompositeLedController(led_strip_handle_t strip,
                           gpio_num_t rPin, gpio_num_t gPin, gpio_num_t bPin,
                           int ledCount,
                           LedLayout& layout,
                           uint8_t brightness = 255);

    void setColor(uint8_t r, uint8_t g, uint8_t b)                          override;
    void setColorForTarget(LedTarget t, uint8_t r, uint8_t g, uint8_t b)    override;
    void runPatternForTarget(LedTarget t,
                             const PatternStep* steps, int count,
                             bool repeat,
                             uint8_t tallyR, uint8_t tallyG, uint8_t tallyB) override;
    void updatePatternTallyColor(uint8_t r, uint8_t g, uint8_t b)           override;
    void stopPattern(LedTarget t)                                            override;
    void setBrightness(uint8_t brightness)                                   override;

private:
    led_strip_handle_t m_strip;
    gpio_num_t         m_rPin, m_gPin, m_bPin;
    int                m_ledCount;
    LedLayout&         m_layout;
    uint8_t            m_brightness;

    // Per-target pattern task handles (NONE slot unused)
    TaskHandle_t m_patternTask[4] = {};   // indexed by LedTarget

    // Tally color shared with pattern tasks (written under brief critical section)
    volatile uint8_t m_tallyR = 0, m_tallyG = 0, m_tallyB = 0;

    void applySection(const LedSection& s, uint8_t r, uint8_t g, uint8_t b);
    uint8_t scale(uint8_t value) const;
    void stopTaskForTarget(LedTarget t);

    struct PatternTaskArg {
        CompositeLedController* ctrl;
        LedTarget               target;
        const PatternStep*      steps;
        int                     count;
        bool                    repeat;
        volatile uint8_t        tallyR, tallyG, tallyB;
    };

    static void patternTaskFn(void* arg);
};
