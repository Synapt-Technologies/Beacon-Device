#pragma once

#include <stdint.h>
#include "types/TallyTypes.hpp"

struct Settings {

    struct Network {
        char ssid[64]     = {};
        char password[64] = {};
    } network;

    struct Beacon {
        char mqttUrl[128]      = {};
        char consumerId[8][32] = { "aedes" }; // [0] defaults to "aedes", rest empty
        char deviceId[8][48]   = {};           // empty = auto-derive from MAC at runtime
    } beacon;

    struct Display {
        uint8_t           brightness[8]   = {255, 255, 255, 255, 255, 255, 255, 255};
        // TODO: Add some sort of alert target / flip here?
    } display;

    struct Runtime { // TODO: Server side loading and saving of this, and UI to edit it
        uint8_t brightness = 255; // Master Brightness
        struct Name {
            char shortName[32] = {"SAT"};
            char longName[32]  = {"Beacon Satelite"};
        } name[8];
        TallyState state_on_disconnect = TallyState::WARNING;
    } runtime;

    char deviceName[32] = "Beacon Satellite";
};
