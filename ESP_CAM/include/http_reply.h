#pragma once

#include "esp_err.h"
#include "esp_http_server.h"
#include "transport/types.hpp"

/**
 * Исходящий JSON нового API. Входное тело сюда не входит.
 * Успешная отправка возвращает ESP_OK. Повторно ответ не пишется.
 */
enum class HttpError {
    InvalidParameter,
    Unauthorized,
    Forbidden,
    Unsupported,
    ProtocolDisabled,
    NotFound,
    DeviceRejected,
    Buffer,
    Internal,
    OtaBusy,
    DeviceIo,
    CameraUnavailable,
    DeviceTimeout,
};

/** data_json — уже собранное JSON-значение, без конверта. */
esp_err_t http_send_ok(httpd_req_t *req, const char *data_json);
esp_err_t http_send_error(httpd_req_t *req, HttpError err, const char *message);
/** Ok → 200 {"ok":true,"data":{}}. Остальные Status → конверт ошибки. */
esp_err_t http_send_status(httpd_req_t *req, ccam::Status st);

/**
 * 405. В контракте нет отдельного кода: HTTP 405, code invalid_parameter,
 * message "method not allowed".
 */
esp_err_t http_send_method_not_allowed(httpd_req_t *req);

/**
 * Кадр с чужим Transfer-Encoding или телом длиннее 1024 байт.
 * Отправляет один 400 и возвращает ESP_FAIL: сокет закрывается, остаток не вычитывается.
 * Обычный кадр — ESP_OK, ответ не отправляется.
 */
esp_err_t http_close_if_bad_frame(httpd_req_t *req);

esp_err_t http_on_404(httpd_req_t *req, httpd_err_code_t error);
esp_err_t http_on_405(httpd_req_t *req, httpd_err_code_t error);
