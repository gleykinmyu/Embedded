#include "http_session.h"

#include <cstdio>
#include <cstring>

#include "cJSON.h"
#include "esp_http_server.h"
#include "http_auth.h"
#include "http_body.h"
#include "http_gate.h"
#include "http_reply.h"
#include "http_status.h"

namespace {

HttpAuth *auth_of(httpd_req_t *req)
{
    if (!req || !req->user_ctx) {
        return nullptr;
    }
    return &static_cast<HttpStatus *>(req->user_ctx)->auth();
}

bool hex64(const char *s)
{
    if (!s || std::strlen(s) != 64) {
        return false;
    }
    for (size_t i = 0; i < 64; ++i) {
        const char c = s[i];
        const bool digit = c >= '0' && c <= '9';
        const bool lower = c >= 'a' && c <= 'f';
        const bool upper = c >= 'A' && c <= 'F';
        if (!digit && !lower && !upper) {
            return false;
        }
    }
    return true;
}

esp_err_t invalid(httpd_req_t *req, const char *message)
{
    return http_send_error(req, HttpError::InvalidParameter, message ? message : "invalid parameter");
}

esp_err_t gate_operator(httpd_req_t *req)
{
    HttpAuth *auth = auth_of(req);
    if (!auth) {
        return http_send_error(req, HttpError::Internal, "internal");
    }
    if (auth->operatorSession(req, false)) {
        return ESP_OK;
    }
    if (auth->bearerMatches(req)) {
        return http_send_error(req, HttpError::Forbidden, "forbidden");
    }
    return http_send_error(req, HttpError::Unauthorized, "unauthorized");
}

esp_err_t read_object(httpd_req_t *req, HttpBody *body)
{
    char why[48] = {};
    const esp_err_t rd = http_read_object(req, body, why, sizeof(why));
    if (rd == ESP_OK) {
        return ESP_OK;
    }
    http_body_free(body);
    if (rd == ESP_FAIL || rd == ESP_ERR_NO_MEM || rd == ESP_ERR_INVALID_SIZE ||
        rd == ESP_ERR_NOT_SUPPORTED) {
        const bool mem = rd == ESP_ERR_NO_MEM;
        (void)http_send_error(req, mem ? HttpError::Buffer : HttpError::InvalidParameter,
                              mem ? "response buffer" : (why[0] ? why : "invalid parameter"));
        return ESP_FAIL;
    }
    return invalid(req, why[0] ? why : "invalid parameter");
}

bool json_int(const cJSON *item, int *out)
{
    if (!cJSON_IsNumber(item) || !out) {
        return false;
    }
    const double v = item->valuedouble;
    if (v < -2147483648.0 || v > 2147483647.0) {
        return false;
    }
    const int n = item->valueint;
    if (static_cast<double>(n) != v) {
        return false;
    }
    *out = n;
    return true;
}

bool token32(const char *s)
{
    if (!s || std::strlen(s) != HttpAuth::kTokenLen) {
        return false;
    }
    for (size_t i = 0; i < HttpAuth::kTokenLen; ++i) {
        const char c = s[i];
        const bool digit = c >= '0' && c <= '9';
        const bool lower = c >= 'a' && c <= 'z';
        const bool upper = c >= 'A' && c <= 'Z';
        if (!digit && !lower && !upper) {
            return false;
        }
    }
    return true;
}

esp_err_t send_token(httpd_req_t *req, const char *token)
{
    if (!token32(token)) {
        return http_send_error(req, HttpError::Internal, "internal");
    }
    char data[64];
    const int n = snprintf(data, sizeof(data), "{\"token\":\"%s\"}", token);
    if (n <= 0 || static_cast<size_t>(n) >= sizeof(data)) {
        return http_send_error(req, HttpError::Internal, "internal");
    }
    http_slide_operator(req);
    return http_send_ok(req, data);
}

} // namespace

esp_err_t http_auth_challenge(httpd_req_t *req)
{
    if (!http_ui_client(req)) {
        return http_send_error(req, HttpError::Unauthorized, "unauthorized");
    }
    HttpAuth *auth = auth_of(req);
    if (!auth) {
        return http_send_error(req, HttpError::Internal, "internal");
    }
    char salt[40] = {};
    char nonce[40] = {};
    char next_salt[40] = {};
    /* Отказ до записи nonce, если хеш пароля ещё не готов или буферы пусты. */
    if (!auth->challenge(salt, sizeof(salt), nonce, sizeof(nonce), next_salt, sizeof(next_salt))) {
        return http_send_error(req, HttpError::Internal, "internal");
    }
    char data[160];
    const int n = snprintf(data, sizeof(data),
                           "{\"salt\":\"%s\",\"nonce\":\"%s\",\"next_salt\":\"%s\"}",
                           salt, nonce, next_salt);
    if (n <= 0 || static_cast<size_t>(n) >= sizeof(data)) {
        /* Nonce уже выдан. Второй объект ответа не собираем. */
        return http_send_error(req, HttpError::Internal, "internal");
    }
    return http_send_ok(req, data);
}

