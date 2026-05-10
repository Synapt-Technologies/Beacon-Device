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
        return nvs_get_str(h, key, buf, &len);
    };
    auto u8 = [&](const char* key, uint8_t* val) {
        return nvs_get_u8(h, key, val);
    };

    str("ssid",        out.network.ssid,       sizeof(out.network.ssid));
    str("password",    out.network.password,    sizeof(out.network.password));
    str("mqttUrl",     out.beacon.mqttUrl,      sizeof(out.beacon.mqttUrl));
    str("deviceName",  out.deviceName,          sizeof(out.deviceName));

    char key[32];
    for (int i = 0; i < 8; i++) {
        str(make_key(key, sizeof(key), "consumerId_", static_cast<size_t>(i)), out.beacon.consumerId[i], sizeof(out.beacon.consumerId[i]));
        str(make_key(key, sizeof(key), "deviceId_",   static_cast<size_t>(i)), out.beacon.deviceId[i],   sizeof(out.beacon.deviceId[i]));
    }

    for (int i = 0; i < (int)(sizeof(out.display.brightness) / sizeof(out.display.brightness[0])); i++) {
        uint8_t brightness = 255;
        if (u8(make_key(key, sizeof(key), "brightness_", static_cast<size_t>(i)), &brightness) == ESP_OK)
            out.display.brightness[i] = brightness;
    }
    
    for (int i = 0; i < (int)(sizeof(out.runtime.name) / sizeof(out.runtime.name[0])); i++) {
        str(make_key(key, sizeof(key), "rt_name_s_", static_cast<size_t>(i)),  out.runtime.name[i].shortName, sizeof(out.runtime.name[i].shortName));
        str(make_key(key, sizeof(key), "rt_name_l_", static_cast<size_t>(i)),  out.runtime.name[i].longName,  sizeof(out.runtime.name[i].longName));
    }

    // str("rt_name_short",  out.runtime.name[0].shortName, sizeof(out.runtime.name[0].shortName));
    // str("rt_name_long",   out.runtime.name[0].longName,  sizeof(out.runtime.name[0].longName));
    if (u8("rt_brightness", &out.runtime.brightness) != ESP_OK) {
        out.runtime.brightness = 255;
    }
    uint8_t raw = 0;
    if (u8("rt_st_disc", &raw) == ESP_OK)
        out.runtime.state_on_disconnect = static_cast<TallyState>(raw);

    nvs_close(h);
    
    ESP_LOGI(TAG, "Loaded: ssid='%s' url='%s' consumer='%s'...",
             out.network.ssid, out.beacon.mqttUrl, out.beacon.consumerId[0]);
    return true;
}

bool NvsSettingsStore::save(const Settings& in)
{
    nvs_handle_t h;
    if (nvs_open(NS, NVS_READWRITE, &h) != ESP_OK) return false;

    auto str = [&](const char* key, const char* buf) {
        return nvs_set_str(h, key, buf);
    };
    auto u8 = [&](const char* key, uint8_t val) {
        return nvs_set_u8(h, key, val);
    };

    esp_err_t err = ESP_OK;
    if (err == ESP_OK) err = str("ssid",        in.network.ssid);
    if (err == ESP_OK) err = str("password",    in.network.password);
    if (err == ESP_OK) err = str("mqttUrl",     in.beacon.mqttUrl);
    if (err == ESP_OK) err = str("deviceName",  in.deviceName);

    char key[32];
    for (int i = 0; i < 8 && err == ESP_OK; i++) {
        if (err == ESP_OK) err = str(make_key(key, sizeof(key), "consumerId_", static_cast<size_t>(i)), in.beacon.consumerId[i]);
        if (err == ESP_OK) err = str(make_key(key, sizeof(key), "deviceId_",   static_cast<size_t>(i)), in.beacon.deviceId[i]);
    }

    for (int i = 0; i < (int)(sizeof(in.display.brightness) / sizeof(in.display.brightness[0])) && err == ESP_OK; i++)
        err = u8(make_key(key, sizeof(key), "brightness_", static_cast<size_t>(i)), in.display.brightness[i]);

    for (int i = 0; i < 8 && err == ESP_OK; i++) {
        if (err == ESP_OK) err = str(make_key(key, sizeof(key), "rt_name_s_", static_cast<size_t>(i)), in.runtime.name[i].shortName);
        if (err == ESP_OK) err = str(make_key(key, sizeof(key), "rt_name_l_", static_cast<size_t>(i)), in.runtime.name[i].longName);
    }

    // if (err == ESP_OK) err = nvs_set_str(h, "rt_name_short", in.runtime.name[0].shortName);
    // if (err == ESP_OK) err = nvs_set_str(h, "rt_name_long",  in.runtime.name[0].longName);
    if (err == ESP_OK) err = u8("rt_brightness", in.runtime.brightness);
    if (err == ESP_OK) err = u8("rt_st_disc", static_cast<uint8_t>(in.runtime.state_on_disconnect));

    if (err == ESP_OK) err = nvs_commit(h);

    nvs_close(h);
    return err == ESP_OK;
}
