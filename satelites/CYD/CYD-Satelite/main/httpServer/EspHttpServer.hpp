#pragma once

#include "esp_http_server.h"
#include <stdint.h>

class EspHttpServer {
public:
    EspHttpServer()  = default;
    ~EspHttpServer() { stop(); }

    void start(uint16_t port = 80);
    void stop();
    bool isRunning() const;

    void registerHandler(const char*        uri,
                         httpd_method_t     method,
                         esp_err_t          (*handler)(httpd_req_t*),
                         void*              ctx = nullptr);

private:
    static constexpr char TAG[]             = "EspHttpServer";
    static constexpr int  MAX_URI_HANDLERS  = 12;

    httpd_handle_t _server = nullptr;
};
