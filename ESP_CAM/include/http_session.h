#pragma once

#include "esp_http_server.h"

/** POST /api/auth/challenge. Общий nonce HttpAuth, без чтения cookie и Bearer. */
esp_err_t http_auth_challenge(httpd_req_t *req);

/** POST /api/auth/login. Неверный JSON не вызывает HttpAuth::login. */
esp_err_t http_auth_login(httpd_req_t *req);

/** POST /api/auth/logout. Сессию гасит HttpAuth::logout, оси — sessionWatchTask. */
esp_err_t http_auth_logout(httpd_req_t *req);

/** PUT /api/auth/password. Неверный JSON не вызывает HttpAuth::setPassword. */
esp_err_t http_auth_password(httpd_req_t *req);

/** PUT /api/auth/session/ttl. Диапазон проверяется до записи. */
esp_err_t http_auth_session_ttl(httpd_req_t *req);

/** GET /api/auth/token. Token в ответ попадает только из уже сохранённого значения. */
esp_err_t http_auth_token(httpd_req_t *req);

/** POST /api/auth/token/reissue. При ошибке записи прежний token остаётся. */
esp_err_t http_auth_reissue(httpd_req_t *req);
