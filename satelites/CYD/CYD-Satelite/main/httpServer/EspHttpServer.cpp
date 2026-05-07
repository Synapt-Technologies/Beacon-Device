#include "httpServer/EspHttpServer.hpp"
#include "esp_log.h"

void EspHttpServer::start(uint16_t port)
{
    if (_server) return;

    httpd_config_t cfg   = HTTPD_DEFAULT_CONFIG();
    cfg.server_port      = port;
    cfg.max_uri_handlers = MAX_URI_HANDLERS;
    cfg.stack_size       = 8192;

    if (httpd_start(&_server, &cfg) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start on port %d", port);
        return;
    }
    ESP_LOGI(TAG, "Started on port %d", port);
}

void EspHttpServer::stop()
{
    if (!_server) return;
    httpd_stop(_server);
    _server = nullptr;
    ESP_LOGI(TAG, "Stopped");
}

bool EspHttpServer::isRunning() const { return _server != nullptr; }

void EspHttpServer::registerHandler(const char* uri, httpd_method_t method,
                                    esp_err_t (*handle)(httpd_req_t*), void* ctx)
{
    if (!_server) {
        ESP_LOGW(TAG, "registerHandler() called before start()");
        return;
    }
    httpd_uri_t u = { uri, method, handle, ctx };
    if (httpd_register_uri_handler(_server, &u) != ESP_OK)
        ESP_LOGE(TAG, "Failed to register: %s", uri);
}
