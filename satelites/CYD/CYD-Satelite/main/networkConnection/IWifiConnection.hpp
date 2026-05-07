#pragma once

#include "networkConnection/INetworkConnection.hpp"
#include <stdint.h>

struct WifiScanResult {
    char    ssid[33];
    uint8_t bssid[6];
    uint8_t channel;
    int8_t  rssi;
    uint8_t authmode;
};

class IWifiConnection : public INetworkConnection {
public:
    IWifiConnection(const char* deviceType = "Beacon_Satellite") : INetworkConnection(deviceType) {}
    virtual ~IWifiConnection() = default;

    IWifiConnection* asWifi() override { return this; }

    // STA
    virtual void   configure(const char* ssid, const char* password) = 0;
    virtual int8_t getRssi()                                   const = 0;
    virtual void   triggerScan()                                     = 0;
    virtual bool   isScanInProgress()                          const = 0;
    virtual int    getScanResults(WifiScanResult* out, int maxCount) = 0;

    // AP
    virtual void           startAp(const char* namePrefix = nullptr,
                                   const char* password = nullptr) = 0;
    virtual void           stopAp()                 = 0;
    virtual bool           isApActive()       const = 0;
    virtual esp_ip4_addr_t getApIp()          const = 0;
};
