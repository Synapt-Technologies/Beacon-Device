#include "httpServer/HttpHandlers.hpp"
#include "networkConnection/IWifiConnection.hpp"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "cJSON.h"
#include <cstring>
#include <cstdlib>

static constexpr char TAG[] = "HttpHandlers";

// ── Embedded UI assets ────────────────────────────────────────────────────────

extern const uint8_t ui_index_html_start[] asm("_binary_index_html_start");
extern const uint8_t ui_index_html_end[]   asm("_binary_index_html_end");
extern const uint8_t ui_ui_css_start[]     asm("_binary_ui_css_start");
extern const uint8_t ui_ui_css_end[]       asm("_binary_ui_css_end");
extern const uint8_t ui_ui_js_start[]      asm("_binary_ui_js_start");
extern const uint8_t ui_ui_js_end[]        asm("_binary_ui_js_end");

static esp_err_t sendAsset(httpd_req_t* req,
                            const uint8_t* start, const uint8_t* end,
                            const char* contentType)
{
    httpd_resp_set_type(req, contentType);
    size_t size = end - start;
    if (size > 0 && start[size - 1] == '\0') size--; // trim embedded null terminator
    return httpd_resp_send(req, reinterpret_cast<const char*>(start), size);
}

// ── Static asset handlers ─────────────────────────────────────────────────────

esp_err_t HttpHandlers::handleRoot(httpd_req_t* req)
{
    return sendAsset(req, ui_index_html_start, ui_index_html_end,
                     "text/html; charset=utf-8");
}

esp_err_t HttpHandlers::handleCss(httpd_req_t* req)
{
    return sendAsset(req, ui_ui_css_start, ui_ui_css_end,
                     "text/css; charset=utf-8");
}

esp_err_t HttpHandlers::handleJs(httpd_req_t* req)
{
    return sendAsset(req, ui_ui_js_start, ui_ui_js_end,
                     "application/javascript; charset=utf-8");
}

// ── Helpers ───────────────────────────────────────────────────────────────────

static esp_err_t sendJson(httpd_req_t* req, cJSON* root)
{
    char* body = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!body) return httpd_resp_send_500(req);
    httpd_resp_set_type(req, "application/json");
    esp_err_t err = httpd_resp_sendstr(req, body);
    free(body);
    return err;
}

static int readBody(httpd_req_t* req, char* buf, size_t bufLen)
{
    int len = req->content_len < (int)bufLen - 1
                  ? req->content_len
                  : (int)bufLen - 1;
    int received = httpd_req_recv(req, buf, len);
    if (received > 0) buf[received] = '\0';
    return received;
}

// ── GET /api/device ───────────────────────────────────────────────────────────

esp_err_t HttpHandlers::handleGetDevice(httpd_req_t* req)
{
    auto* ctx = static_cast<HttpCtx*>(req->user_ctx);

    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "model",         ctx->profile.model);
    cJSON_AddNumberToObject(root, "deviceType",    static_cast<int>(ctx->profile.deviceType));
    cJSON_AddNumberToObject(root, "consumerCount", ctx->profile.consumerCount);
    return sendJson(req, root);
}

// ── GET /api/config ───────────────────────────────────────────────────────────

esp_err_t HttpHandlers::handleGetConfig(httpd_req_t* req) // TODO add runtime config
{
    auto*           ctx = static_cast<HttpCtx*>(req->user_ctx);
    const Settings& s  = ctx->config.get();
    const int       cn = ctx->profile.consumerCount;
    const int       bn = ctx->profile.deviceType == DeviceType::SINGLE_TOPIC ? 1 : ctx->profile.consumerCount;

    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "deviceName", s.deviceName);

    cJSON* network = cJSON_AddObjectToObject(root, "network");
    cJSON_AddStringToObject(network, "ssid",     s.network.ssid);

    cJSON* beacon = cJSON_AddObjectToObject(root, "beacon");
    cJSON_AddStringToObject(beacon, "mqttUrl", s.beacon.mqttUrl);
    cJSON* consumers = cJSON_AddArrayToObject(beacon, "consumers");
    for (int i = 0; i < bn; i++) {
        cJSON* c = cJSON_CreateObject();
        cJSON_AddStringToObject(c, "consumerId", s.beacon.consumerId[i]);
        cJSON_AddStringToObject(c, "deviceId",   s.beacon.deviceId[i]);
        cJSON_AddItemToArray(consumers, c);
    }

    cJSON* display     = cJSON_AddObjectToObject(root, "display");
    cJSON* brightness  = cJSON_AddArrayToObject(display, "brightness");
    cJSON* alertTarget = cJSON_AddArrayToObject(display, "alertTarget");
    for (int i = 0; i < cn; i++) {
        cJSON_AddItemToArray(brightness,   cJSON_CreateNumber(s.display.brightness[i]));
        cJSON_AddItemToArray(alertTarget,  cJSON_CreateNumber(static_cast<int>(s.display.alertTarget[i])));
    }

    cJSON* runtime     = cJSON_AddObjectToObject(root, "runtime");
    cJSON_AddNumberToObject(runtime, "master_brightness", s.runtime.brightness);
    cJSON_AddNumberToObject(runtime, "state_on_disconnect", static_cast<int>(s.runtime.state_on_disconnect));
    cJSON* names = cJSON_AddArrayToObject(runtime, "name");
    for (int i = 0; i < bn; i++) {
        cJSON* c = cJSON_CreateObject();
        cJSON_AddStringToObject(c, "short", s.runtime.name[i].shortName);
        cJSON_AddStringToObject(c, "long",  s.runtime.name[i].longName);
        cJSON_AddItemToArray(names, c);
    }
    return sendJson(req, root);
}

