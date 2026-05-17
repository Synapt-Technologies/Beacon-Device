#include "orchestrator/SateliteOrchestrator.hpp"
#include "networkConnection/IWifiConnection.hpp"

#include "esp_log.h"
#include "esp_mac.h"
#include <cstdio>
#include <cstring>


// ? Lifecycle

void SateliteOrchestrator::doStart()
{
    for (int i = 0; i < _groupCount; i++)
        _groups[i]->init();

    _config.onNetworkChanged([this](const Settings::Network& s){ onNetworkChanged(s); });
    _config.onBeaconChanged ([this](const Settings::Beacon&  s){ onBeaconChanged(s);  });
    _config.onDisplayChanged([this](const Settings::Display& s){ onDisplayChanged(s); });
    _config.onRuntimeChanged([this](const Settings::Runtime& s){ onRuntimeChanged(s); });

    _beacon.setTallyCallback(
        [this](TallyState state) { applyTally(state); }
    );
    _beacon.setAlertCallback(
        [this](DeviceAlertType type, DeviceAlertAction action,
               DeviceAlertTarget target, uint32_t ms, const char* text) {
            applyAlert(type, action, target, ms, text);
        }
    );
    _beacon.setNameCallback(
        [this](const char* s, const char* l) {
            Settings::Runtime r = _config.get().runtime;
            strncpy(r.name[0].shortName, s, sizeof(r.name[0].shortName) - 1);
            r.name[0].shortName[sizeof(r.name[0].shortName) - 1] = '\0';
            strncpy(r.name[0].longName, l, sizeof(r.name[0].longName) - 1);
            r.name[0].longName[sizeof(r.name[0].longName) - 1] = '\0';

            _config.applyRuntime(r);
        }
    );
    _beacon.setConnectionCallback(
        [this](BeaconStatus s) { onBeaconStatus(s); }
    );

    _network.setConnectionCallback(
        [this](NetworkStatus s, esp_ip4_addr_t ip) { onNetworkStatus(s, ip); }
    );

    _network.start();
    _config.load();
    _http.start();
    registerHttpHandlers();

    ESP_LOGI(TAG, "Started");
    ESP_LOGI("main", "Stack HWM: %lu bytes free", uxTaskGetStackHighWaterMark(NULL) * sizeof(StackType_t));
}

void SateliteOrchestrator::stop()
{
    _http.stop();
    _beacon.stop();
    _network.stop();
    ESP_LOGI(TAG, "Stopped");
}

// ? Config callbacks

void SateliteOrchestrator::onNetworkChanged(const Settings::Network& s)
{
    ESP_LOGI(TAG, "Network settings changed, reconfiguring WiFi");
    if (auto* wifi = _network.asWifi())
        wifi->configure(s.ssid, s.password);
}

void SateliteOrchestrator::onBeaconChanged(const Settings::Beacon& s)
{
    ESP_LOGI(TAG, "Beacon settings changed, reconnecting");
    _beacon.setBaseAddress(s.mqttUrl);

    char consumerId[48];
    if (s.consumerId[0][0] != '\0') {
        strncpy(consumerId, s.consumerId[0], sizeof(consumerId) - 1);
    } else {
        strncpy(consumerId, "aedes", sizeof(consumerId) - 1);
    }

    char deviceId[48];
    if (s.deviceId[0][0] != '\0') {
        strncpy(deviceId, s.deviceId[0], sizeof(deviceId) - 1);
    } else {
        uint8_t mac[6];
        esp_read_mac(mac, ESP_MAC_WIFI_STA);
        snprintf(deviceId, sizeof(deviceId), "%02x%02x%02x%02x%02x%02x",
                 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    }
    _beacon.setAddress(consumerId, deviceId);
}

void SateliteOrchestrator::onDisplayChanged(const Settings::Display& s)
{
    for (int i = 0; i < _groupCount; i++)
        _groups[i]->setMasterBrightness(s.brightness[i]);
}

void SateliteOrchestrator::onRuntimeChanged(const Settings::Runtime& s)
{
    ESP_LOGI(TAG, "Runtime settings changed: Shortname: %s, Longname: %s",
             s.name[0].shortName, s.name[0].longName);


    // TODO: More runtime. E.g. state_on_disconnect, flip_sides, etc.
    for (int i = 0; i < _groupCount; i++) {
        _groups[i]->setText(0, s.name[0].shortName);
        _groups[i]->setText(1, s.name[0].longName);
    }
}

// ? Runtime Callbacks

void SateliteOrchestrator::applyTally(TallyState state)
{
    ESP_LOGI(TAG, "Applying tally state: %d", static_cast<int>(state));
    for (int i = 0; i < _groupCount; i++)
        _groups[i]->setState(state);
}

void SateliteOrchestrator::applyAlert(DeviceAlertType type, DeviceAlertAction action,
                                      DeviceAlertTarget target, uint32_t timeout,
                                      const char* text)
{
    ESP_LOGI(TAG, "Applying alert: type=%d action=%d", static_cast<int>(type), static_cast<int>(action));
    for (int i = 0; i < _groupCount; i++) {
        if (type == DeviceAlertType::COLOR || type == DeviceAlertType::BOTH) {
            if (action == DeviceAlertAction::CLEAR)
                _groups[i]->clearColorAlert();
            else
                _groups[i]->setColorAlert(action, target, timeout);
        }
        if (type == DeviceAlertType::TEXT || type == DeviceAlertType::BOTH) {
            if (action == DeviceAlertAction::CLEAR)
                _groups[i]->clearTextAlert(target);
            else
                _groups[i]->setTextAlert(text ? text : "", target, timeout);
        }
    }
}

// ? Network Callbacks

void SateliteOrchestrator::onBeaconStatus(BeaconStatus status)
{
    ESP_LOGI(TAG, "Beacon status changed: %d", static_cast<int>(status));
    _beaconStatus = status;

    if (status != BeaconStatus::CONNECTED)
        applyTally(_config.get().runtime.state_on_disconnect);
}

void SateliteOrchestrator::onNetworkStatus(NetworkStatus status, esp_ip4_addr_t ip)
{
    ESP_LOGI(TAG, "Network status changed: %d", static_cast<int>(status));
    _networkStatus = status;
    _networkIp = ip;

    if (status == NetworkStatus::CONNECTED)
        _beacon.start();
    else
        _beacon.stop();
}

// ? HTTP

void SateliteOrchestrator::registerHttpHandlers()
{
    _http.registerHandler("/",           HTTP_GET,  HttpHandlers::handleRoot,      nullptr);
    _http.registerHandler("/ui.css",     HTTP_GET,  HttpHandlers::handleCss,       nullptr);
    _http.registerHandler("/ui.js",      HTTP_GET,  HttpHandlers::handleJs,        nullptr);
    _http.registerHandler("/api/device", HTTP_GET,  HttpHandlers::handleGetDevice, &_httpCtx);
    _http.registerHandler("/api/config", HTTP_GET,  HttpHandlers::handleGetConfig, &_httpCtx);
    _http.registerHandler("/api/config", HTTP_POST, HttpHandlers::handleSetConfig, &_httpCtx);
    _http.registerHandler("/api/status", HTTP_GET,  HttpHandlers::handleGetStatus, &_httpCtx);
    _http.registerHandler("/api/scan/start", HTTP_POST, HttpHandlers::handleStartScan, &_httpCtx);
    _http.registerHandler("/api/scan",   HTTP_GET,  HttpHandlers::handleGetScan,   &_httpCtx);
    _http.registerHandler("/api/reboot", HTTP_POST, HttpHandlers::handleReboot,    nullptr);
}
