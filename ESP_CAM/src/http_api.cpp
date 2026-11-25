#include "http_api.h"

#include <cstring>

#include "cJSON.h"
#include "esp_log.h"
#include "http_auth.h"
#include "http_gate.h"
#include "http_observe.h"
#include "http_reply.h"
#include "http_session.h"
#include "http_status.h"

static const char *TAG = "http-api";

namespace {

enum class Access { PublicUi, Probe, Operator, Either };

struct Step {
    const char *tail;
    httpd_method_t method;
    Access access;
    bool raw;
    esp_err_t (*handle)(httpd_req_t *) = nullptr;
};

enum class Hit { Missing, Method, Ok };

const Step *find_step(const Step *steps, size_t n, const char *tail, int method, Hit *hit)
{
    const Step *other = nullptr;
    for (size_t i = 0; i < n; ++i) {
        if (strcmp(steps[i].tail, tail) != 0) {
            continue;
        }
        if (steps[i].method == method) {
            *hit = Hit::Ok;
            return &steps[i];
        }
        other = &steps[i];
    }
    *hit = other ? Hit::Method : Hit::Missing;
    return nullptr;
}

esp_err_t not_ready(httpd_req_t *req)
{
    return http_send_error(req, HttpError::Internal, "not implemented");
}

esp_err_t allow(httpd_req_t *req, Access access, bool raw)
{
    switch (access) {
    case Access::PublicUi:
        if (!http_ui_client(req)) {
            return http_send_error(req, HttpError::Unauthorized, "unauthorized");
        }
        return not_ready(req);
    case Access::Operator:
        if (http_operator(req)) {
            return not_ready(req);
        }
        if (req->user_ctx && static_cast<HttpStatus *>(req->user_ctx)->auth().bearerMatches(req)) {
            return http_send_error(req, HttpError::Forbidden, "forbidden");
        }
        return http_send_error(req, HttpError::Unauthorized, "unauthorized");
    case Access::Either: {
        const Caller caller = http_caller(req);
        if (caller == Caller::None) {
            return http_send_error(req, HttpError::Unauthorized, "unauthorized");
        }
        if (raw && !http_raw_allowed(caller)) {
            return http_send_error(req, HttpError::ProtocolDisabled, "protocol disabled");
        }
        return not_ready(req);
    }
    default:
        break;
    }
    return http_send_error(req, HttpError::Internal, "internal");
}

esp_err_t send_probe(httpd_req_t *req)
{
    HttpStatus *self = req && req->user_ctx ? static_cast<HttpStatus *>(req->user_ctx) : nullptr;
    if (!self) {
        return http_send_error(req, HttpError::Internal, "internal");
    }
    const Caller who = http_presented(req);
    cJSON *data = cJSON_CreateObject();
    if (!data) {
        return http_send_error(req, HttpError::Buffer, "response buffer");
    }
    const char *method = "none";
    if (who == Caller::Operator) {
        method = "operator";
    } else if (who == Caller::Service) {
        method = "service";
    }
    bool ok = cJSON_AddBoolToObject(data, "authenticated", who != Caller::None) != nullptr;
    ok = ok && cJSON_AddStringToObject(data, "method", method) != nullptr;
    if (who != Caller::None) {
        const char *user = self->auth().user();
        ok = ok && cJSON_AddStringToObject(data, "user", user ? user : "") != nullptr;
        ok = ok && cJSON_AddNumberToObject(data, "ttl_seconds", self->auth().sessionTtlSec()) != nullptr;
        if (who == Caller::Operator) {
            ok = ok && cJSON_AddNumberToObject(data, "seconds_left", self->auth().sessionRemainSec()) != nullptr;
        } else {
            ok = ok && cJSON_AddNullToObject(data, "seconds_left") != nullptr;
        }
        ok = ok && cJSON_AddNumberToObject(data, "ttl_min_seconds", HttpAuth::kSessTtlMinSec) != nullptr;
        ok = ok && cJSON_AddNumberToObject(data, "ttl_max_seconds", HttpAuth::kSessTtlMaxSec) != nullptr;
    }
    if (!ok) {
        cJSON_Delete(data);
        return http_send_error(req, HttpError::Buffer, "response buffer");
    }
    char *text = cJSON_PrintUnformatted(data);
    cJSON_Delete(data);
    if (!text) {
        return http_send_error(req, HttpError::Buffer, "response buffer");
    }
    const esp_err_t rc = http_send_ok(req, text);
    cJSON_free(text);
    return rc;
}

esp_err_t finish(httpd_req_t *req, Access access, bool raw)
{
    if (access == Access::Probe) {
        return send_probe(req);
    }
    return allow(req, access, raw);
}

/** Хвост пути без query. req->uri не меняется: query остаётся обработчикам. */
bool path_tail(const char *uri, const char *prefix, char *out, size_t cap)
{
    if (!uri || !prefix || !out || cap == 0) {
        return false;
    }
    const char *query = strchr(uri, '?');
    const size_t uri_n = query ? static_cast<size_t>(query - uri) : strlen(uri);
    const size_t pre = strlen(prefix);
    if (uri_n < pre || memcmp(uri, prefix, pre) != 0) {
        return false;
    }
    const size_t tail_n = uri_n - pre;
    if (tail_n + 1 > cap) {
        return false;
    }
    memcpy(out, uri + pre, tail_n);
    out[tail_n] = '\0';
    return true;
}

esp_err_t enter(httpd_req_t *req)
{
    if (http_close_if_bad_frame(req) != ESP_OK) {
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t dispatch_table(httpd_req_t *req, const char *prefix, const Step *steps, size_t n)
{
    char tail[HTTPD_MAX_URI_LEN + 1];
    if (!path_tail(req ? req->uri : nullptr, prefix, tail, sizeof(tail)) || tail[0] == '\0') {
        return http_send_error(req, HttpError::NotFound, "not found");
    }
    Hit hit = Hit::Missing;
    const Step *step = find_step(steps, n, tail, req->method, &hit);
    if (hit == Hit::Method) {
        return http_send_method_not_allowed(req);
    }
    if (hit != Hit::Ok || !step) {
        return http_send_error(req, HttpError::NotFound, "not found");
    }
    if (step->handle) {
        return step->handle(req);
    }
    return finish(req, step->access, step->raw);
}

bool preset_number(const char *s, const char **rest)
{
    if (!s || s[0] < '1' || s[0] > '9') {
        return false;
    }
    int value = 0;
    const char *p = s;
    while (*p >= '0' && *p <= '9') {
        value = value * 10 + (*p - '0');
        if (value > 99) {
            return false;
        }
        ++p;
    }
    if (value < 1) {
        return false;
    }
    *rest = p;
    return true;
}

bool osd_name(const char *s)
{
    if (!s || s[0] < 'a' || s[0] > 'z') {
        return false;
    }
    size_t n = 1;
    for (; s[n] != '\0'; ++n) {
        const char c = s[n];
        const bool ok = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_';
        if (!ok || n > 31) {
            return false;
        }
    }
    return n >= 1 && n <= 32;
}

const Step kAuth[] = {
    {"challenge", HTTP_POST, Access::PublicUi, false, http_auth_challenge},
    {"login", HTTP_POST, Access::PublicUi, false, http_auth_login},
    {"logout", HTTP_POST, Access::Operator, false, http_auth_logout},
    {"session", HTTP_GET, Access::Probe, false},
    {"password", HTTP_PUT, Access::Operator, false, http_auth_password},
    {"session/ttl", HTTP_PUT, Access::Operator, false, http_auth_session_ttl},
    {"token", HTTP_GET, Access::Operator, false, http_auth_token},
    {"token/reissue", HTTP_POST, Access::Operator, false, http_auth_reissue},
};

const Step kMotion[] = {
    {"stop", HTTP_POST, Access::Either, false},
    {"home", HTTP_POST, Access::Either, false},
    {"rate", HTTP_PUT, Access::Either, false},
};

const Step kOptics[] = {
    {"position-control", HTTP_PUT, Access::Either, false},
};

const Step kPresets[] = {
    {"latest", HTTP_GET, Access::Either, false},
    {"travel/path", HTTP_PUT, Access::Either, false},
    {"travel/speed", HTTP_PUT, Access::Either, false},
    {"travel/table", HTTP_PUT, Access::Either, false},
};

const Step kInstall[] = {
    {"axes", HTTP_PATCH, Access::Either, false},
    {"tilt-span", HTTP_PUT, Access::Either, false},
};

const char *kLimitAxes[] = {"tilt_up", "tilt_down", "pan_left", "pan_right", nullptr};

const Step kCamera[] = {
    {"catalog", HTTP_GET, Access::Either, false},
    {"values", HTTP_GET, Access::Either, false},
    {"identity/model", HTTP_GET, Access::Either, false},
    {"identity/version", HTTP_GET, Access::Either, false},
    {"osd/catalog", HTTP_GET, Access::Either, false},
    {"menu", HTTP_PUT, Access::Either, false},
    {"menu/keys", HTTP_POST, Access::Either, false},
    {"shutter", HTTP_PUT, Access::Either, false},
    {"synchro-scan", HTTP_PUT, Access::Either, false},
    {"gain", HTTP_PUT, Access::Either, false},
    {"iris", HTTP_PUT, Access::Either, false},
    {"nd-filter", HTTP_PUT, Access::Either, false},
    {"white-balance", HTTP_PUT, Access::Either, false},
    {"white-balance/sample", HTTP_POST, Access::Either, false},
    {"black-balance/sample", HTTP_POST, Access::Either, false},
    {"chroma", HTTP_PUT, Access::Either, false},
    {"paint/gain", HTTP_PUT, Access::Either, false},
    {"paint/pedestal", HTTP_PUT, Access::Either, false},
    {"detail", HTTP_PUT, Access::Either, false},
    {"detail/hd", HTTP_PUT, Access::Either, false},
    {"detail/master", HTTP_PUT, Access::Either, false},
    {"gamma", HTTP_PUT, Access::Either, false},
    {"noise-reduction", HTTP_PUT, Access::Either, false},
    {"shading", HTTP_PUT, Access::Either, false},
    {"bars", HTTP_PUT, Access::Either, false},
    {"polarity", HTTP_PUT, Access::Either, false},
    {"scan", HTTP_PUT, Access::Either, false},
    {"digital-gain", HTTP_PUT, Access::Either, false},
    {"metering/area", HTTP_PUT, Access::Either, false},
    {"metering/peak", HTTP_PUT, Access::Either, false},
    {"scene", HTTP_PUT, Access::Either, false},
    {"scene/memory", HTTP_PUT, Access::Either, false},
    {"genlock", HTTP_PUT, Access::Either, false},
    {"agc-ceiling", HTTP_PUT, Access::Either, false},
    {"monitor", HTTP_PUT, Access::Either, false},
    {"lens-interface", HTTP_PUT, Access::Either, false},
    {"focus", HTTP_PUT, Access::Either, false},
};

const Step kDiag[] = {
    {"exposure", HTTP_GET, Access::Either, false},
    {"fault", HTTP_GET, Access::Either, false},
    {"landing", HTTP_GET, Access::Either, false},
    {"head/version", HTTP_GET, Access::Either, false},
    {"heater", HTTP_GET, Access::Either, false},
    {"defroster", HTTP_GET, Access::Either, false},
    {"protocol", HTTP_GET, Access::Either, false},
    {"protocol", HTTP_PUT, Access::Operator, false},
    {"protocol/head", HTTP_POST, Access::Either, true},
    {"protocol/camera", HTTP_POST, Access::Either, true},
};

esp_err_t on_auth(httpd_req_t *req)
{
    if (enter(req) != ESP_OK) {
        return ESP_FAIL;
    }
    return dispatch_table(req, "/api/auth/", kAuth, sizeof(kAuth) / sizeof(kAuth[0]));
}

esp_err_t on_motion(httpd_req_t *req)
{
    if (enter(req) != ESP_OK) {
        return ESP_FAIL;
    }
    return finish(req, Access::Either, false);
}

esp_err_t on_motion_tail(httpd_req_t *req)
{
    if (enter(req) != ESP_OK) {
        return ESP_FAIL;
    }
    return dispatch_table(req, "/api/motion/", kMotion, sizeof(kMotion) / sizeof(kMotion[0]));
}

esp_err_t on_power(httpd_req_t *req)
{
    if (enter(req) != ESP_OK) {
        return ESP_FAIL;
    }
    return finish(req, Access::Either, false);
}

esp_err_t on_aim(httpd_req_t *req)
{
    if (enter(req) != ESP_OK) {
        return ESP_FAIL;
    }
    return finish(req, Access::Either, false);
}

esp_err_t on_optics(httpd_req_t *req)
{
    if (enter(req) != ESP_OK) {
        return ESP_FAIL;
    }
    return finish(req, Access::Either, false);
}

esp_err_t on_optics_tail(httpd_req_t *req)
{
    if (enter(req) != ESP_OK) {
        return ESP_FAIL;
    }
    return dispatch_table(req, "/api/optics/", kOptics, sizeof(kOptics) / sizeof(kOptics[0]));
}

esp_err_t on_presets(httpd_req_t *req)
{
    if (enter(req) != ESP_OK) {
        return ESP_FAIL;
    }
    return finish(req, Access::Either, false);
}

esp_err_t on_presets_tail(httpd_req_t *req)
{
    if (enter(req) != ESP_OK) {
        return ESP_FAIL;
    }
    char tail[HTTPD_MAX_URI_LEN + 1];
    if (!path_tail(req ? req->uri : nullptr, "/api/presets/", tail, sizeof(tail))) {
        return http_send_error(req, HttpError::NotFound, "not found");
    }
    Hit hit = Hit::Missing;
    if (find_step(kPresets, sizeof(kPresets) / sizeof(kPresets[0]), tail, req->method, &hit)) {
        return finish(req, Access::Either, false);
    }
    if (hit == Hit::Method) {
        return http_send_method_not_allowed(req);
    }
    const char *rest = nullptr;
    if (!preset_number(tail, &rest)) {
        return http_send_error(req, HttpError::NotFound, "not found");
    }
    if (rest[0] == '\0') {
        if (req->method == HTTP_PUT || req->method == HTTP_DELETE) {
            return finish(req, Access::Either, false);
        }
        return http_send_method_not_allowed(req);
    }
    if (strcmp(rest, "/recall") == 0) {
        if (req->method == HTTP_POST) {
            return finish(req, Access::Either, false);
        }
        return http_send_method_not_allowed(req);
    }
    return http_send_error(req, HttpError::NotFound, "not found");
}

bool known_token(const char *tail, const char *const *words, const char **rest)
{
    if (!tail || !words) {
        return false;
    }
    for (const char *const *w = words; *w; ++w) {
        const size_t n = strlen(*w);
        if (strncmp(tail, *w, n) != 0) {
            continue;
        }
        if (tail[n] == '\0' || tail[n] == '/') {
            *rest = tail + n;
            return true;
        }
    }
    return false;
}

esp_err_t on_limits(httpd_req_t *req)
{
    if (enter(req) != ESP_OK) {
        return ESP_FAIL;
    }
    char tail[HTTPD_MAX_URI_LEN + 1];
    const char *rest = nullptr;
    if (!path_tail(req ? req->uri : nullptr, "/api/limits/", tail, sizeof(tail))) {
        return http_send_error(req, HttpError::NotFound, "not found");
    }
    if (!known_token(tail, kLimitAxes, &rest)) {
        return http_send_error(req, HttpError::NotFound, "not found");
    }
    if (rest[0] == '\0') {
        if (req->method == HTTP_PUT) {
            return finish(req, Access::Either, false);
        }
        return http_send_method_not_allowed(req);
    }
    if (strcmp(rest, "/toggle") == 0) {
        if (req->method == HTTP_POST) {
            return finish(req, Access::Either, false);
        }
        return http_send_method_not_allowed(req);
    }
    return http_send_error(req, HttpError::NotFound, "not found");
}

esp_err_t on_install(httpd_req_t *req)
{
    if (enter(req) != ESP_OK) {
        return ESP_FAIL;
    }
    return dispatch_table(req, "/api/installation/", kInstall, sizeof(kInstall) / sizeof(kInstall[0]));
}

esp_err_t on_fan(httpd_req_t *req)
{
    if (enter(req) != ESP_OK) {
        return ESP_FAIL;
    }
    char tail[HTTPD_MAX_URI_LEN + 1];
    if (!path_tail(req ? req->uri : nullptr, "/api/fans/", tail, sizeof(tail)) ||
        (strcmp(tail, "primary") != 0 && strcmp(tail, "secondary") != 0)) {
        return http_send_error(req, HttpError::NotFound, "not found");
    }
    if (req->method != HTTP_PUT) {
        return http_send_method_not_allowed(req);
    }
    return finish(req, Access::Either, false);
}

esp_err_t on_exact_either(httpd_req_t *req)
{
    if (enter(req) != ESP_OK) {
        return ESP_FAIL;
    }
    return finish(req, Access::Either, false);
}

esp_err_t on_camera(httpd_req_t *req)
{
    if (enter(req) != ESP_OK) {
        return ESP_FAIL;
    }
    char tail[HTTPD_MAX_URI_LEN + 1];
    if (!path_tail(req ? req->uri : nullptr, "/api/camera/", tail, sizeof(tail))) {
        return http_send_error(req, HttpError::NotFound, "not found");
    }
    Hit hit = Hit::Missing;
    const Step *step = find_step(kCamera, sizeof(kCamera) / sizeof(kCamera[0]), tail, req->method, &hit);
    if (hit == Hit::Ok && step) {
        return finish(req, step->access, step->raw);
    }
    if (hit == Hit::Method) {
        return http_send_method_not_allowed(req);
    }
    if (strncmp(tail, "osd/", 4) == 0 && strcmp(tail, "osd/catalog") != 0 && osd_name(tail + 4)) {
        if (req->method == HTTP_GET || req->method == HTTP_PUT) {
            return finish(req, Access::Either, false);
        }
        return http_send_method_not_allowed(req);
    }
    return http_send_error(req, HttpError::NotFound, "not found");
}

esp_err_t on_diag(httpd_req_t *req)
{
    if (enter(req) != ESP_OK) {
        return ESP_FAIL;
    }
    return dispatch_table(req, "/api/diagnostics/", kDiag, sizeof(kDiag) / sizeof(kDiag[0]));
}

bool add(httpd_handle_t server, const char *uri, httpd_method_t method, esp_err_t (*fn)(httpd_req_t *), void *ctx)
{
    httpd_uri_t h = {};
    h.uri = uri;
    h.method = method;
    h.handler = fn;
    h.user_ctx = ctx;
    const esp_err_t rc = httpd_register_uri_handler(server, &h);
    if (rc != ESP_OK) {
        ESP_LOGE(TAG, "register %s method %d failed: %s", uri, static_cast<int>(method), esp_err_to_name(rc));
        return false;
    }
    return true;
}

} // namespace

bool http_api_register(httpd_handle_t server, void *ctx)
{
    if (!server) {
        return false;
    }
    const httpd_method_t any = static_cast<httpd_method_t>(HTTP_ANY);
    /* status + 14 точных + 9 HTTP_ANY. Вместе с 5 старыми слотами это 29 из 48. */
    return add(server, "/api/status", HTTP_GET, http_get_status, ctx) &&
           add(server, "/api/auth/*", any, on_auth, ctx) &&
           add(server, "/api/motion", HTTP_POST, on_motion, ctx) &&
           add(server, "/api/motion/*", any, on_motion_tail, ctx) &&
           add(server, "/api/power", HTTP_POST, on_power, ctx) &&
           add(server, "/api/aim", HTTP_GET, on_aim, ctx) &&
           add(server, "/api/aim", HTTP_PUT, on_aim, ctx) &&
           add(server, "/api/optics", HTTP_GET, on_optics, ctx) &&
           add(server, "/api/optics", HTTP_PATCH, on_optics, ctx) &&
           add(server, "/api/optics/*", any, on_optics_tail, ctx) &&
           add(server, "/api/presets", HTTP_GET, on_presets, ctx) &&
           add(server, "/api/presets/*", any, on_presets_tail, ctx) &&
           add(server, "/api/limits/*", any, on_limits, ctx) &&
           add(server, "/api/installation/*", any, on_install, ctx) &&
           add(server, "/api/fans/*", any, on_fan, ctx) &&
           add(server, "/api/tally", HTTP_PUT, on_exact_either, ctx) &&
           add(server, "/api/wireless-control", HTTP_PUT, on_exact_either, ctx) &&
           add(server, "/api/washer", HTTP_PUT, on_exact_either, ctx) &&
           add(server, "/api/option-output", HTTP_PUT, on_exact_either, ctx) &&
           add(server, "/api/wiper", HTTP_PUT, on_exact_either, ctx) &&
           add(server, "/api/bridge/models", HTTP_PUT, on_exact_either, ctx) &&
           add(server, "/api/network", HTTP_PUT, on_exact_either, ctx) &&
           add(server, "/api/camera/*", any, on_camera, ctx) &&
           add(server, "/api/diagnostics/*", any, on_diag, ctx);
}
