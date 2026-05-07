#include "mqtt_manager.h"
#include "esp_log.h"
#include <string.h>

static const char* TAG = "MqttManager";

MqttManager::~MqttManager() { stop(); }

void MqttManager::start(const char* url)
{
    if (m_client) stop();

    esp_mqtt_client_config_t cfg = {};
    cfg.broker.address.uri = url;
    cfg.task.priority      = 18;

    m_client = esp_mqtt_client_init(&cfg);
    esp_mqtt_client_register_event(m_client,
                                   (esp_mqtt_event_id_t)ESP_EVENT_ANY_ID,
                                   eventHandler, this);
    esp_mqtt_client_start(m_client);
    ESP_LOGI(TAG, "Connecting to %s", url);
}

void MqttManager::subscribe(const char* topic, MessageCb cb)
{
    if (m_subCount >= MAX_SUBS) {
        ESP_LOGE(TAG, "Subscription limit reached");
        return;
    }
    Subscription& s = m_subs[m_subCount++];
    strlcpy(s.topic, topic, sizeof(s.topic));
    s.cb = cb;

    // If already connected, subscribe immediately
    if (m_connected && m_client)
        esp_mqtt_client_subscribe(m_client, topic, 0);

    ESP_LOGI(TAG, "Registered subscription: %s", topic);
}

void MqttManager::clearSubscriptions()
{
    if (m_client && m_connected) {
        for (int i = 0; i < m_subCount; i++)
            esp_mqtt_client_unsubscribe(m_client, m_subs[i].topic);
    }
    memset(m_subs, 0, sizeof(m_subs));
    m_subCount = 0;
}

void MqttManager::stop()
{
    if (!m_client) return;
    esp_mqtt_client_stop(m_client);
    esp_mqtt_client_destroy(m_client);
    m_client    = nullptr;
    m_connected = false;
}

bool MqttManager::isConnected() const { return m_connected; }

// ── private ──────────────────────────────────────────────────────────────────

void MqttManager::resubscribeAll()
{
    for (int i = 0; i < m_subCount; i++)
        esp_mqtt_client_subscribe(m_client, m_subs[i].topic, 0);
}

void MqttManager::eventHandler(void* arg, esp_event_base_t /*base*/,
                                int32_t /*id*/, void* data)
{
    static_cast<MqttManager*>(arg)->onEvent(
        static_cast<esp_mqtt_event_handle_t>(data));
}

void MqttManager::onEvent(esp_mqtt_event_handle_t event)
{
    switch (event->event_id) {
    case MQTT_EVENT_CONNECTED:
        m_connected = true;
        ESP_LOGI(TAG, "Connected");
        resubscribeAll();
        break;

    case MQTT_EVENT_DISCONNECTED:
        m_connected = false;
        ESP_LOGW(TAG, "Disconnected");
        break;

    case MQTT_EVENT_DATA:
        // Route to the matching subscription callback
        for (int i = 0; i < m_subCount; i++) {
            // Topic in event may not be null-terminated; compare with length
            if ((int)strlen(m_subs[i].topic) == event->topic_len &&
                memcmp(m_subs[i].topic, event->topic, event->topic_len) == 0) {
                if (m_subs[i].cb)
                    m_subs[i].cb(event->data, event->data_len);
                break;
            }
        }
        break;

    case MQTT_EVENT_ERROR:
        ESP_LOGE(TAG, "Error");
        break;

    default: break;
    }
}
