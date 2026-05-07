#include "web_server.h"
#include "beacon_app.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

static const char* TAG = "WebServer";

// ── Embedded UI assets ─────────────────────────────────────────────────────────

extern const uint8_t ui_index_html_start[] asm("_binary_index_html_start");
extern const uint8_t ui_index_html_end[]   asm("_binary_index_html_end");
extern const uint8_t ui_ui_css_start[]     asm("_binary_ui_css_start");
extern const uint8_t ui_ui_css_end[]       asm("_binary_ui_css_end");
extern const uint8_t ui_ui_js_start[]      asm("_binary_ui_js_start");
extern const uint8_t ui_ui_js_end[]        asm("_binary_ui_js_end");

struct EmbeddedAsset {
    const uint8_t* data;
    size_t         size;
    const char*    contentType;
    bool           trimNullTerminator;
};

static const EmbeddedAsset INDEX_HTML = {
    ui_index_html_start,
    static_cast<size_t>(ui_index_html_end - ui_index_html_start),
    "text/html; charset=utf-8",
    true
};

static const EmbeddedAsset UI_CSS = {
    ui_ui_css_start,
    static_cast<size_t>(ui_ui_css_end - ui_ui_css_start),
    "text/css; charset=utf-8",
    true
};

static const EmbeddedAsset UI_JS = {
    ui_ui_js_start,
    static_cast<size_t>(ui_ui_js_end - ui_ui_js_start),
    "application/javascript; charset=utf-8",
    true
};

static esp_err_t sendAsset(httpd_req_t* req, const EmbeddedAsset& asset)
{
    httpd_resp_set_type(req, asset.contentType);
    const size_t payloadSize =
        (asset.trimNullTerminator && asset.size > 0) ? asset.size - 1 : asset.size;
    return httpd_resp_send(req, reinterpret_cast<const char*>(asset.data), payloadSize);
}

// ── WebServer ─────────────────────────────────────────────────────────────────

WebServer::WebServer(IConfig& config, IWifiManager& wifi, IMqttManager& mqtt)
    : m_config(config), m_wifi(wifi), m_mqtt(mqtt) {}

WebServer::~WebServer() { stop(); }

void WebServer::start()
{
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.max_uri_handlers = 12;

    if (httpd_start(&m_server, &cfg) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HTTP server");
        return;
    }

    httpd_uri_t uris[] = {
        { "/",           HTTP_GET,  handleRoot,   this },
        { "/ui.css",     HTTP_GET,  handleUiCss,  this },
        { "/ui.js",      HTTP_GET,  handleUiJs,   this },
        { "/api/config", HTTP_GET,  handleGetCfg, this },
        { "/api/config", HTTP_POST, handleSetCfg, this },
        { "/api/reboot", HTTP_POST, handleReboot, this },
        { "/api/scan",   HTTP_GET,  handleScan,   this },
        { "/api/status", HTTP_GET,  handleStatus, this },
    };
    for (auto& u : uris) {
        if (httpd_register_uri_handler(m_server, &u) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to register URI handler: %s", u.uri);
        }
    }

    ESP_LOGI(TAG, "HTTP server started on port %d", cfg.server_port);
}

void WebServer::stop()
{
    if (m_server) {
        httpd_stop(m_server);
        m_server = nullptr;
    }
}

// ── Handlers ──────────────────────────────────────────────────────────────────

esp_err_t WebServer::handleRoot(httpd_req_t* req)
{
    return sendAsset(req, INDEX_HTML);
}

esp_err_t WebServer::handleUiCss(httpd_req_t* req)
{
    return sendAsset(req, UI_CSS);
}

esp_err_t WebServer::handleUiJs(httpd_req_t* req)
{
    return sendAsset(req, UI_JS);
}

esp_err_t WebServer::handleGetCfg(httpd_req_t* req)
{
    auto* self = static_cast<WebServer*>(req->user_ctx);
    const DeviceConfig& c = self->m_config.get();

    char buf[640];
    snprintf(buf, sizeof(buf),
        "{\"device_name\":\"%s\","
        "\"led_brightness\":%d,"
        "\"wifi_ssid\":\"%s\","
        "\"mqtt_url\":\"%s\","
        "\"consumer_id\":\"%s\","
        "\"device_id\":\"%s\","
        "\"led_layout\":\"%s\"}",
        c.device_name, c.led_brightness,
        c.wifi_ssid, c.mqtt_url,
        c.consumer_id, c.device_id, c.led_layout);

    httpd_resp_set_type(req, "application/json");
    return httpd_resp_sendstr(req, buf);
}

