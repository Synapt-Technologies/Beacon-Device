#pragma once

#include <functional>
#include <stdint.h>
#include <cstring>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_netif_types.h"
#include "types/TallyTypes.hpp"

enum class BeaconStatus {
    DISCONNECTED,
    CONNECTED,
    ERROR,
};

class IBeaconConnection {
public:
    using TallyCb = std::function<void(TallyState state)>;
    using AlertCb = std::function<void(DeviceAlertType type, DeviceAlertAction action,
                                       DeviceAlertTarget target, uint32_t timeout, const char* text)>;
    using NameCb  = std::function<void(const char* shortName, const char* longName)>; // TODO: More generic runtime callback?
    using ConnectionCb = std::function<void(BeaconStatus status)>;

    virtual ~IBeaconConnection() = default;

    virtual void start() = 0;
    virtual void stop() = 0;

    bool isConnected() const {
        return _connected && (xTaskGetTickCount() - _lastKeepAlive) < pdMS_TO_TICKS(_aliveTimeout);
    };

    virtual void setBaseAddress(const char* url) = 0;

    void setAddress(const char* consumer, const char* device) {
        // TODO check address checking redundancy. Currently it is done in multiple places.
        strncpy(_consumer, consumer ? consumer : "aedes", sizeof(_consumer) - 1);
        _consumer[sizeof(_consumer) - 1] = '\0';
        
        if (device) {
            strncpy(_device, device, sizeof(_device) - 1);
            _device[sizeof(_device) - 1] = '\0';
        } else {
            _device[0] = '\0';
        }
        updateSubscriptions();
    }

    void setTallyCallback(TallyCb cb) { _tallyCb = cb; }
    void setAlertCallback(AlertCb cb) { _alertCb = cb; }
    void setNameCallback(NameCb cb) { _nameCb = cb; }
    void setConnectionCallback(ConnectionCb cb) { _connectionCb = cb; }

    void getConsumerAddress(char* out, int len) const {
        strncpy(out, _consumer, len - 1);
        out[len - 1] = '\0';
    }

    void getDeviceAddress(char* out, int len) const {
        strncpy(out, _device, len - 1);
        out[len - 1] = '\0';
    }

protected:
    TallyCb      _tallyCb;
    AlertCb      _alertCb;
    NameCb       _nameCb;
    ConnectionCb _connectionCb;
    char _consumer[64] = "aedes";
    char _device[64]   = {};

    bool     _connected    = false;
    uint32_t _aliveTimeout = 2000;

    TickType_t _lastKeepAlive = 0;

    void notifyConnectionStatus(BeaconStatus status) {
        if (_connectionCb) _connectionCb(status);
    }

    virtual void updateSubscriptions() = 0;
    virtual void clearSubscriptions() = 0;

};
