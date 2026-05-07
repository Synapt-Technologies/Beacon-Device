#include "config.h"
#include "nvs.h"
#include "esp_log.h"

static const char* TAG = "NvsConfig";

bool NvsConfig::load()
{
    nvs_handle_t h;
    if (nvs_open(NS, NVS_READONLY, &h) != ESP_OK) {
        ESP_LOGI(TAG, "No saved config, using defaults");
        return true;
    }

    size_t len;
    len = sizeof(m_cfg.wifi_ssid);    nvs_get_str(h, "wifi_ssid",   m_cfg.wifi_ssid,   &len);
    len = sizeof(m_cfg.wifi_pass);    nvs_get_str(h, "wifi_pass",   m_cfg.wifi_pass,   &len);
    len = sizeof(m_cfg.mqtt_url);     nvs_get_str(h, "mqtt_url",    m_cfg.mqtt_url,    &len);
    len = sizeof(m_cfg.consumer_id);  nvs_get_str(h, "consumer_id", m_cfg.consumer_id, &len);
    len = sizeof(m_cfg.device_id);    nvs_get_str(h, "device_id",   m_cfg.device_id,   &len);
    len = sizeof(m_cfg.led_layout);   nvs_get_str(h, "led_layout",  m_cfg.led_layout,  &len);
    len = sizeof(m_cfg.device_name);  nvs_get_str(h, "device_name", m_cfg.device_name, &len);
    nvs_get_u8(h, "brightness", &m_cfg.led_brightness);

    nvs_close(h);
    ESP_LOGI(TAG, "Loaded: ssid='%s' url='%s' consumer='%s' device='%s' name='%s'",
             m_cfg.wifi_ssid, m_cfg.mqtt_url,
             m_cfg.consumer_id, m_cfg.device_id, m_cfg.device_name);
    return true;
}

bool NvsConfig::save()
{
    nvs_handle_t h;
    if (nvs_open(NS, NVS_READWRITE, &h) != ESP_OK) return false;

    esp_err_t err = ESP_OK;
    if (err == ESP_OK) err = nvs_set_str(h, "wifi_ssid",   m_cfg.wifi_ssid);
    if (err == ESP_OK) err = nvs_set_str(h, "wifi_pass",   m_cfg.wifi_pass);
    if (err == ESP_OK) err = nvs_set_str(h, "mqtt_url",    m_cfg.mqtt_url);
    if (err == ESP_OK) err = nvs_set_str(h, "consumer_id", m_cfg.consumer_id);
    if (err == ESP_OK) err = nvs_set_str(h, "device_id",   m_cfg.device_id);
    if (err == ESP_OK) err = nvs_set_str(h, "led_layout",  m_cfg.led_layout);
    if (err == ESP_OK) err = nvs_set_str(h, "device_name", m_cfg.device_name);
    if (err == ESP_OK) err = nvs_set_u8 (h, "brightness",  m_cfg.led_brightness);
    if (err == ESP_OK) err = nvs_commit(h);

    nvs_close(h);
    return err == ESP_OK;
}

const DeviceConfig& NvsConfig::get() const { return m_cfg; }
void NvsConfig::set(const DeviceConfig& cfg) { m_cfg = cfg; }