esp_err_t WebServer::handleSetCfg(httpd_req_t* req)
{
    auto* self = static_cast<WebServer*>(req->user_ctx);
    DeviceConfig oldCfg = self->m_config.get();

    char body[768] = {};
    int received = httpd_req_recv(req, body,
                                  (int)sizeof(body) - 1 < req->content_len
                                      ? (int)sizeof(body) - 1
                                      : req->content_len);
    if (received <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Empty body");
        return ESP_FAIL;
    }

    auto strField = [&](const char* key, char* out, size_t maxLen) {
        char search[64];
        snprintf(search, sizeof(search), "\"%s\":\"", key);
        const char* p = strstr(body, search);
        if (!p) return;
        p += strlen(search);
        size_t i = 0;
        while (*p && *p != '"' && i < maxLen - 1) out[i++] = *p++;
        out[i] = '\0';
    };
    auto u8Field = [&](const char* key, uint8_t& out) {
        char search[64];
        snprintf(search, sizeof(search), "\"%s\":", key);
        const char* p = strstr(body, search);
        if (p) out = (uint8_t)atoi(p + strlen(search));
    };

    DeviceConfig cfg = oldCfg;
    
    auto hasField = [&](const char* key) {
        char search[64];
        snprintf(search, sizeof(search), "\"%s\":", key);
        return strstr(body, search) != nullptr;
    };
    
    if (hasField("device_name"))  strField("device_name",  cfg.device_name,  sizeof(cfg.device_name));
    if (hasField("wifi_ssid"))    strField("wifi_ssid",    cfg.wifi_ssid,    sizeof(cfg.wifi_ssid));
    if (hasField("wifi_pass"))    strField("wifi_pass",    cfg.wifi_pass,    sizeof(cfg.wifi_pass));
    if (hasField("mqtt_url"))     strField("mqtt_url",     cfg.mqtt_url,     sizeof(cfg.mqtt_url));
    if (hasField("consumer_id"))  strField("consumer_id",  cfg.consumer_id,  sizeof(cfg.consumer_id));
    if (hasField("device_id"))    strField("device_id",    cfg.device_id,    sizeof(cfg.device_id));
    if (hasField("led_layout"))   strField("led_layout",   cfg.led_layout,   sizeof(cfg.led_layout));
    if (hasField("led_brightness")) u8Field ("led_brightness", cfg.led_brightness);

    self->m_config.set(cfg);
    if (!self->m_config.save()) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Save failed");
        return ESP_FAIL;
    }

    bool rebootNeeded = false;
    if (self->m_app) {
        self->m_app->applyRuntimeConfig(oldCfg, cfg, rebootNeeded);
    } else {
        rebootNeeded =
            strcmp(oldCfg.device_name, cfg.device_name) != 0 ||
            oldCfg.led_brightness != cfg.led_brightness ||
            strcmp(oldCfg.wifi_ssid, cfg.wifi_ssid) != 0 ||
            strcmp(oldCfg.wifi_pass, cfg.wifi_pass) != 0 ||
            strcmp(oldCfg.mqtt_url, cfg.mqtt_url) != 0 ||
            strcmp(oldCfg.consumer_id, cfg.consumer_id) != 0 ||
            strcmp(oldCfg.device_id, cfg.device_id) != 0 ||
            strcmp(oldCfg.led_layout, cfg.led_layout) != 0;
    }

    self->m_rebootNeeded = self->m_rebootNeeded || rebootNeeded;

    char resp[64];
    snprintf(resp, sizeof(resp), "{\"ok\":true,\"reboot_needed\":%s}",
             self->m_rebootNeeded ? "true" : "false");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, resp);
    return ESP_OK;
}

esp_err_t WebServer::handleReboot(httpd_req_t* req)
{
    auto* self = static_cast<WebServer*>(req->user_ctx);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"ok\":true}");
    self->m_rebootNeeded = false;
    vTaskDelay(pdMS_TO_TICKS(200));
    esp_restart();
    return ESP_OK;
}

esp_err_t WebServer::handleScan(httpd_req_t* req)
{
    auto* self = static_cast<WebServer*>(req->user_ctx);
    self->m_wifi.triggerScan();

    auto* records = new wifi_ap_record_t[16];
    int count = self->m_wifi.getApRecords(records, 16);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr_chunk(req, "[");
    bool first = true;
    for (int i = 0; i < count; i++) {
        if (records[i].ssid[0] == '\0') continue;
        char entry[40];
        snprintf(entry, sizeof(entry), "%s\"%s\"", first ? "" : ",", (char*)records[i].ssid);
        httpd_resp_sendstr_chunk(req, entry);
        first = false;
    }
    httpd_resp_sendstr_chunk(req, "]");
    delete[] records;
    return httpd_resp_sendstr_chunk(req, nullptr);
}

esp_err_t WebServer::handleStatus(httpd_req_t* req)
{
    auto* self = static_cast<WebServer*>(req->user_ctx);

    char ipStr[16] = "0.0.0.0";
    if (self->m_wifi.isConnected()) {
        esp_ip4_addr_t ip = self->m_wifi.getStaIp();
        snprintf(ipStr, sizeof(ipStr), IPSTR, IP2STR(&ip));
    }

    char buf[160];
    snprintf(buf, sizeof(buf),
        "{\"wifi\":%s,\"mqtt\":%s,\"beacon\":%s,\"reboot_needed\":%s,\"ip\":\"%s\"}",
        self->m_wifi.isConnected() ? "true" : "false",
        self->m_mqtt.isConnected() ? "true" : "false",
        self->m_beaconOnline       ? "true" : "false",
        self->m_rebootNeeded       ? "true" : "false",
        ipStr);

    httpd_resp_set_type(req, "application/json");
    return httpd_resp_sendstr(req, buf);
}
