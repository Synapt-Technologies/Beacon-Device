#pragma once

#include "esp_http_server.h"
#include "config/Config.hpp"
#include "config/DeviceProfile.hpp"
#include "networkConnection/INetworkConnection.hpp"
#include "beaconConnection/IBeaconConnection.hpp"

typedef struct HttpCtx {
    Config&              config;
    const DeviceProfile& profile;
    INetworkConnection&  network;
    IBeaconConnection&   beacon;
} HttpCtx;

namespace HttpHandlers {

    // Static assets
    esp_err_t handleRoot(httpd_req_t* req);
    esp_err_t handleCss (httpd_req_t* req);
    esp_err_t handleJs  (httpd_req_t* req);

    // API
    esp_err_t handleGetDevice(httpd_req_t* req); // GET  /api/device
    esp_err_t handleGetConfig(httpd_req_t* req); // GET  /api/config
    esp_err_t handleSetConfig(httpd_req_t* req); // POST /api/config
    esp_err_t handleGetStatus(httpd_req_t* req); // GET  /api/status
    esp_err_t handleStartScan(httpd_req_t* req); // POST /api/scan/start
    esp_err_t handleGetScan  (httpd_req_t* req); // GET  /api/scan
    esp_err_t handleReboot   (httpd_req_t* req); // POST /api/reboot

}
