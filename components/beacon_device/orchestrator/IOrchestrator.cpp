#include "orchestrator/IOrchestrator.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

void IOrchestrator::start() {
    xTaskCreate([](void* arg) {
        static_cast<IOrchestrator*>(arg)->doStart();
        vTaskDelete(nullptr);
    }, "beacon_start", STARTUP_STACK_SIZE, this, 5, nullptr);
}
