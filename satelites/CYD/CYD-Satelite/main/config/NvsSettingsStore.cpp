#include "config/NvsSettingsStore.hpp"
#include "nvs.h"
#include "esp_log.h"
#include <cstdio>

static constexpr char TAG[] = "NvsSettingsStore";

static const char* make_key(char* buf, size_t len, const char* prefix, size_t index)
{
    std::snprintf(buf, len, "%s%u", prefix, static_cast<unsigned>(index));
    return buf;
}

bool NvsSettingsStore::load(Settings& out)
{
    nvs_handle_t h;
    if (nvs_open(NS, NVS_READONLY, &h) != ESP_OK) {
        ESP_LOGI(TAG, "No saved settings, using defaults");
        return true;
    }

    auto str = [&](const char* key, char* buf, size_t len) {
        nvs_get_str(h, key, buf, &len);
    };

    str("ssid",        out.network.ssid,       sizeof(out.network.ssid));
    str("password",    out.network.password,    sizeof(out.network.password));
    str("mqttUrl",     out.beacon.mqttUrl,      sizeof(out.beacon.mqttUrl));
    str("deviceName",  out.deviceName,          sizeof(out.deviceName));

    char key[32];
    for (int i = 0; i < 8; i++) {
        size_t s = sizeof(out.beacon.consumerId[i]);
        nvs_get_str(h, make_key(key, sizeof(key), "consumerId_", static_cast<size_t>(i)), out.beacon.consumerId[i], &s);
        s = sizeof(out.beacon.deviceId[i]);
        nvs_get_str(h, make_key(key, sizeof(key), "deviceId_", static_cast<size_t>(i)), out.beacon.deviceId[i], &s);
    }

    for (int i = 0; i < (int)(sizeof(out.display.brightness) / sizeof(out.display.brightness[0])); i++) {
        uint8_t brightness = 0;
        if (nvs_get_u8(h, make_key(key, sizeof(key), "brightness_", static_cast<size_t>(i)), &brightness) == ESP_OK)
            out.display.brightness[i] = brightness;
    }

    for (int i = 0; i < (int)(sizeof(out.display.alertTarget) / sizeof(out.display.alertTarget[0])); i++) {
        uint8_t raw = 0;
        if (nvs_get_u8(h, make_key(key, sizeof(key), "alertTarget_", static_cast<size_t>(i)), &raw) == ESP_OK)
            out.display.alertTarget[i] = static_cast<DeviceAlertTarget>(raw);
    }

    nvs_close(h);
    ESP_LOGI(TAG, "Loaded: ssid='%s' url='%s' consumer='%s'...",
             out.network.ssid, out.beacon.mqttUrl, out.beacon.consumerId[0]);
    return true;
}

bool NvsSettingsStore::save(const Settings& in)
{
    nvs_handle_t h;
    if (nvs_open(NS, NVS_READWRITE, &h) != ESP_OK) return false;

    esp_err_t err = ESP_OK;
    if (err == ESP_OK) err = nvs_set_str(h, "ssid",       in.network.ssid);
    if (err == ESP_OK) err = nvs_set_str(h, "password",   in.network.password);
    if (err == ESP_OK) err = nvs_set_str(h, "mqttUrl",    in.beacon.mqttUrl);
    if (err == ESP_OK) err = nvs_set_str(h, "deviceName", in.deviceName);

    char key[32];
    for (int i = 0; i < 8 && err == ESP_OK; i++) {
        if (err == ESP_OK) err = nvs_set_str(h, make_key(key, sizeof(key), "consumerId_", static_cast<size_t>(i)), in.beacon.consumerId[i]);
        if (err == ESP_OK) err = nvs_set_str(h, make_key(key, sizeof(key), "deviceId_",   static_cast<size_t>(i)), in.beacon.deviceId[i]);
    }

    for (int i = 0; i < (int)(sizeof(in.display.brightness) / sizeof(in.display.brightness[0])) && err == ESP_OK; i++)
        err = nvs_set_u8(h, make_key(key, sizeof(key), "brightness_", static_cast<size_t>(i)), in.display.brightness[i]);

    for (int i = 0; i < (int)(sizeof(in.display.alertTarget) / sizeof(in.display.alertTarget[0])) && err == ESP_OK; i++)
        err = nvs_set_u8(h, make_key(key, sizeof(key), "alertTarget_", static_cast<size_t>(i)), static_cast<uint8_t>(in.display.alertTarget[i]));

    if (err == ESP_OK) err = nvs_commit(h);

    nvs_close(h);
    return err == ESP_OK;
}
