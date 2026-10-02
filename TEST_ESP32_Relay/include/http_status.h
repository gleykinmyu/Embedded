#pragma once

#include "esp_http_server.h"

class EthLan8720;

class HttpStatus {
public:
    explicit HttpStatus(EthLan8720 &eth);
    void start();
    void stop();
    bool listening() const { return srv_ != nullptr; }

private:
    esp_err_t on_root(httpd_req_t *req);
    esp_err_t on_status(httpd_req_t *req);

    static HttpStatus &self(httpd_req_t *req);
    static esp_err_t root_get(httpd_req_t *req);
    static esp_err_t status_get(httpd_req_t *req);

    EthLan8720 &eth_;
    httpd_handle_t srv_{};
};
