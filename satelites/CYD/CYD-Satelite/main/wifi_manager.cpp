#include "wifi_manager.h"
#include "esp_log.h"
#include "esp_mac.h"
#include <string.h>
#include <stdio.h>

static const char* TAG = "WifiManager";

WifiManager::WifiManager(const DeviceConfig& cfg) : m_cfg(cfg) {}

void WifiManager::start()
{
    m_eg = xEventGroupCreate();

    esp_netif_create_default_wifi_ap();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t initCfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&initCfg));

    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, eventHandler, this);
    esp_event_handler_register(IP_EVENT,   IP_EVENT_STA_GOT_IP, eventHandler, this);

    // Configure AP
    wifi_config_t apCfg = {};
    buildApSsid((char*)apCfg.ap.ssid, sizeof(apCfg.ap.ssid));
    apCfg.ap.ssid_len       = strlen((char*)apCfg.ap.ssid);
    apCfg.ap.max_connection = 4;
    apCfg.ap.authmode       = WIFI_AUTH_OPEN;

    // Configure STA
    wifi_config_t staCfg = {};
    strlcpy((char*)staCfg.sta.ssid,     m_cfg.wifi_ssid, sizeof(staCfg.sta.ssid));
    strlcpy((char*)staCfg.sta.password, m_cfg.wifi_pass, sizeof(staCfg.sta.password));
    staCfg.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP,  &apCfg));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &staCfg));
    ESP_ERROR_CHECK(esp_wifi_start());
    esp_wifi_set_ps(WIFI_PS_NONE);

    // Trigger initial scan so the web UI has data immediately
    wifi_scan_config_t scanCfg = {};
    scanCfg.show_hidden = false;
    esp_wifi_scan_start(&scanCfg, false);

    ESP_LOGI(TAG, "Started. AP SSID: %s", apCfg.ap.ssid);
}

bool WifiManager::isConnected() const { return m_connected; }

esp_ip4_addr_t WifiManager::getStaIp() const { return m_ip; }

bool WifiManager::applyStaCredentials(const char* ssid, const char* pass)
{
    if (!m_eg) {
        ESP_LOGW(TAG, "Cannot apply STA config before start()");
        return false;
    }

    wifi_config_t staCfg = {};
    strlcpy((char*)staCfg.sta.ssid, ssid ? ssid : "", sizeof(staCfg.sta.ssid));
    strlcpy((char*)staCfg.sta.password, pass ? pass : "", sizeof(staCfg.sta.password));
    staCfg.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    esp_err_t err = esp_wifi_set_config(WIFI_IF_STA, &staCfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_set_config(STA) failed: %d", (int)err);
        return false;
    }

    if (staCfg.sta.ssid[0] == '\0') {
        esp_wifi_disconnect();
        m_connected = false;
        m_ip = {};
        xEventGroupClearBits(m_eg, WIFI_CONNECTED_BIT);
        ESP_LOGI(TAG, "STA credentials cleared");
        return true;
    }

    esp_wifi_disconnect();
    err = esp_wifi_connect();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_connect failed: %d", (int)err);
        return false;
    }

    ESP_LOGI(TAG, "Applied STA credentials for SSID '%s'", (char*)staCfg.sta.ssid);
    return true;
}

void WifiManager::waitForConnection() const
{
    xEventGroupWaitBits(m_eg, WIFI_CONNECTED_BIT, pdFALSE, pdTRUE, portMAX_DELAY);
}

void WifiManager::triggerScan()
{
    wifi_scan_config_t cfg = {};
    esp_wifi_scan_start(&cfg, false);
}

int WifiManager::getApRecords(wifi_ap_record_t* out, uint16_t max) const
{
    uint16_t n = m_apCount < max ? m_apCount : max;
    memcpy(out, m_apRecords, n * sizeof(wifi_ap_record_t));
    return n;
}

// ── private ──────────────────────────────────────────────────────────────────

void WifiManager::eventHandler(void* arg, esp_event_base_t base,
                                int32_t id, void* data)
{
    static_cast<WifiManager*>(arg)->onEvent(base, id, data);
}

void WifiManager::onEvent(esp_event_base_t base, int32_t id, void* data)
{
    if (base == WIFI_EVENT) {
        switch (id) {
        case WIFI_EVENT_STA_START:
            if (m_cfg.wifi_ssid[0] != '\0')
                esp_wifi_connect();
            break;

        case WIFI_EVENT_STA_DISCONNECTED: {
            m_connected = false;
            xEventGroupClearBits(m_eg, WIFI_CONNECTED_BIT);
            wifi_event_sta_disconnected_t* e =
                static_cast<wifi_event_sta_disconnected_t*>(data);
            ESP_LOGW(TAG, "STA disconnected reason=%d, retrying...", e->reason);
            if (m_cfg.wifi_ssid[0] != '\0')
                esp_wifi_connect();
            break;
        }

        case WIFI_EVENT_SCAN_DONE: {
            uint16_t count = AP_MAX_RECORDS;
            esp_wifi_scan_get_ap_records(&count, m_apRecords);
            m_apCount = count;
            ESP_LOGI(TAG, "Scan done: %d APs found", count);
            break;
        }

        default: break;
        }
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* e = static_cast<ip_event_got_ip_t*>(data);
        m_ip        = e->ip_info.ip;
        m_connected = true;
        xEventGroupSetBits(m_eg, WIFI_CONNECTED_BIT);
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&m_ip));
    }
}

void WifiManager::buildApSsid(char* out, size_t len) const
{
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP);
    snprintf(out, len, "CYD-%02X%02X%02X", mac[3], mac[4], mac[5]);
}