// ── POST /api/config ──────────────────────────────────────────────────────────

esp_err_t HttpHandlers::handleSetConfig(httpd_req_t* req)
{
    auto* ctx = static_cast<HttpCtx*>(req->user_ctx);

    if (req->content_len <= 0 || req->content_len > 4096) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Empty body");
        return ESP_FAIL;
    }

    char* body = static_cast<char*>(malloc(req->content_len + 1));
    if (!body) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Out of memory");
        return ESP_FAIL;
    }

    int received = readBody(req, body, req->content_len + 1);
    if (received <= 0) {
        free(body);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Empty body");
        return ESP_FAIL;
    }

    cJSON* root = cJSON_Parse(body);
    free(body);
    if (!root) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }



    Settings updated = ctx->config.get();
    const int n      = ctx->profile.consumerCount;

    cJSON* deviceName = cJSON_GetObjectItem(root, "deviceName");
    if (cJSON_IsString(deviceName))
        strncpy(updated.deviceName, deviceName->valuestring, sizeof(updated.deviceName) - 1);

    cJSON* network = cJSON_GetObjectItem(root, "network");
    if (network) {
        cJSON* ssid = cJSON_GetObjectItem(network, "ssid");
        cJSON* pass = cJSON_GetObjectItem(network, "password");
        if (cJSON_IsString(ssid)) strncpy(updated.network.ssid,     ssid->valuestring, sizeof(updated.network.ssid) - 1);
        if (cJSON_IsString(pass)) strncpy(updated.network.password, pass->valuestring, sizeof(updated.network.password) - 1);
    }

    cJSON* beacon = cJSON_GetObjectItem(root, "beacon");
    if (beacon) {
        cJSON* mqttUrl = cJSON_GetObjectItem(beacon, "mqttUrl");
        if (cJSON_IsString(mqttUrl))
            strncpy(updated.beacon.mqttUrl, mqttUrl->valuestring, sizeof(updated.beacon.mqttUrl) - 1);

        cJSON* consumers = cJSON_GetObjectItem(beacon, "consumers");
        if (cJSON_IsArray(consumers)) {
            int i = 0;
            cJSON* entry;
            cJSON_ArrayForEach(entry, consumers) {
                if (i >= n) break;
                cJSON* cid = cJSON_GetObjectItem(entry, "consumerId");
                cJSON* did = cJSON_GetObjectItem(entry, "deviceId");
                if (cJSON_IsString(cid)) strncpy(updated.beacon.consumerId[i], cid->valuestring, sizeof(updated.beacon.consumerId[i]) - 1);
                if (cJSON_IsString(did)) strncpy(updated.beacon.deviceId[i],   did->valuestring, sizeof(updated.beacon.deviceId[i]) - 1);
                i++;
            }
        }
    }

    cJSON* display = cJSON_GetObjectItem(root, "display");
    if (display) {
        cJSON* brightness = cJSON_GetObjectItem(display, "brightness");
        if (cJSON_IsArray(brightness)) {
            int i = 0;
            cJSON* val;
            cJSON_ArrayForEach(val, brightness) {
                if (i >= n) break;
                if (cJSON_IsNumber(val))
                    updated.display.brightness[i] = static_cast<uint8_t>(val->valueint);
                i++;
            }
        }
        cJSON* alertTarget = cJSON_GetObjectItem(display, "alertTarget");
        if (cJSON_IsArray(alertTarget)) {
            int i = 0;
            cJSON* val;
            cJSON_ArrayForEach(val, alertTarget) {
                if (i >= n) break;
                if (cJSON_IsNumber(val))
                    updated.display.alertTarget[i] = static_cast<DeviceAlertTarget>(val->valueint);
                i++;
            }
        }
    }    
    
    
    cJSON* runtime = cJSON_GetObjectItem(root, "runtime");
    if (runtime) {
        cJSON* master_brightness = cJSON_GetObjectItem(runtime, "master_brightness");
        if (cJSON_IsNumber(master_brightness))
            updated.runtime.brightness = static_cast<uint8_t>(master_brightness->valueint);
        cJSON* state_on_disconnect = cJSON_GetObjectItem(runtime, "state_on_disconnect");
        if (cJSON_IsNumber(state_on_disconnect))
            updated.runtime.state_on_disconnect = static_cast<TallyState>(state_on_disconnect->valueint);

        cJSON* names = cJSON_GetObjectItem(runtime, "name");
        if (cJSON_IsArray(names)) {
            int i = 0;
            cJSON* entry;
            cJSON_ArrayForEach(entry, names) {
                if (i >= n) break;
                cJSON* sname = cJSON_GetObjectItem(entry, "short");
                cJSON* lname = cJSON_GetObjectItem(entry, "long");
                if (cJSON_IsString(sname)) strncpy(updated.runtime.name[i].shortName, sname->valuestring, sizeof(updated.runtime.name[i].shortName) - 1);
                if (cJSON_IsString(lname)) strncpy(updated.runtime.name[i].longName, lname->valuestring, sizeof(updated.runtime.name[i].longName) - 1);
                i++;
            }
        }
    }

    cJSON_Delete(root);

    bool saved = ctx->config.apply(updated);
    if (!saved) ESP_LOGW(TAG, "Config applied but save failed");

    cJSON* resp = cJSON_CreateObject();
    cJSON_AddBoolToObject(resp, "ok", saved);
    return sendJson(req, resp);
}

