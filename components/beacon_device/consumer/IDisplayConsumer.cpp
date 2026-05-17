#include "consumer/IDisplayConsumer.hpp"

IDisplayConsumer::~IDisplayConsumer() {
    for (auto& s : _texts) {
        if (s.revert) {
            xTimerDelete(s.revert, portMAX_DELAY);
            s.revert = nullptr;
        }
    }
}

void IDisplayConsumer::ApplyText(const char* text, uint8_t index, uint32_t timeout) {
    if (index >= TEXT_COUNT) return;
    TextSlot& s = _texts[index];

    if (s.revert) {
        xTimerDelete(s.revert, 0);
        s.revert = nullptr;
    }

    if (timeout == 0) {
        strncpy(s.base, text ? text : "", TEXT_BUF - 1);
        s.base[TEXT_BUF - 1] = '\0';
        onTextChanged(index, s.base);
    } else {
        onTextChanged(index, text ? text : "");
        auto* ctx = new RevertCtx{this, index};
        s.revert = xTimerCreate("txt_rv", pdMS_TO_TICKS(timeout), pdFALSE, ctx, revertTimerCb);
        if (s.revert) {
            xTimerStart(s.revert, 0);
        } else {
            onTextChanged(index, s.base);
            delete ctx;
        }
    }
}

void IDisplayConsumer::clearAlertText(uint8_t index) {
    if (index >= TEXT_COUNT) return;
    TextSlot& s = _texts[index];
    if (s.revert) {
        xTimerDelete(s.revert, 0);
        s.revert = nullptr;
    }
    onTextChanged(index, s.base);
}

void IDisplayConsumer::revertTimerCb(TimerHandle_t h) {
    auto* ctx = static_cast<RevertCtx*>(pvTimerGetTimerID(h));
    ctx->self->_texts[ctx->index].revert = nullptr;
    ctx->self->onTextChanged(ctx->index, ctx->self->_texts[ctx->index].base);
    xTimerDelete(h, 0);
    delete ctx;
}
