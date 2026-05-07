#pragma once

#include <functional>
#include <stdint.h>
#include <cstring>
#include "esp_netif_types.h"

enum class NetworkStatus {
    DISCONNECTED,
    CONNECTING,
    CONNECTED,
    ERROR,
};

class IWifiConnection;

class INetworkConnection {
public:
    using ConnectionCb = std::function<void(NetworkStatus status, esp_ip4_addr_t ip)>;

    INetworkConnection(const char* deviceType = "Beacon_Satellite") {
        std::strncpy(_deviceType, deviceType, sizeof(_deviceType) - 1);
        _deviceType[sizeof(_deviceType) - 1] = '\0';
    }
    virtual ~INetworkConnection() = default;

    virtual void           start()                                      = 0;
    virtual void           stop()                                       = 0;
    virtual NetworkStatus  getStatus()                            const = 0;
    virtual esp_ip4_addr_t getIp()                                const = 0;
    virtual void           setConnectionCallback(ConnectionCb cb)       = 0;

    bool isConnected() const { return getStatus() == NetworkStatus::CONNECTED; }

    virtual IWifiConnection* asWifi() { return nullptr; }

protected:
    char _deviceType[32] = "";
    NetworkStatus  _status = NetworkStatus::DISCONNECTED;
    esp_ip4_addr_t _ip     = {};
};
