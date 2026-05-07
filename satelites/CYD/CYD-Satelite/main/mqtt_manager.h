#pragma once
#include "interfaces.h"
#include "mqtt_client.h"

class MqttManager : public IMqttManager {
public:
    MqttManager() = default;
    ~MqttManager() override;

    void start(const char* url)                     override;
    void subscribe(const char* topic, MessageCb cb) override;
    void clearSubscriptions()                       override;
    void stop()                                     override;
    bool isConnected()                        const override;

private:
    static constexpr int MAX_SUBS = 6;

    struct Subscription {
        char      topic[128];
        MessageCb cb;
    };

    esp_mqtt_client_handle_t m_client    = nullptr;
    bool                     m_connected = false;
    Subscription             m_subs[MAX_SUBS];
    int                      m_subCount  = 0;

    void resubscribeAll();

    static void eventHandler(void* arg, esp_event_base_t base,
                             int32_t id, void* data);
    void        onEvent(esp_mqtt_event_handle_t event);
};