// ── GET /api/status ───────────────────────────────────────────────────────────

esp_err_t HttpHandlers::handleGetStatus(httpd_req_t* req)
{
    auto* ctx = static_cast<HttpCtx*>(req->user_ctx);

    char ipStr[16] = "0.0.0.0";
    if (ctx->network.isConnected()) {
        esp_ip4_addr_t ip = ctx->network.getIp();
        snprintf(ipStr, sizeof(ipStr), IPSTR, IP2STR(&ip));
    }

    cJSON* root = cJSON_CreateObject();
    cJSON_AddBoolToObject  (root, "wifi",   ctx->network.isConnected());
    cJSON_AddStringToObject(root, "ip",     ipStr);
    cJSON_AddBoolToObject  (root, "beacon", ctx->beacon.isConnected());
    return sendJson(req, root);
}

// ── GET /api/scan ─────────────────────────────────────────────────────────────

esp_err_t HttpHandlers::handleStartScan(httpd_req_t* req)
{
    auto* ctx  = static_cast<HttpCtx*>(req->user_ctx);
    auto* wifi = ctx->network.asWifi();

    cJSON* root = cJSON_CreateObject();
    bool ok = wifi != nullptr;
    if (wifi) {
        wifi->triggerScan();
    }
    cJSON_AddBoolToObject(root, "ok", ok);
    cJSON_AddBoolToObject(root, "scanning", wifi ? wifi->isScanInProgress() : false);
    return sendJson(req, root);
}

// ── GET /api/scan ─────────────────────────────────────────────────────────────

esp_err_t HttpHandlers::handleGetScan(httpd_req_t* req)
{
    auto* ctx  = static_cast<HttpCtx*>(req->user_ctx);
    auto* wifi = ctx->network.asWifi();

    cJSON* root = cJSON_CreateObject();
    cJSON_AddBoolToObject(root, "scanning", wifi ? wifi->isScanInProgress() : false);
    cJSON* resultsJson = cJSON_AddArrayToObject(root, "results");

    if (wifi) {
        auto* results = static_cast<WifiScanResult*>(malloc(32 * sizeof(WifiScanResult)));
        if (!results) {
            cJSON_Delete(root);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Out of memory");
            return ESP_FAIL;
        }

        int count = wifi->getScanResults(results, 32);
        for (int i = 0; i < count; i++) {
            if (results[i].ssid[0] == '\0') continue;
            cJSON* entry = cJSON_CreateObject();
            cJSON_AddStringToObject(entry, "ssid", results[i].ssid);
            cJSON_AddNumberToObject(entry, "rssi", results[i].rssi);
            cJSON_AddItemToArray(resultsJson, entry);
        }
        free(results);
    }
    return sendJson(req, root);
}

// ── POST /api/reboot ──────────────────────────────────────────────────────────

esp_err_t HttpHandlers::handleReboot(httpd_req_t* req)
{
    cJSON* root = cJSON_CreateObject();
    cJSON_AddBoolToObject(root, "ok", true);
    sendJson(req, root);

    vTaskDelay(pdMS_TO_TICKS(200));
    esp_restart();
    return ESP_OK;
}
