#pragma once

#include "esp_http_server.h"

/** GET /api/status. Снимок уже известного состояния, без шины и без DeviceLock. */
esp_err_t http_get_status(httpd_req_t *req);
