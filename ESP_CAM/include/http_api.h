#pragma once

#include "esp_http_server.h"

/**
 * Новые маршруты. Старые cmd-пути не заменяет. /api/* не регистрируется.
 * false — обязательный слот не встал, сервер нужно остановить.
 */
bool http_api_register(httpd_handle_t server, void *ctx);