esp_err_t http_auth_login(httpd_req_t *req)
{
    if (!http_ui_client(req)) {
        return http_send_error(req, HttpError::Unauthorized, "unauthorized");
    }
    HttpAuth *auth = auth_of(req);
    if (!auth) {
        return http_send_error(req, HttpError::Internal, "internal");
    }

    HttpBody body;
    char why[48] = {};
    const esp_err_t rd = http_read_object(req, &body, why, sizeof(why));
    if (rd != ESP_OK) {
        http_body_free(&body);
        if (rd == ESP_FAIL || rd == ESP_ERR_NO_MEM || rd == ESP_ERR_INVALID_SIZE ||
            rd == ESP_ERR_NOT_SUPPORTED) {
            const bool mem = rd == ESP_ERR_NO_MEM;
            (void)http_send_error(req, mem ? HttpError::Buffer : HttpError::InvalidParameter,
                                  mem ? "response buffer" : (why[0] ? why : "invalid parameter"));
            return ESP_FAIL;
        }
        return invalid(req, why[0] ? why : "invalid parameter");
    }

    static const char *kKeys[] = {"user", "proof", nullptr};
    cJSON *user = nullptr;
    cJSON *proof = nullptr;
    if (!http_reject_unknown(body.root, kKeys)) {
        user = http_require(body.root, "user", cJSON_String);
        proof = http_require(body.root, "proof", cJSON_String);
    }
    const char *user_s = user ? user->valuestring : nullptr;
    const char *proof_s = proof ? proof->valuestring : nullptr;
    /* kUserMaxLen — байты. OpenAPI считает символы; алфавит контрактом не задан. */
    const size_t user_n = user_s ? std::strlen(user_s) : 0;
    if (!user_s || !proof_s || user_n < 1 || user_n > HttpAuth::kUserMaxLen || !hex64(proof_s)) {
        http_body_free(&body);
        return invalid(req, "invalid parameter");
    }

    char user_buf[HttpAuth::kUserMaxLen + 1];
    char proof_buf[65];
    std::memcpy(user_buf, user_s, user_n + 1);
    std::memcpy(proof_buf, proof_s, 65);
    http_body_free(&body);

    /* Тело успеха собирается до login: при нехватке памяти nonce и сессия не меняются. */
    cJSON *data = cJSON_CreateObject();
    if (!data) {
        std::memset(proof_buf, 0, sizeof(proof_buf));
        return http_send_error(req, HttpError::Buffer, "response buffer");
    }
    const bool built = cJSON_AddStringToObject(data, "user", user_buf) != nullptr &&
                       cJSON_AddNumberToObject(data, "ttl_seconds", auth->sessionTtlSec()) != nullptr;
    char *data_json = built ? cJSON_PrintUnformatted(data) : nullptr;
    cJSON_Delete(data);
    if (!data_json) {
        std::memset(proof_buf, 0, sizeof(proof_buf));
        return http_send_error(req, HttpError::Buffer, "response buffer");
    }

    char sess[HttpAuth::kSessLen + 1] = {};
    const bool opened = auth->login(user_buf, proof_buf, sess, sizeof(sess));
    std::memset(proof_buf, 0, sizeof(proof_buf));
    if (!opened) {
        cJSON_free(data_json);
        return http_send_error(req, HttpError::Unauthorized, "unauthorized");
    }

    /*
     * Сессия уже создана. http_send_ok выделяет конверт после этого.
     * Ошибка Set-Cookie или отправки не откатывает сессию: сторож
     * уже мог забрать конец предыдущей сессии. Клиенту уходит один ответ.
     */
    char cookie[96];
    const int cn = snprintf(cookie, sizeof(cookie), "esp_sess=%s; Path=/; HttpOnly; SameSite=Lax", sess);
    if (cn <= 0 || static_cast<size_t>(cn) >= sizeof(cookie) ||
        httpd_resp_set_hdr(req, "Set-Cookie", cookie) != ESP_OK) {
        cJSON_free(data_json);
        return http_send_error(req, HttpError::Internal, "internal");
    }
    const esp_err_t sent = http_send_ok(req, data_json);
    cJSON_free(data_json);
    return sent;
}

