#pragma once

#include <functional>
#include <stdint.h>
#include "esp_netif_types.h"
#include "esp_wifi_types.h"
#include "led_layout.h"
#include "led_pattern.h"

struct DeviceConfig {
    char    wifi_ssid[64]    = {};
    char    wifi_pass[64]    = {};
    char    mqtt_url[128]    = {};
    char    consumer_id[32]  = "aedes";
    char    device_id[48]    = {};          // empty = auto-fill from MAC at runtime
    char    led_layout[128]  = "F=ALL;S=0,21,ALL";
    char    device_name[32]  = "CYD Satelite";
    uint8_t led_brightness   = 255;
};

class IConfig {
public:
    virtual ~IConfig() = default;
    virtual bool               load()                    = 0;
    virtual bool               save()                    = 0;
    virtual const DeviceConfig& get()              const = 0;
    virtual void               set(const DeviceConfig&)  = 0;
};

class ILedController {
public:
    virtual ~ILedController() = default;

    // Sets all LEDs regardless of target
    virtual void setColor(uint8_t r, uint8_t g, uint8_t b) = 0;

    // Sets only LED sections matching the given target (ALL matches everything)
    virtual void setColorForTarget(LedTarget target, uint8_t r, uint8_t g, uint8_t b) = 0;

    // Runs a pattern on LED sections matching target.
    // repeat=true: loops forever until stopPattern(); false: plays once then stops.
    // tallyR/G/B: current tally color for PatternStep::useTallyColor steps.
    virtual void runPatternForTarget(LedTarget target,
                                     const PatternStep* steps, int count,
                                     bool repeat,
                                     uint8_t tallyR, uint8_t tallyG, uint8_t tallyB) = 0;

    // Updates the tally color supplied to any currently running pattern task.
    virtual void updatePatternTallyColor(uint8_t r, uint8_t g, uint8_t b) = 0;

    // Stops any running pattern for the given target; restores last solid color.
    virtual void stopPattern(LedTarget target) = 0;

    virtual void setBrightness(uint8_t brightness) = 0;
};

class IWifiManager {
public:
    virtual ~IWifiManager() = default;
    virtual void           start()                                           = 0;
    virtual bool           applyStaCredentials(const char* ssid,
                                               const char* pass)             = 0;
    virtual bool           isConnected()                               const = 0;
    virtual esp_ip4_addr_t getStaIp()                                  const = 0;
    virtual int            getApRecords(wifi_ap_record_t* out,
                                        uint16_t max)                  const = 0;
    virtual void           waitForConnection()                         const = 0;
    virtual void           triggerScan()                                     = 0;
};

class IMqttManager {
public:
    using MessageCb = std::function<void(const char* data, int len)>;
    virtual ~IMqttManager() = default;

    // Connect to broker (does not subscribe yet)
    virtual void start(const char* url)                              = 0;

    // Subscribe to topic; cb is called for every matching incoming message.
    // Safe to call before or after start() — re-subscribes on reconnect.
    virtual void subscribe(const char* topic, MessageCb cb)          = 0;
    virtual void clearSubscriptions()                                 = 0;

    virtual void stop()                                              = 0;
    virtual bool isConnected()                                 const = 0;
};
