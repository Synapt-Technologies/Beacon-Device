#pragma once
#include "interfaces.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"

#define WIFI_CONNECTED_BIT BIT0

class WifiManager : public IWifiManager {
public:
    explicit WifiManager(const DeviceConfig& cfg);

    void           start()                                           override;
    bool           applyStaCredentials(const char* ssid,
                                       const char* pass)             override;
    bool           isConnected()                               const override;
    esp_ip4_addr_t getStaIp()                                  const override;
    int            getApRecords(wifi_ap_record_t* out,
                                uint16_t max)                  const override;

    void           waitForConnection()                         const override;
    void           triggerScan()                                     override;

private:
    const DeviceConfig& m_cfg;
    EventGroupHandle_t  m_eg        = nullptr;
    bool                m_connected = false;
    esp_ip4_addr_t      m_ip        = {};

    static constexpr int   AP_MAX_RECORDS  = 16;
    wifi_ap_record_t       m_apRecords[AP_MAX_RECORDS] = {};
    uint16_t               m_apCount = 0;

    static void eventHandler(void* arg, esp_event_base_t base,
                             int32_t id, void* data);
    void        onEvent(esp_event_base_t base, int32_t id, void* data);
    void        buildApSsid(char* out, size_t len) const;
};
