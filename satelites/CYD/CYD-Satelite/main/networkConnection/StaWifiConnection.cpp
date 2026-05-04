#include "networkConnection/StaWifiConnection.hpp"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include <cstring>
#include <cstdio>

// ── INetworkConnection ────────────────────────────────────────────────────────

void StaWifiConnection::start()
{
    if (_staNetif) return; // guard against double-call

    _staNetif = esp_netif_create_default_wifi_sta();
    _apNetif  = esp_netif_create_default_wifi_ap();

    wifi_init_config_t initCfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&initCfg));

    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,    eventHandler, this);
    esp_event_handler_register(IP_EVENT,   IP_EVENT_STA_GOT_IP, eventHandler, this);

    // Build AP config. All WiFi config must happen before esp_wifi_start() —
    // calling esp_wifi_set_config / esp_wifi_set_mode from inside a WiFi event
    // callback causes ppTask to re-enter esp_event_post, which crashes in IDF v6.
    char apSsid[32] = {};
    buildApSsid(apSsid, sizeof(apSsid), _deviceType);

    wifi_config_t apCfg = {};
    strlcpy((char*)apCfg.ap.ssid, apSsid, sizeof(apCfg.ap.ssid));
    apCfg.ap.ssid_len       = static_cast<uint8_t>(strlen(apSsid));
    apCfg.ap.max_connection = 4;
    apCfg.ap.authmode       = WIFI_AUTH_OPEN;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &apCfg));
    applyStaConfig();
    ESP_ERROR_CHECK(esp_wifi_start());
    esp_wifi_set_ps(WIFI_PS_NONE);

    _reconnectTimer = xTimerCreate("wifi_rc", pdMS_TO_TICKS(RECONNECT_MS),
                                   pdFALSE, this, reconnectTimerCb);
    _scanMutex = xSemaphoreCreateMutex();
    ESP_ERROR_CHECK(_scanMutex ? ESP_OK : ESP_ERR_NO_MEM);

    _running    = true;
    _retryCount = 0;
    _apActive   = true;

    esp_netif_ip_info_t ipInfo = {};
    esp_netif_get_ip_info(_apNetif, &ipInfo);
    _apIp = ipInfo.ip;

    ESP_LOGI(TAG, "Started. AP: %s", apSsid);
}

void StaWifiConnection::stop()
{
    if (!_staNetif) return;

    _running = false; // stop event handler and timer callback from acting first

    if (_reconnectTimer) {
        xTimerStop(_reconnectTimer, 0);
    }

    esp_event_handler_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID,    eventHandler);
    esp_event_handler_unregister(IP_EVENT,   IP_EVENT_STA_GOT_IP, eventHandler);

    esp_wifi_stop();
    esp_wifi_deinit();

    esp_netif_destroy(_staNetif); _staNetif = nullptr;
    esp_netif_destroy(_apNetif);  _apNetif  = nullptr;
    _apActive = false;

    if (_reconnectTimer) {
        xTimerDelete(_reconnectTimer, 0);
        _reconnectTimer = nullptr;
    }

    _ip     = {};
    _status = NetworkStatus::DISCONNECTED;
    if (_scanMutex) {
        xSemaphoreTake(_scanMutex, portMAX_DELAY);
        _scanInProgress = false;
        _scanCount = 0;
        xSemaphoreGive(_scanMutex);
        vSemaphoreDelete(_scanMutex);
        _scanMutex = nullptr;
    }
    fireCallback(NetworkStatus::DISCONNECTED);
}

NetworkStatus StaWifiConnection::getStatus() const { return _status; }
esp_ip4_addr_t StaWifiConnection::getIp()    const { return _ip;     }

void StaWifiConnection::setConnectionCallback(ConnectionCb cb)
{
    _cb = cb;
}

// ── IWifiConnection — STA ─────────────────────────────────────────────────────

void StaWifiConnection::configure(const char* ssid, const char* password)
{
    strncpy(_ssid, ssid     ? ssid     : "", sizeof(_ssid) - 1);
    strncpy(_pass, password ? password : "", sizeof(_pass) - 1);
    _ssid[sizeof(_ssid) - 1] = '\0';
    _pass[sizeof(_pass) - 1] = '\0';

    if (_staNetif) applyStaConfig(); // update driver; orchestrator triggers reconnect
}

