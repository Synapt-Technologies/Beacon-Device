#pragma once

#include "beaconConnection/IBeaconConnection.hpp"
#include "mqtt_client.h"
#include "esp_log.h"
#include <cstring>
#include <cstdio>

class TcpMqttBeaconConnection : public IBeaconConnection {
public:
    TcpMqttBeaconConnection(const char* url) {
        strncpy(_url, url, sizeof(_url) - 1);
    }

    ~TcpMqttBeaconConnection() {
        this->stop();
    }

    void start() override {
        if (_client)
            this->stop();

        esp_mqtt_client_config_t cfg = {};
        cfg.broker.address.uri             = _url;
        cfg.task.priority                  = 18;
        cfg.network.reconnect_timeout_ms   = 2000;

        _client = esp_mqtt_client_init(&cfg);
        esp_mqtt_client_register_event(_client,
                                       (esp_mqtt_event_id_t)ESP_EVENT_ANY_ID,
                                       eventHandler, this);
        esp_mqtt_client_start(_client);
        ESP_LOGI(TAG, "Connecting to %s", _url);
    }

    void stop() override {
        if (!_client) return;

        esp_mqtt_client_stop(_client);
        esp_mqtt_client_destroy(_client);
        _client         = nullptr;
        _connected      = false;
        _tallyTopic[0]  = '\0';
        _alertTopic[0]  = '\0';
    }

    void setTallyCallback(TallyCb cb) override { _tallyCb = cb; }
    void setAlertCallback(AlertCb cb) override { _alertCb = cb; }

private:
    static constexpr char TAG[]       = "TcpMqtt";
    static constexpr char _infoTopic[] = "system/info";

    char                     _url[256]        = {};
    char                     _tallyTopic[160] = {};
    char                     _alertTopic[160] = {};
    esp_mqtt_client_handle_t _client          = nullptr;

    void updateSubscriptions() override {
        if (!_client || !_connected) return;

        clearSubscriptions();

        snprintf(_tallyTopic, sizeof(_tallyTopic), "tally/device/%s/%s", _consumer, _device);
        snprintf(_alertTopic, sizeof(_alertTopic), "tally/device/%s/%s/alert", _consumer, _device);

        if (esp_mqtt_client_subscribe(_client, _infoTopic, 0) < 0)
            ESP_LOGW(TAG, "Failed to subscribe to %s", _infoTopic);
        else
            ESP_LOGI(TAG, "Subscribed to %s", _infoTopic);

        if (_tallyCb) {
            if (esp_mqtt_client_subscribe(_client, _tallyTopic, 0) < 0)
                ESP_LOGW(TAG, "Failed to subscribe to %s", _tallyTopic);
            else
                ESP_LOGI(TAG, "Subscribed to %s", _tallyTopic);
        }

        if (_alertCb) {
            if (esp_mqtt_client_subscribe(_client, _alertTopic, 0) < 0)
                ESP_LOGW(TAG, "Failed to subscribe to %s", _alertTopic);
            else
                ESP_LOGI(TAG, "Subscribed to %s", _alertTopic);
        }
    }

    void clearSubscriptions() override {
        if (!_client) return;

        // TODO Unsubscribe all topics function?
        // TODO unsubscribe to info? Doesn't change.

        esp_mqtt_client_unsubscribe(_client, _infoTopic);

        if (_tallyTopic[0]) {
            esp_mqtt_client_unsubscribe(_client, _tallyTopic);
            _tallyTopic[0] = '\0';
        }
        if (_alertTopic[0]) {
            esp_mqtt_client_unsubscribe(_client, _alertTopic);
            _alertTopic[0] = '\0';
        }
    }

