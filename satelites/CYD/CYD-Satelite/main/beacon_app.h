#pragma once
#include "interfaces.h"
#include "web_server.h"

class BeaconApp {
public:
    BeaconApp(ILedController& leds,
              IConfig&        config,
              IWifiManager&   wifi,
              IMqttManager&   mqtt,
              WebServer&      web);

    void run();  // does not return
    void applyRuntimeConfig(const DeviceConfig& previous,
                            const DeviceConfig& current,
                            bool& rebootNeeded);

private:
    ILedController& m_leds;
    IConfig&        m_config;
    IWifiManager&   m_wifi;
    IMqttManager&   m_mqtt;
    WebServer&      m_web;

    // Last tally color (used by useTallyColor pattern steps)
    struct Color { uint8_t r, g, b; };
    Color m_tallyColor = {0, 0, 0};

    bool m_beaconOnline = false;

    void configureMqtt(const DeviceConfig& cfg, bool resetSubscriptions);
    void buildTopics(const DeviceConfig& cfg,
                     char* tallyTopic, int tallyTopicLen,
                     char* alertTopic, int alertTopicLen) const;

    void onTally      (const char* data, int len);
    void onAlert      (const char* data, int len);
    void onSystemInfo (const char* data, int len);

    static Color    stateColor(int ss);
    static LedTarget alertTarget(int target);
    static int      extractInt(const char* data, int len,
                               const char* key, int fallback);

    static void mqttTask(void* arg);
};
