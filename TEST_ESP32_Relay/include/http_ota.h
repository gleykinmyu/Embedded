#pragma once

#include "esp_err.h"
#include "esp_http_server.h"

class HttpOta {
public:
    static void confirm_running();
    static void print_status();
    static esp_err_t post_update(httpd_req_t *req);
    static esp_err_t get_status(httpd_req_t *req);
};