esp_err_t http_auth_logout(httpd_req_t *req)
{
    const esp_err_t gate = gate_operator(req);
    if (gate != ESP_OK) {
        return gate;
    }
    HttpAuth *auth = auth_of(req);
    if (!auth) {
        return http_send_error(req, HttpError::Internal, "internal");
    }
    auth->logout();
    if (httpd_resp_set_hdr(req, "Set-Cookie",
                           "esp_sess=; Path=/; Max-Age=0; HttpOnly; SameSite=Lax") != ESP_OK) {
        return http_send_error(req, HttpError::Internal, "internal");
    }
    return http_send_ok(req, "{\"applied\":true}");
}

esp_err_t http_auth_password(httpd_req_t *req)
{
    const esp_err_t gate = gate_operator(req);
    if (gate != ESP_OK) {
        return gate;
    }
    HttpAuth *auth = auth_of(req);
    if (!auth) {
        return http_send_error(req, HttpError::Internal, "internal");
    }

    HttpBody body;
    const esp_err_t rd = read_object(req, &body);
    if (rd != ESP_OK) {
        return rd;
    }
    static const char *kKeys[] = {"proof", "new_password_hash", nullptr};
    cJSON *proof = nullptr;
    cJSON *next = nullptr;
    if (!http_reject_unknown(body.root, kKeys)) {
        proof = http_require(body.root, "proof", cJSON_String);
        next = http_require(body.root, "new_password_hash", cJSON_String);
    }
    const char *proof_s = proof ? proof->valuestring : nullptr;
    const char *next_s = next ? next->valuestring : nullptr;
    if (!hex64(proof_s) || !hex64(next_s)) {
        http_body_free(&body);
        return invalid(req, "invalid parameter");
    }
    char proof_buf[65];
    char hash_buf[65];
    std::memcpy(proof_buf, proof_s, 65);
    std::memcpy(hash_buf, next_s, 65);
    http_body_free(&body);

    const AuthWrite wr = auth->setPassword(proof_buf, hash_buf);
    std::memset(proof_buf, 0, sizeof(proof_buf));
    std::memset(hash_buf, 0, sizeof(hash_buf));
    if (wr == AuthWrite::Rejected) {
        return http_send_error(req, HttpError::Unauthorized, "unauthorized");
    }
    if (wr != AuthWrite::Applied) {
        return http_send_error(req, HttpError::Internal, "internal");
    }
    http_slide_operator(req);
    return http_send_ok(req, "{\"applied\":true}");
}

esp_err_t http_auth_session_ttl(httpd_req_t *req)
{
    const esp_err_t gate = gate_operator(req);
    if (gate != ESP_OK) {
        return gate;
    }
    HttpAuth *auth = auth_of(req);
    if (!auth) {
        return http_send_error(req, HttpError::Internal, "internal");
    }

    HttpBody body;
    const esp_err_t rd = read_object(req, &body);
    if (rd != ESP_OK) {
        return rd;
    }
    static const char *kKeys[] = {"ttl_seconds", nullptr};
    cJSON *ttl = nullptr;
    if (!http_reject_unknown(body.root, kKeys)) {
        ttl = http_require(body.root, "ttl_seconds", cJSON_Number);
    }
    int sec = 0;
    const bool in_range = json_int(ttl, &sec) && sec >= static_cast<int>(HttpAuth::kSessTtlMinSec) &&
                          sec <= static_cast<int>(HttpAuth::kSessTtlMaxSec);
    http_body_free(&body);
    if (!in_range) {
        return invalid(req, "invalid parameter");
    }
    if (!auth->setSessionTtlSec(static_cast<uint32_t>(sec))) {
        return http_send_error(req, HttpError::Internal, "internal");
    }
    char data[40];
    const int n = snprintf(data, sizeof(data), "{\"ttl_seconds\":%d}", sec);
    if (n <= 0 || static_cast<size_t>(n) >= sizeof(data)) {
        return http_send_error(req, HttpError::Internal, "internal");
    }
    http_slide_operator(req);
    return http_send_ok(req, data);
}

esp_err_t http_auth_token(httpd_req_t *req)
{
    const esp_err_t gate = gate_operator(req);
    if (gate != ESP_OK) {
        return gate;
    }
    HttpAuth *auth = auth_of(req);
    if (!auth) {
        return http_send_error(req, HttpError::Internal, "internal");
    }
    return send_token(req, auth->apiToken());
}

esp_err_t http_auth_reissue(httpd_req_t *req)
{
    const esp_err_t gate = gate_operator(req);
    if (gate != ESP_OK) {
        return gate;
    }
    HttpAuth *auth = auth_of(req);
    if (!auth) {
        return http_send_error(req, HttpError::Internal, "internal");
    }
    if (!auth->reissueToken()) {
        return http_send_error(req, HttpError::Internal, "internal");
    }
    return send_token(req, auth->apiToken());
}