int8_t StaWifiConnection::getRssi() const // TODO Change return type?
{
    int rssi = 0;
    esp_wifi_sta_get_rssi(&rssi);
    return static_cast<int8_t>(rssi);
}

void StaWifiConnection::triggerScan()
{
    if (!_running) {
        ESP_LOGW(TAG, "Scan requested while WiFi is stopped");
        return;
    }

    wifi_scan_config_t cfg = {};
    esp_err_t err = esp_wifi_scan_start(&cfg, false);
    if (err == ESP_ERR_WIFI_STATE) {
        ESP_LOGI(TAG, "Scan already in progress");
        if (_scanMutex && xSemaphoreTake(_scanMutex, portMAX_DELAY) == pdTRUE) {
            _scanInProgress = true;
            xSemaphoreGive(_scanMutex);
        }
        return;
    }
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to start scan: %s", esp_err_to_name(err));
        if (_scanMutex && xSemaphoreTake(_scanMutex, portMAX_DELAY) == pdTRUE) {
            _scanInProgress = false;
            xSemaphoreGive(_scanMutex);
        }
        return;
    }
    if (_scanMutex && xSemaphoreTake(_scanMutex, portMAX_DELAY) == pdTRUE) {
        _scanInProgress = true;
        xSemaphoreGive(_scanMutex);
    }
}

bool StaWifiConnection::isScanInProgress() const
{
    if (!_scanMutex) return false;
    if (xSemaphoreTake(_scanMutex, portMAX_DELAY) != pdTRUE) return true;
    bool inProgress = _scanInProgress;
    xSemaphoreGive(_scanMutex);
    return inProgress;
}

int StaWifiConnection::getScanResults(WifiScanResult* out, int maxCount)
{
    if (!out || maxCount <= 0) {
        ESP_LOGW(TAG, "Invalid scan result buffer");
        return 0;
    }

    int count = 0;
    if (_scanMutex && xSemaphoreTake(_scanMutex, portMAX_DELAY) == pdTRUE) {
        count = _scanCount < maxCount ? _scanCount : maxCount;
        memcpy(out, _scanResults, count * sizeof(WifiScanResult));
        xSemaphoreGive(_scanMutex);
    }
    return count;
}

// ── IWifiConnection — AP ──────────────────────────────────────────────────────

// startAp / stopAp are intentionally no-ops after start(): the AP is configured
// once before esp_wifi_start() and stays running. They only update the logical
// _apActive flag so the orchestrator / HTTP layer can read isApActive() correctly.
// Calling esp_wifi_set_config from inside a WiFi event handler crashes in IDF v6
// (ppTask re-enters esp_event_post with a NULL event-loop mutex).

void StaWifiConnection::startAp(const char* /*namePrefix*/, const char* /*password*/)
{
    if (!_staNetif) return;
    _apActive = true;
    esp_netif_ip_info_t ipInfo = {};
    esp_netif_get_ip_info(_apNetif, &ipInfo);
    _apIp = ipInfo.ip;
    ESP_LOGI(TAG, "AP active");
}

void StaWifiConnection::stopAp()
{
    if (!_apActive) return;
    _apActive = false;
    _apIp     = {};
    ESP_LOGI(TAG, "AP inactive");
}

bool           StaWifiConnection::isApActive() const { return _apActive; }
esp_ip4_addr_t StaWifiConnection::getApIp()    const { return _apIp;     }

// ── Private ───────────────────────────────────────────────────────────────────

void StaWifiConnection::eventHandler(void* arg, esp_event_base_t base,
                                     int32_t id, void* data)
{
    static_cast<StaWifiConnection*>(arg)->onEvent(base, id, data);
}

void StaWifiConnection::reconnectTimerCb(TimerHandle_t timer)
{
    auto* self = static_cast<StaWifiConnection*>(pvTimerGetTimerID(timer));
    if (self->_running && self->_ssid[0])
        esp_wifi_connect();
}