    static void eventHandler(void* arg, esp_event_base_t base, int32_t event_id, void* event_data) {
        auto* self  = static_cast<TcpMqttBeaconConnection*>(arg);
        auto* event = static_cast<esp_mqtt_event_handle_t>(event_data);

        switch (event_id) {
            case MQTT_EVENT_CONNECTED:
                self->_connected     = true;
                self->_lastKeepAlive = xTaskGetTickCount();
                self->updateSubscriptions();
                ESP_LOGI(TAG, "Connected");
                break;
            case MQTT_EVENT_DISCONNECTED:
                self->_connected = false;
                ESP_LOGI(TAG, "Disconnected");
                break;
            case MQTT_EVENT_DATA:
                self->handleData(event);
                break;
            default:
                break;
        }
    }

    void handleData(esp_mqtt_event_handle_t event) {
        auto matches = [&](const char* topic) {
            return static_cast<int>(strlen(topic)) == event->topic_len &&
                   strncmp(event->topic, topic, event->topic_len) == 0;
        };

        ESP_LOGI(TAG, "Received data on topic %.*s: %.*s",
                 event->topic_len, event->topic,
                 event->data_len, event->data);

        if      (matches(_tallyTopic)) onTally(event->data, event->data_len);
        else if (matches(_alertTopic)) onAlert(event->data, event->data_len);
        else if (matches(_infoTopic))  onGlobalInfo(event->data, event->data_len);
    }

    void onGlobalInfo(const char* data, int len) { // TODO add Beacon info parsing
        _lastKeepAlive = xTaskGetTickCount();
    }

    void onTally(const char* data, int len) {
        if (!_tallyCb)
            return;

        TallyState ss = TallyState::NONE;
        int rawSs = extractInt(data, len, "ss", -1);
        if (rawSs >= 0) {
            ss = static_cast<TallyState>(rawSs);
        } else {
            char state[16] = {};
            const char* key = "\"state\":\"";
            for (int i = 0; i <= len - 9; i++) {
                if (strncmp(data + i, key, 9) == 0) {
                    const char* p = data + i + 9;
                    int j = 0;
                    while (p + j < data + len && p[j] != '"' && j < 15)
                        state[j] = p[j], j++;
                    break;
                }
            }
            if      (strcmp(state, "PROGRAM") == 0) ss = TallyState::PROGRAM;
            else if (strcmp(state, "DANGER")  == 0) ss = TallyState::DANGER;
            else if (strcmp(state, "PREVIEW") == 0) ss = TallyState::PREVIEW;
            else if (strcmp(state, "WARNING") == 0) ss = TallyState::WARNING;
        }

        ESP_LOGI(TAG, "Tally ss=%d", static_cast<int>(ss));
        _tallyCb(ss);
    }

    void onAlert(const char* data, int len) {
        if (!_alertCb)
            return;

        int type   = extractInt(data, len, "type",   -1);
        int target = extractInt(data, len, "target", -1);
        int time   = extractInt(data, len, "time",   -1);

        ESP_LOGI(TAG, "Alert type=%d target=%d time=%d", type, target, time);

        _alertCb(
            static_cast<DeviceAlertAction>(type),
            static_cast<DeviceAlertTarget>(target),
            time >= 0 ? static_cast<uint32_t>(time) : 0
        );
    }

    static int extractInt(const char* data, int len, const char* key, int defaultVal) {
        char search[32];
        snprintf(search, sizeof(search), "\"%s\":", key);
        int keyLen = static_cast<int>(strlen(search));
        for (int i = 0; i <= len - keyLen; i++) {
            if (strncmp(data + i, search, keyLen) == 0) {
                const char* p = data + i + keyLen;
                while (p < data + len && *p == ' ') p++;
                int remaining = static_cast<int>(data + len - p);
                char numBuf[16] = {};
                int copyLen = remaining < (int)sizeof(numBuf) - 1 ? remaining : (int)sizeof(numBuf) - 1;
                strncpy(numBuf, p, copyLen);
                char* end;
                long val = strtol(numBuf, &end, 10);
                if (end != numBuf) return static_cast<int>(val);
            }
        }
        return defaultVal;
    }
};
