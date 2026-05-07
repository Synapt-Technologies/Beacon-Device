#pragma once
#include "interfaces.h"
#include "esp_http_server.h"

class BeaconApp;

class WebServer {
public:
    WebServer(IConfig& config, IWifiManager& wifi, IMqttManager& mqtt);
    ~WebServer();

    void start();
    void stop();

    void setBeaconOnline(bool online) { m_beaconOnline = online; }
    void setBeaconApp(BeaconApp* app) { m_app = app; }

private:
    IConfig&       m_config;
    IWifiManager&  m_wifi;
    IMqttManager&  m_mqtt;
    BeaconApp*     m_app          = nullptr;
    httpd_handle_t m_server       = nullptr;
    bool           m_beaconOnline = false;
    bool           m_rebootNeeded = false;

    static esp_err_t handleRoot  (httpd_req_t* req);
    static esp_err_t handleUiCss (httpd_req_t* req);
    static esp_err_t handleUiJs  (httpd_req_t* req);
    static esp_err_t handleGetCfg(httpd_req_t* req);
    static esp_err_t handleSetCfg(httpd_req_t* req);
    static esp_err_t handleReboot(httpd_req_t* req);
    static esp_err_t handleScan  (httpd_req_t* req);
    static esp_err_t handleStatus(httpd_req_t* req);
};
