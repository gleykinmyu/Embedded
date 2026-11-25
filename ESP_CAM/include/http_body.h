#pragma once

#include "cJSON.h"
#include "esp_err.h"
#include "esp_http_server.h"

/** Самое большое тело нового API. Chunked и прочий Transfer-Encoding не принимаются. */
constexpr size_t kHttpBodyMax = 1024;

struct HttpBody {
    cJSON *root = nullptr;
};

/**
 * Кадр можно оставить на keep-alive.
 * ESP_OK — нет чужого Transfer-Encoding и Content-Length не больше 1024.
 * ESP_ERR_INVALID_SIZE — тело длиннее лимита, байты не читались.
 * ESP_ERR_NOT_SUPPORTED — Transfer-Encoding не identity. Байты не читались.
 * В этих двух случаях вызывающий отправляет один ответ и закрывает сокет.
 */
esp_err_t http_frame_status(httpd_req_t *req);

/**
 * Корень должен быть JSON-объектом. Повторный вызов сначала освобождает прежний root.
 * null, массив, хвост после значения, повтор ключа, сырой NUL и \\u0000 — ошибка.
 * ESP_OK — объект в out, его снимает http_body_free.
 * ESP_ERR_INVALID_SIZE / ESP_ERR_NOT_SUPPORTED — см. http_frame_status, тело не читалось.
 * ESP_ERR_NO_MEM — буфер не выделен, тело не длиннее 1024 и всё ещё в сокете.
 * ESP_ERR_INVALID_STATE — нет тела, битый JSON, не объект или повтор ключа; прочитанное снято.
 * ESP_FAIL — обрыв или бюджет времени чтения: сокет закрывать, второй ответ не слать,
 * если первый уже ушёл. Одно чтение httpd_req_recv может занять recv timeout сервера,
 * прежде чем бюджет будет замечен.
 * err, если не null, получает короткую причину.
 */
esp_err_t http_read_object(httpd_req_t *req, HttpBody *out, char *err, size_t err_cap);
void http_body_free(HttpBody *body);

/** true, если есть поле вне списка или одно имя встретилось дважды. keys кончается nullptr. */
bool http_reject_unknown(const cJSON *obj, const char *const *keys);

/**
 * Поле нужного типа. JSON null не подходит ни под какой тип.
 * cJSON_True и cJSON_False оба принимаются как логическое значение.
 */
cJSON *http_require(const cJSON *obj, const char *key, int cjson_type);
