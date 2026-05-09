#include "orchestrator/SateliteOrchestrator.hpp"
#include "networkConnection/IWifiConnection.hpp"
#include "consumer/IDisplayConsumer.hpp"

#include "esp_log.h"
#include "esp_mac.h"
#include <cstdio>
#include <cstring>


// ? Lifecycle

void SateliteOrchestrator::start()
{
    // TODO Add checks. e.g. deviceType can only be Single in this orchestrator.


    _config.onNetworkChanged([this](const Settings::Network& s){ onNetworkChanged(s); });
    _config.onBeaconChanged ([this](const Settings::Beacon&  s){ onBeaconChanged(s);  });
    _config.onDisplayChanged([this](const Settings::Display& s){ onDisplayChanged(s); });
    _config.onRuntimeChanged([this](const Settings::Runtime& s){ onRuntimeChanged(s); });

    _beacon.setTallyCallback(
        [this](TallyState state) { applyTally(state); }
    );
    _beacon.setAlertCallback(
        [this](DeviceAlertAction a, DeviceAlertTarget t, uint32_t ms) { applyAlert(a, t, ms); }
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

    _network.setConnectionCallback( // TODO Check if needed. For the ui? Should it be stored in the INetworkConnection implementation?
        [this](NetworkStatus s, esp_ip4_addr_t ip) { onNetworkStatus(s, ip); }
    ); 

    // TODO Init inside of ochestrator? Probably yes because the callbacks.
    // TODO Check order.
    _network.start();

    _config.load();

    _http.start();
    registerHttpHandlers(); // TODO Build context.

    ESP_LOGI(TAG, "Started");
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
    // _beacon.stop(); // TODO Check if needed.
}

void SateliteOrchestrator::onBeaconChanged(const Settings::Beacon& s)
{
    ESP_LOGI(TAG, "Beacon settings changed, reconnecting");

    // if (s.mqttUrl[0] != '\0') { // TODO more safeties.
        _beacon.setBaseAddress(s.mqttUrl);
    // } else {
        // _beacon.stop();
        // return;
    // }

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

    // if (_wifi.isConnected()) { // TODO Check if beaconConnection handles disconnects well.
    //     char url[128];
    //     strncpy(url, s.mqttUrl, sizeof(url) - 1);
    //     _beacon.start();
    // }
}

void SateliteOrchestrator::onDisplayChanged(const Settings::Display& s)
{
    for (int i = 0; i < _consumerCount; i++) {
        _consumers[i]->setBrightness(s.brightness[i]);
        // TODO Add alert target handeling. Not implemented because of multi target consumers.
    }
}

void SateliteOrchestrator::onRuntimeChanged(const Settings::Runtime& s)
{

    ESP_LOGI(TAG, "Runtime settings changed: Shortname: %s, Longname: %s", s.name[0].shortName, s.name[0].longName);

    // for (int i = 0; i < _consumerCount; i++) { // TODO add master brightness
    //     _consumers[i]->setBrightness(s.brightness);
    //     // TODO Add alert target handeling. Not implemented because of multi target consumers.
    // }

    for (int i = 0; i < _consumerCount; i++) {
        if (auto* d = _consumers[i]->asDisplay()) {
            d->setText(s.name[0].shortName, 0, 0);
            d->setText(s.name[0].longName,  1, 0);
        }
    }
}

// ? Runtime Callbacks
void SateliteOrchestrator::applyTally(TallyState state)
{
    ESP_LOGI(TAG, "Applying tally state: %d", static_cast<int>(state));

    for (int i = 0; i < _consumerCount; i++) {
        _consumers[i]->setState(state);
    }
}

void SateliteOrchestrator::applyAlert(DeviceAlertAction action,
                                       DeviceAlertTarget target,
                                       uint32_t timeout)
{
    ESP_LOGI(TAG, "Applying alert: %d", static_cast<int>(action));
    for (int i = 0; i < _consumerCount; i++) {
        _consumers[i]->setAlert(action, target, timeout);
    }
}



// ? Network Callbacks


void SateliteOrchestrator::onNetworkStatus(NetworkStatus status, esp_ip4_addr_t ip)
{
    _networkStatus = status;
    this->_networkIp = ip;

    if (status == NetworkStatus::CONNECTED) {
        _beacon.start();
    } else {
        _beacon.stop();
    }
}


// ? HTTP
// TODO Handled here?

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
