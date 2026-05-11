#pragma once

#include "orchestrator/IOrchestrator.hpp"

// TODO: implement multi-topic orchestrator.
// Each consumer has its own MQTT subscription and displays an independent tally state.
class NodeOrchestrator : public IOrchestrator {
public:
    void stop() override {}

protected:
    void doStart() override {}
};
