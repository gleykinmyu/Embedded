#pragma once

#include "esp_http_server.h"

class EthW5500;
class RelayBoard;

class HttpUi {
public:
    HttpUi(RelayBoard &relays, EthW5500 &eth);
    void start();
    void stop();
    bool listening() const { return srv_ != nullptr; }

private:
    void add(const char *uri, esp_err_t (*handler)(httpd_req_t *));
    esp_err_t send_json(httpd_req_t *req);
    esp_err_t on_root(httpd_req_t *req);
    esp_err_t on_relays(httpd_req_t *req);
    esp_err_t on_relay(httpd_req_t *req);
    esp_err_t on_chase_start(httpd_req_t *req);
    esp_err_t on_chase_stop(httpd_req_t *req);

    static HttpUi &self(httpd_req_t *req);
    static esp_err_t root_get(httpd_req_t *req);
    static esp_err_t relays_get(httpd_req_t *req);
    static esp_err_t relay_get(httpd_req_t *req);
    static esp_err_t chase_start_get(httpd_req_t *req);
    static esp_err_t chase_stop_get(httpd_req_t *req);

    RelayBoard &relays_;
    EthW5500 &eth_;
    httpd_handle_t srv_{};
};
