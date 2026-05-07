#pragma once

#include <stdint.h>

enum class DeviceType : uint8_t {
    SINGLE_TOPIC, // one MQTT subscription → all consumers show the same tally state
    MULTI_TOPIC,  // one MQTT subscription per consumer → each shows its own tally state
};

// TODO: ConsumerCount -> List of labels
struct DeviceProfile {
    DeviceType  deviceType    = DeviceType::SINGLE_TOPIC;
    char        model[32]     = "Beacon Satellite";
    uint8_t     consumerCount = 3; // Max 8, as defined by the Settings struct and enforced in the Orchestrator.
};
