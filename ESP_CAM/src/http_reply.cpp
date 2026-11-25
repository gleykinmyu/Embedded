#include "http_reply.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "esp_http_server.h"
#include "http_body.h"

namespace {

struct ErrMap {
    const char *status;
    const char *code;
};

ErrMap map_error(HttpError err)
{
    switch (err) {
    case HttpError::InvalidParameter:
        return {"400 Bad Request", "invalid_parameter"};
    case HttpError::Unauthorized:
        return {"401 Unauthorized", "unauthorized"};
    case HttpError::Forbidden:
        return {"403 Forbidden", "forbidden"};
    case HttpError::Unsupported:
        return {"403 Forbidden", "unsupported"};
    case HttpError::ProtocolDisabled:
        return {"403 Forbidden", "protocol_disabled"};
    case HttpError::NotFound:
        return {"404 Not Found", "not_found"};
    case HttpError::DeviceRejected:
        return {"409 Conflict", "device_rejected"};
    case HttpError::Buffer:
        return {"500 Internal Server Error", "buffer"};
    case HttpError::Internal:
        return {"500 Internal Server Error", "internal"};
    case HttpError::OtaBusy:
        return {"503 Service Unavailable", "ota_busy"};
    case HttpError::DeviceIo:
        return {"503 Service Unavailable", "device_io"};
    case HttpError::CameraUnavailable:
        return {"503 Service Unavailable", "camera_unavailable"};
    case HttpError::DeviceTimeout:
        return {"504 Gateway Timeout", "device_timeout"};
    }
    return {"500 Internal Server Error", "internal"};
}

void json_escape(const char *in, char *out, size_t cap)
{
    if (!out || cap == 0) {
        return;
    }
    size_t o = 0;
    if (!in) {
        out[0] = '\0';
        return;
    }
    for (size_t i = 0; in[i] != '\0' && o + 1 < cap; ++i) {
        const unsigned char c = static_cast<unsigned char>(in[i]);
        if (c == '"' || c == '\\') {
            if (o + 2 >= cap) {
                break;
            }
            out[o++] = '\\';
            out[o++] = static_cast<char>(c);
        } else if (c < 0x20) {
            if (o + 6 >= cap) {
                break;
            }
            const int n = snprintf(out + o, cap - o, "\\u%04X", c);
            if (n <= 0) {
                break;
            }
            o += static_cast<size_t>(n);
        } else {
            out[o++] = static_cast<char>(c);
        }
    }
    out[o] = '\0';
}

esp_err_t send_json(httpd_req_t *req, const char *status, const char *body)
{
    if (!req || !status || !body) {
        return ESP_FAIL;
    }
    if (httpd_resp_set_status(req, status) != ESP_OK) {
        return ESP_FAIL;
    }
    if (httpd_resp_set_type(req, "application/json") != ESP_OK) {
        return ESP_FAIL;
    }
    if (httpd_resp_set_hdr(req, "Cache-Control", "no-store") != ESP_OK) {
        return ESP_FAIL;
    }
    return httpd_resp_send(req, body, HTTPD_RESP_USE_STRLEN);
}

esp_err_t send_error_text(httpd_req_t *req, const char *status, const char *code, const char *message)
{
    char esc[160];
    json_escape(message ? message : "", esc, sizeof(esc));
    char buf[320];
    const int n = snprintf(buf, sizeof(buf),
                           "{\"ok\":false,\"error\":{\"code\":\"%s\",\"message\":\"%s\"}}",
                           code, esc);
    if (n <= 0 || static_cast<size_t>(n) >= sizeof(buf)) {
        return ESP_FAIL;
    }
    return send_json(req, status, buf);
}

} // namespace

esp_err_t http_send_ok(httpd_req_t *req, const char *data_json)
{
    if (!data_json) {
        data_json = "{}";
    }
    const size_t need = strlen(data_json) + 24;
    char *buf = static_cast<char *>(malloc(need));
    if (!buf) {
        return send_error_text(req, "500 Internal Server Error", "buffer", "response buffer");
    }
    const int n = snprintf(buf, need, "{\"ok\":true,\"data\":%s}", data_json);
    esp_err_t rc = ESP_FAIL;
    if (n > 0 && static_cast<size_t>(n) < need) {
        rc = send_json(req, "200 OK", buf);
    }
    free(buf);
    return rc;
}

esp_err_t http_send_error(httpd_req_t *req, HttpError err, const char *message)
{
    const ErrMap m = map_error(err);
    return send_error_text(req, m.status, m.code, message);
}

esp_err_t http_send_status(httpd_req_t *req, ccam::Status st)
{
    switch (st) {
    case ccam::Status::Ok:
        return http_send_ok(req, "{}");
    case ccam::Status::Param:
        return http_send_error(req, HttpError::InvalidParameter, "invalid parameter");
    case ccam::Status::Io:
        return http_send_error(req, HttpError::DeviceIo, "device io");
    case ccam::Status::Timeout:
        return http_send_error(req, HttpError::DeviceTimeout, "device timeout");
    case ccam::Status::Nack:
        return http_send_error(req, HttpError::DeviceRejected, "device rejected");
    case ccam::Status::Buffer:
        return http_send_error(req, HttpError::Buffer, "response buffer");
    }
    return http_send_error(req, HttpError::Internal, "internal");
}

esp_err_t http_send_method_not_allowed(httpd_req_t *req)
{
    return send_error_text(req, "405 Method Not Allowed", "invalid_parameter", "method not allowed");
}

esp_err_t http_close_if_bad_frame(httpd_req_t *req)
{
    const esp_err_t st = http_frame_status(req);
    if (st == ESP_OK) {
        return ESP_OK;
    }
    HttpError err = HttpError::InvalidParameter;
    const char *msg = "unsupported transfer encoding";
    if (st == ESP_ERR_INVALID_SIZE) {
        msg = "body too large";
    } else if (st != ESP_ERR_NOT_SUPPORTED) {
        err = HttpError::Internal;
        msg = "internal";
    }
    (void)http_send_error(req, err, msg);
    return ESP_FAIL;
}

esp_err_t http_on_404(httpd_req_t *req, httpd_err_code_t error)
{
    (void)error;
    if (http_close_if_bad_frame(req) != ESP_OK) {
        return ESP_FAIL;
    }
    const esp_err_t rc = http_send_error(req, HttpError::NotFound, "not found");
    return rc == ESP_OK ? ESP_OK : ESP_FAIL;
}

esp_err_t http_on_405(httpd_req_t *req, httpd_err_code_t error)
{
    (void)error;
    if (http_close_if_bad_frame(req) != ESP_OK) {
        return ESP_FAIL;
    }
    const esp_err_t rc = http_send_method_not_allowed(req);
    return rc == ESP_OK ? ESP_OK : ESP_FAIL;
}
