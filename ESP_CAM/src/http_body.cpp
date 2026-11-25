#include "http_body.h"

#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "esp_timer.h"

namespace {

constexpr int64_t kHttpBodyReadUs = 5000000;

void set_err(char *err, size_t cap, const char *text)
{
    if (!err || cap == 0) {
        return;
    }
    snprintf(err, cap, "%s", text ? text : "");
}

void clear_body(HttpBody *out)
{
    if (!out) {
        return;
    }
    cJSON_Delete(out->root);
    out->root = nullptr;
}

bool tail_only_space(const char *p, const char *end)
{
    for (; p < end; ++p) {
        if (!std::isspace(static_cast<unsigned char>(*p))) {
            return false;
        }
    }
    return true;
}

bool is_hex(unsigned char c)
{
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

/** Сырой NUL или экранированный U+0000. \\u0000 внутри JSON-строки тоже отвергается. */
bool has_embedded_nul(const char *buf, size_t n)
{
    for (size_t i = 0; i < n; ++i) {
        const unsigned char c = static_cast<unsigned char>(buf[i]);
        if (c == '\0') {
            return true;
        }
        if (c != '\\' || i + 1 >= n) {
            continue;
        }
        const unsigned char e = static_cast<unsigned char>(buf[i + 1]);
        if (e == '"' || e == '\\' || e == '/' || e == 'b' || e == 'f' || e == 'n' || e == 'r' || e == 't') {
            ++i;
            continue;
        }
        if (e != 'u' || i + 5 >= n) {
            continue;
        }
        bool hex = true;
        bool zeros = true;
        for (size_t k = 0; k < 4; ++k) {
            const unsigned char h = static_cast<unsigned char>(buf[i + 2 + k]);
            if (!is_hex(h)) {
                hex = false;
                break;
            }
            if (h != '0') {
                zeros = false;
            }
        }
        if (hex && zeros) {
            return true;
        }
        if (hex) {
            i += 5;
        }
    }
    return false;
}

bool duplicate_keys(const cJSON *node)
{
    if (!node) {
        return false;
    }
    if (cJSON_IsArray(node)) {
        for (const cJSON *child = node->child; child; child = child->next) {
            if (duplicate_keys(child)) {
                return true;
            }
        }
        return false;
    }
    if (!cJSON_IsObject(node)) {
        return false;
    }
    for (const cJSON *item = node->child; item; item = item->next) {
        if (!item->string) {
            return true;
        }
        for (const cJSON *prev = node->child; prev != item; prev = prev->next) {
            if (prev->string && strcmp(prev->string, item->string) == 0) {
                return true;
            }
        }
        if (duplicate_keys(item)) {
            return true;
        }
    }
    return false;
}

bool eq_identity(const char *s)
{
    const char id[] = "identity";
    for (size_t i = 0; id[i] != '\0'; ++i) {
        unsigned char c = static_cast<unsigned char>(s[i]);
        if (c >= 'A' && c <= 'Z') {
            c = static_cast<unsigned char>(c - 'A' + 'a');
        }
        if (c != static_cast<unsigned char>(id[i])) {
            return false;
        }
    }
    return s[sizeof(id) - 1] == '\0';
}

} // namespace

esp_err_t http_frame_status(httpd_req_t *req)
{
    if (!req) {
        return ESP_ERR_INVALID_ARG;
    }
    char te[48] = {};
    const esp_err_t hdr = httpd_req_get_hdr_value_str(req, "Transfer-Encoding", te, sizeof(te));
    if (hdr == ESP_ERR_HTTPD_RESULT_TRUNC || (hdr == ESP_OK && !eq_identity(te))) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    if (req->content_len > kHttpBodyMax) {
        return ESP_ERR_INVALID_SIZE;
    }
    return ESP_OK;
}

esp_err_t http_read_object(httpd_req_t *req, HttpBody *out, char *err, size_t err_cap)
{
    if (!req || !out) {
        set_err(err, err_cap, "invalid parameter");
        return ESP_ERR_INVALID_ARG;
    }
    clear_body(out);

    const esp_err_t frame = http_frame_status(req);
    if (frame == ESP_ERR_INVALID_SIZE) {
        set_err(err, err_cap, "body too large");
        return frame;
    }
    if (frame == ESP_ERR_NOT_SUPPORTED) {
        set_err(err, err_cap, "unsupported transfer encoding");
        return frame;
    }
    if (frame != ESP_OK) {
        set_err(err, err_cap, "invalid parameter");
        return frame;
    }
    if (req->content_len == 0) {
        set_err(err, err_cap, "body required");
        return ESP_ERR_INVALID_STATE;
    }

    char *buf = static_cast<char *>(malloc(req->content_len + 1));
    if (!buf) {
        set_err(err, err_cap, "out of memory");
        return ESP_ERR_NO_MEM;
    }

    size_t got = 0;
    const int64_t deadline = esp_timer_get_time() + kHttpBodyReadUs;
    while (got < req->content_len) {
        if (esp_timer_get_time() >= deadline) {
            free(buf);
            set_err(err, err_cap, "body timeout");
            return ESP_FAIL;
        }
        const int n = httpd_req_recv(req, buf + got, req->content_len - got);
        if (n == HTTPD_SOCK_ERR_TIMEOUT) {
            continue;
        }
        if (n <= 0) {
            free(buf);
            set_err(err, err_cap, "body truncated");
            return ESP_FAIL;
        }
        got += static_cast<size_t>(n);
    }
    buf[got] = '\0';

    if (has_embedded_nul(buf, got)) {
        free(buf);
        set_err(err, err_cap, "invalid json");
        return ESP_ERR_INVALID_STATE;
    }

    const char *end = nullptr;
    cJSON *root = cJSON_ParseWithLengthOpts(buf, got, &end, 0);
    const bool junk = !root || !end || end < buf || end > buf + got || !tail_only_space(end, buf + got);
    free(buf);
    if (junk) {
        cJSON_Delete(root);
        set_err(err, err_cap, "invalid json");
        return ESP_ERR_INVALID_STATE;
    }
    if (!cJSON_IsObject(root)) {
        cJSON_Delete(root);
        set_err(err, err_cap, "json object required");
        return ESP_ERR_INVALID_STATE;
    }
    if (duplicate_keys(root)) {
        cJSON_Delete(root);
        set_err(err, err_cap, "duplicate key");
        return ESP_ERR_INVALID_STATE;
    }
    out->root = root;
    set_err(err, err_cap, "");
    return ESP_OK;
}

void http_body_free(HttpBody *body)
{
    clear_body(body);
}

bool http_reject_unknown(const cJSON *obj, const char *const *keys)
{
    if (!obj || !cJSON_IsObject(obj)) {
        return true;
    }
    for (const cJSON *item = obj->child; item; item = item->next) {
        if (!item->string) {
            return true;
        }
        int seen = 0;
        for (const cJSON *prev = obj->child; prev && prev != item; prev = prev->next) {
            if (prev->string && strcmp(prev->string, item->string) == 0) {
                ++seen;
            }
        }
        if (seen > 0) {
            return true;
        }
        bool known = false;
        if (keys) {
            for (const char *const *k = keys; *k; ++k) {
                if (strcmp(*k, item->string) == 0) {
                    known = true;
                    break;
                }
            }
        }
        if (!known) {
            return true;
        }
    }
    return false;
}

cJSON *http_require(const cJSON *obj, const char *key, int cjson_type)
{
    if (!obj || !key || !cJSON_IsObject(obj)) {
        return nullptr;
    }
    cJSON *item = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (!item || cJSON_IsNull(item)) {
        return nullptr;
    }
    const int kind = item->type & 0xFF;
    if (cjson_type == cJSON_True || cjson_type == cJSON_False) {
        if (kind == cJSON_True || kind == cJSON_False) {
            return item;
        }
        return nullptr;
    }
    if (kind != cjson_type) {
        return nullptr;
    }
    return item;
}
