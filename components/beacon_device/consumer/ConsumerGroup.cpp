#include "consumer/ConsumerGroup.hpp"
#include <climits>

ConsumerGroup::~ConsumerGroup() {
    clearColorAlert();
}

void ConsumerGroup::addConsumer(IConsumer* consumer) {
    consumer->registerWith(*this);
    if (_consumerCount < MAX_CONSUMERS)
        _allConsumers[_consumerCount++] = consumer;
}

void ConsumerGroup::addSection(ISection* section) {
    if (_sectionCount < MAX_SECTIONS)
        _sections[_sectionCount++] = section;
}

void ConsumerGroup::addTextRenderer(ITextRenderer* renderer) {
    if (_textCount < MAX_TEXT_RENDERERS)
        _textRenderers[_textCount++] = renderer;
}

void ConsumerGroup::init() {
    for (uint8_t i = 0; i < _consumerCount; i++)
        _allConsumers[i]->init();
}

// ── State ──────────────────────────────────────────────────────────────

void ConsumerGroup::setState(TallyState state) {
    _state = state;
    if (_colorAlertTask) {
        xTaskNotify(_colorAlertTask, 2, eSetBits);
        return;
    }
    applyCurrentState();
}

// TODO: Trim brightness
void ConsumerGroup::setMasterBrightness(uint8_t brightness) {
    _masterBrightness = brightness;
    for (uint8_t i = 0; i < _sectionCount; i++)
        _sections[i]->setBrightness(brightness);
    if (!_colorAlertTask)
        applyCurrentState();
}

void ConsumerGroup::applyCurrentState() {
    for (uint8_t i = 0; i < _sectionCount; i++)
        _sections[i]->applyColor(_state);
}

// ── Color alerts ───────────────────────────────────────────────────────

void ConsumerGroup::setColorAlert(DeviceAlertAction action, DeviceAlertTarget target, uint32_t timeoutMs) {
    startColorAlertTask(action, target, timeoutMs);
}

void ConsumerGroup::clearColorAlert() {
    if (!_colorAlertTask) return;
    TaskHandle_t h = _colorAlertTask;
    _colorAlertTask = nullptr;
    xTaskNotify(h, 1, eSetBits);
}

void ConsumerGroup::startColorAlertTask(DeviceAlertAction action, DeviceAlertTarget target, uint32_t timeoutMs) {
    if (_colorAlertTask) {
        TaskHandle_t h = _colorAlertTask;
        _colorAlertTask = nullptr;
        xTaskNotify(h, 1, eSetBits);
        applyCurrentState();
    }

    auto* arg       = new AlertTaskArg;
    arg->group      = this;
    arg->action     = action;
    arg->target     = target;
    arg->timeoutMs  = timeoutMs;

    xTaskCreate(colorAlertTask, "grp_alert", 4096, arg, 18, &_colorAlertTask);
}

void ConsumerGroup::colorAlertTask(void* arg) {
    auto* a = static_cast<AlertTaskArg*>(arg);
    ConsumerGroup* self = a->group;

    const IConsumer::AlertPattern* p = IConsumer::getAlertPattern(a->action);
    const uint8_t  stepCount  = p ? p->patternLen : 1;
    const TickType_t stepTicks = pdMS_TO_TICKS(p ? p->speedMs : 400);

    const bool     infinite   = (a->timeoutMs == 0);
    const TickType_t startTime = xTaskGetTickCount();
    const TickType_t endTime   = startTime + pdMS_TO_TICKS(a->timeoutMs);

    uint8_t step = 0;
    bool stop = false;

    while (!stop && (infinite || xTaskGetTickCount() < endTime)) {
        for (uint8_t i = 0; i < self->_sectionCount; i++)
            self->_sections[i]->applyAlertStep(a->action, a->target, step, self->_state);

        TickType_t deadline = xTaskGetTickCount() + stepTicks;
        if (!infinite && deadline > endTime) deadline = endTime;

        while (xTaskGetTickCount() < deadline) {
            uint32_t notif = 0;
            xTaskNotifyWait(0, ULONG_MAX, &notif, deadline - xTaskGetTickCount());
            if (notif & 1) { stop = true; break; }
            if (notif & 2) {
                // State changed during alert — re-apply current step with new fallback.
                for (uint8_t i = 0; i < self->_sectionCount; i++)
                    self->_sections[i]->applyAlertStep(a->action, a->target, step, self->_state);
            }
        }
        step = (step + 1) % stepCount;
    }

    // Restore tally state unless a replacement task already started.
    const bool replaced = (self->_colorAlertTask != nullptr)
                       && (self->_colorAlertTask != xTaskGetCurrentTaskHandle());
    if (!replaced) {
        self->_colorAlertTask = nullptr;
        self->applyCurrentState();
    }

    delete a;
    vTaskDelete(nullptr);
}

// ── Text alerts ────────────────────────────────────────────────────────

void ConsumerGroup::setTextAlert(const char* text, DeviceAlertTarget target, uint32_t timeoutMs) {
    for (uint8_t i = 0; i < _textCount; i++) {
        if (targetMatches(_textRenderers[i]->alertTextTarget(), target))
            _textRenderers[i]->setAlertText(text, timeoutMs);
    }
}

void ConsumerGroup::clearTextAlert(DeviceAlertTarget target) {
    for (uint8_t i = 0; i < _textCount; i++) {
        if (targetMatches(_textRenderers[i]->alertTextTarget(), target))
            _textRenderers[i]->clearAlertText();
    }
}

// ── Slot text ─────────────────────────────────────────────────────────

void ConsumerGroup::setText(uint8_t slot, const char* text, uint32_t timeoutMs) {
    for (uint8_t i = 0; i < _textCount; i++)
        _textRenderers[i]->setSlotText(slot, text, timeoutMs);
}

void ConsumerGroup::clearText(uint8_t slot) {
    for (uint8_t i = 0; i < _textCount; i++)
        _textRenderers[i]->clearSlotText(slot);
}