void StaWifiConnection::onEvent(esp_event_base_t base, int32_t id, void* data)
{
    if (!_running) return; // guard against post-stop events

    if (base == WIFI_EVENT) {
        switch (id) {
        case WIFI_EVENT_STA_START:
            _apActive = true;
            if (_ssid[0]) {
                esp_wifi_connect();
                _status = NetworkStatus::CONNECTING;
                fireCallback(NetworkStatus::CONNECTING);
            } else {
                _status = NetworkStatus::DISCONNECTED;
                fireCallback(NetworkStatus::DISCONNECTED);
            }
            break;

        case WIFI_EVENT_STA_DISCONNECTED: {
            _ip       = {};
            _status   = NetworkStatus::DISCONNECTED;
            _apActive = true;

            fireCallback(NetworkStatus::DISCONNECTED);

            auto* e = static_cast<wifi_event_sta_disconnected_t*>(data);
            ESP_LOGW(TAG, "Disconnected reason=%d", e->reason);

            if (_ssid[0] && e->reason != WIFI_REASON_ASSOC_LEAVE) {
                _status = NetworkStatus::CONNECTING;
                fireCallback(NetworkStatus::CONNECTING);
                if (_retryCount++ == 0) {
                    esp_wifi_connect();
                } else {
                    xTimerStart(_reconnectTimer, 0);
                }
            }
            break;
        }

        case WIFI_EVENT_SCAN_DONE: {
            auto* e = static_cast<wifi_event_sta_scan_done_t*>(data);
            if (e && e->status != 0) {
                ESP_LOGW(TAG, "Scan failed, status=%d", e->status);
                if (_scanMutex && xSemaphoreTake(_scanMutex, portMAX_DELAY) == pdTRUE) {
                    _scanInProgress = false;
                    _scanCount = 0;
                    xSemaphoreGive(_scanMutex);
                }
                break;
            }

            uint16_t count = SCAN_MAX;
            esp_err_t err = esp_wifi_scan_get_ap_records(&count, _scanRecords);
            if (err != ESP_OK) {
                ESP_LOGW(TAG, "Scan results fetch failed: %s", esp_err_to_name(err));
                if (_scanMutex && xSemaphoreTake(_scanMutex, portMAX_DELAY) == pdTRUE) {
                    _scanInProgress = false;
                    _scanCount = 0;
                    xSemaphoreGive(_scanMutex);
                }
                break;
            }

            if (_scanMutex && xSemaphoreTake(_scanMutex, portMAX_DELAY) == pdTRUE) {
                _scanInProgress = false;
                _scanCount = count;
                for (int i = 0; i < count; i++) {
                    strncpy(_scanResults[i].ssid, (char*)_scanRecords[i].ssid, 32);
                    _scanResults[i].ssid[32] = '\0';
                    memcpy(_scanResults[i].bssid, _scanRecords[i].bssid, 6);
                    _scanResults[i].channel  = _scanRecords[i].primary;
                    _scanResults[i].rssi     = _scanRecords[i].rssi;
                    _scanResults[i].authmode = _scanRecords[i].authmode;
                }
                xSemaphoreGive(_scanMutex);
            }
            ESP_LOGI(TAG, "Scan done: %d APs", count);
            break;
        }

        default: break;
        }
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        auto* e     = static_cast<ip_event_got_ip_t*>(data);
        _ip         = e->ip_info.ip;
        _retryCount = 0;
        _status     = NetworkStatus::CONNECTED;
        _apActive   = false;

        fireCallback(NetworkStatus::CONNECTED);

        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&_ip));
    }
}

void StaWifiConnection::fireCallback(NetworkStatus status)
{
    if (_cb) _cb(status, _ip);
}

void StaWifiConnection::buildApSsid(char* out, size_t len, const char* namePrefix)
{
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP);
    snprintf(out, len, "%s-%02X%02X%02X", namePrefix, mac[3], mac[4], mac[5]);
}

void StaWifiConnection::applyStaConfig()
{
    wifi_config_t staCfg = {};
    strlcpy((char*)staCfg.sta.ssid,     _ssid, sizeof(staCfg.sta.ssid));
    strlcpy((char*)staCfg.sta.password, _pass, sizeof(staCfg.sta.password));
    staCfg.sta.threshold.authmode = _pass[0] ? WIFI_AUTH_WPA2_PSK : WIFI_AUTH_OPEN;
    staCfg.sta.scan_method        = WIFI_FAST_SCAN;
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &staCfg));
}
