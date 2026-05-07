#pragma once

#include "networkConnection/IWifiConnection.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "freertos/semphr.h"
#include "esp_wifi.h"

class StaWifiConnection : public IWifiConnection {
public:
    StaWifiConnection(const char* deviceType = "Beacon_Satellite") : IWifiConnection(deviceType) {}
    ~StaWifiConnection() { stop(); }

    // INetworkConnection
    void           start()                                      override; // TODO: Should be in higher level interface
    void           stop()                                       override;
    NetworkStatus  getStatus()                            const override;
    esp_ip4_addr_t getIp()                                const override;
    void           setConnectionCallback(ConnectionCb cb)       override;

    // IWifiConnection — STA
    void   configure(const char* ssid, const char* password)    override;
    int8_t getRssi()                                      const override;
    void   triggerScan()                                        override;
    bool   isScanInProgress()                             const override;
    int    getScanResults(WifiScanResult* out, int maxCount)     override;

    // IWifiConnection — AP
    void           startAp(const char* namePrefix = nullptr, const char* password = nullptr) override;
    void           stopAp()                                     override;
    bool           isApActive()                           const override;
    esp_ip4_addr_t getApIp()                              const override;

private:
    static constexpr char TAG[]            = "StaWifi";
    static constexpr int  SCAN_MAX         = 32;
    static constexpr int  RECONNECT_MS     = 1000;

    bool           _running     = false;
    int            _retryCount  = 0;
    ConnectionCb   _cb;
    TimerHandle_t  _reconnectTimer = nullptr;

    esp_netif_t*   _staNetif = nullptr;
    esp_netif_t*   _apNetif  = nullptr;
    bool           _apActive = false;
    esp_ip4_addr_t _apIp     = {};

    char _ssid[64] = {};
    char _pass[64] = {};

    WifiScanResult _scanResults[SCAN_MAX] = {};
    wifi_ap_record_t _scanRecords[SCAN_MAX] = {};
    int            _scanCount             = 0;
    bool           _scanInProgress        = false;
    SemaphoreHandle_t _scanMutex          = nullptr; // TODO: Simplify. maybe async http endpoint?

    static void eventHandler(void* arg, esp_event_base_t base,
                             int32_t id, void* data);
    static void reconnectTimerCb(TimerHandle_t timer);

    void        onEvent(esp_event_base_t base, int32_t id, void* data);
    void        fireCallback(NetworkStatus status);

    static void buildApSsid(char* out, size_t len, const char* namePrefix);
    void        applyStaConfig();
};
