#include "http_status.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "eth_lan8720.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "catalog/osd_menu.hpp"
#include "catalog/video_menu.hpp"
#include "devices/cam_menu_deps.hpp"
#include "devices/cam_param_ranges.hpp"
#include "devices/osd_params.hpp"
#include "devices/integrated_ptz.hpp"
#include "devices/models/he130.hpp"
#include "http_auth.h"
#include "lwip/inet.h"
#include "lwip/ip_addr.h"
#include "ota_busy.h"
#include "ph350_ctrl.h"
#include "protocol/pt_types.hpp"

static const char *TAG = "http";

namespace {

struct Q {
    char cmd[40]{};
    char rate[8]{};
    char preset[8]{};
    char v[16]{};
    char v2[16]{};
    char v3[16]{};
    char v4[16]{};
    char data[48]{};
    char data2[24]{};
    char only[120]{};  ///< params scope=deps: whitelist keys
    char gates[120]{}; ///< params scope=deps: whitelist gates (iris:manual,…)
    char scope[16]{};  ///< "" | "deps"
    char user[40]{};
    char pass[40]{};
    char proof[68]{}; ///< hex SHA-256, логин и смена пароля
    char hash[68]{};  ///< hex SHA-256 нового пароля
};

void url_decode_inplace(char *s)
{
    if (!s) {
        return;
    }
    char *w = s;
    for (char *r = s; *r; ++r) {
        if (*r == '+' ) {
            *w++ = ' ';
        } else if (*r == '%' && r[1] && r[2]) {
            auto hex = [](char c) -> int {
                if (c >= '0' && c <= '9') return c - '0';
                if (c >= 'a' && c <= 'f') return c - 'a' + 10;
                if (c >= 'A' && c <= 'F') return c - 'A' + 10;
                return -1;
            };
            const int hi = hex(r[1]);
            const int lo = hex(r[2]);
            if (hi >= 0 && lo >= 0) {
                *w++ = static_cast<char>((hi << 4) | lo);
                r += 2;
            } else {
                *w++ = *r;
            }
        } else {
            *w++ = *r;
        }
    }
    *w = '\0';
}

void load_q(httpd_req_t *req, Q &q) {
    char query[320] = {};
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) != ESP_OK) {
        return;
    }
    (void)httpd_query_key_value(query, "cmd", q.cmd, sizeof(q.cmd));
    (void)httpd_query_key_value(query, "rate", q.rate, sizeof(q.rate));
    (void)httpd_query_key_value(query, "preset", q.preset, sizeof(q.preset));
    (void)httpd_query_key_value(query, "v", q.v, sizeof(q.v));
    (void)httpd_query_key_value(query, "v2", q.v2, sizeof(q.v2));
    (void)httpd_query_key_value(query, "v3", q.v3, sizeof(q.v3));
    (void)httpd_query_key_value(query, "v4", q.v4, sizeof(q.v4));
    (void)httpd_query_key_value(query, "data", q.data, sizeof(q.data));
    (void)httpd_query_key_value(query, "data2", q.data2, sizeof(q.data2));
    (void)httpd_query_key_value(query, "only", q.only, sizeof(q.only));
    (void)httpd_query_key_value(query, "gates", q.gates, sizeof(q.gates));
    (void)httpd_query_key_value(query, "scope", q.scope, sizeof(q.scope));
    (void)httpd_query_key_value(query, "user", q.user, sizeof(q.user));
    (void)httpd_query_key_value(query, "pass", q.pass, sizeof(q.pass));
    (void)httpd_query_key_value(query, "proof", q.proof, sizeof(q.proof));
    (void)httpd_query_key_value(query, "hash", q.hash, sizeof(q.hash));
    url_decode_inplace(q.user);
    url_decode_inplace(q.pass);
    url_decode_inplace(q.data);
    url_decode_inplace(q.data2);
    url_decode_inplace(q.only);
    url_decode_inplace(q.gates);
}

int as_int(const char *s, int def = 0) {
    return s && s[0] ? atoi(s) : def;
}

bool parse_ip4_str(const char *s, uint32_t *out) {
    if (!s || !s[0] || !out) {
        return false;
    }
    ip_addr_t a{};
    if (!ipaddr_aton(s, &a) || !IP_IS_V4(&a)) {
        return false;
    }
    *out = ip4_addr_get_u32(ip_2_ip4(&a));
    return true;
}

uint16_t as_u16(const char *s, uint16_t def = 0) {
    if (!s || !s[0]) {
        return def;
    }
    return static_cast<uint16_t>(strtoul(s, nullptr, 0));
}

uint8_t clamp_rate(int n, uint8_t fallback) {
    if (n >= 1 && n <= 100) {
        return static_cast<uint8_t>(n);
    }
    return fallback;
}

esp_err_t send_json(httpd_req_t *req, const char *buf) {
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_send(req, buf, HTTPD_RESP_USE_STRLEN);
}

void format_wire(const uint8_t *data, size_t len, char *out, size_t cap) {
    if (!out || cap == 0) {
        return;
    }
    size_t o = 0;
    for (size_t i = 0; i < len && o + 8 < cap; ++i) {
        const uint8_t b = data[i];
        int n = 0;
        if (b == ccam::kStx) {
            n = snprintf(out + o, cap - o, "%s", "<STX>");
        } else if (b == ccam::kEtx) {
            n = snprintf(out + o, cap - o, "%s", "<ETX>");
        } else if (b == ccam::kAck) {
            n = snprintf(out + o, cap - o, "%s", "<ACK>");
        } else if (b == ccam::kPtCr) {
            n = snprintf(out + o, cap - o, "%s", "<CR>");
        } else if (b >= 32 && b <= 126 && b != '"' && b != '\\') {
            n = snprintf(out + o, cap - o, "%c", static_cast<char>(b));
        } else {
            // JSON-safe: один '\' в строке ответа должен быть "\\xHH"
            n = snprintf(out + o, cap - o, "\\\\x%02X", b);
        }
        if (n <= 0) {
            break;
        }
        o += static_cast<size_t>(n);
    }
    out[o < cap ? o : cap - 1] = '\0';
}

void ascii_rx(const uint8_t *rx, size_t n, char *out, size_t cap) {
    if (!out || cap == 0) {
        return;
    }
    size_t o = 0;
    for (size_t i = 0; i < n && o + 1 < cap; ++i) {
        const uint8_t b = rx[i];
        if (b == '\r' || b == '\n') {
            continue;
        }
        out[o++] = (b >= 32 && b <= 126) ? static_cast<char>(b) : '.';
    }
    out[o] = '\0';
}

void json_escape(const char *in, char *out, size_t cap) {
    if (!out || cap == 0) {
        return;
    }
    size_t o = 0;
    if (!in) {
        out[0] = '\0';
        return;
    }
    for (size_t i = 0; in[i] != '\0'; ++i) {
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
            if (o + 1 >= cap) {
                break;
            }
            out[o++] = static_cast<char>(c);
        }
    }
    out[o] = '\0';
}

esp_err_t reply_st(httpd_req_t *req, ccam::Status st, const char *extra = nullptr) {
    char buf[512];
    if (extra && extra[0]) {
        char esc[256];
        json_escape(extra, esc, sizeof(esc));
        snprintf(buf, sizeof(buf), "{\"ok\":%s,\"status\":\"%s\",\"data\":\"%s\"}",
                 st == ccam::Status::Ok ? "true" : "false", Ph350Ctrl::statusText(st), esc);
    } else {
        snprintf(buf, sizeof(buf), "{\"ok\":%s,\"status\":\"%s\"}",
                 st == ccam::Status::Ok ? "true" : "false", Ph350Ctrl::statusText(st));
    }
    return send_json(req, buf);
}

esp_err_t reply_cam(httpd_req_t *req, Ph350Ctrl &pt, ccam::Status st, const char *extra = nullptr) {
    char tx[160];
    char rx[160];
    format_wire(pt.cam().lastTx(), pt.cam().lastTxLen(), tx, sizeof(tx));
    format_wire(pt.cam().lastRx(), pt.cam().lastRxLen(), rx, sizeof(rx));
    char buf[768];
    if (extra && extra[0]) {
        char esc[256];
        json_escape(extra, esc, sizeof(esc));
        snprintf(buf, sizeof(buf),
                 "{\"ok\":%s,\"status\":\"%s\",\"data\":\"%s\",\"tx\":\"%s\",\"rx\":\"%s\"}",
                 st == ccam::Status::Ok ? "true" : "false", Ph350Ctrl::statusText(st), esc, tx, rx);
    } else {
        snprintf(buf, sizeof(buf), "{\"ok\":%s,\"status\":\"%s\",\"tx\":\"%s\",\"rx\":\"%s\"}",
                 st == ccam::Status::Ok ? "true" : "false", Ph350Ctrl::statusText(st), tx, rx);
    }
    return send_json(req, buf);
}

} // namespace

// pio_idf_embed_txt(generated/status.html) — PIO-safe IDF embed
extern const char status_html_start[] asm("_binary_status_html_start");
extern const char status_html_end[] asm("_binary_status_html_end");

namespace {

struct HttpMu {
    SemaphoreHandle_t mu;
    explicit HttpMu(SemaphoreHandle_t m) : mu(m) {
        if (mu) {
            xSemaphoreTake(mu, portMAX_DELAY);
        }
    }
    ~HttpMu() {
        if (mu) {
            xSemaphoreGive(mu);
        }
    }
    HttpMu(const HttpMu &) = delete;
    HttpMu &operator=(const HttpMu &) = delete;
};

} // namespace

HttpStatus::HttpStatus(EthLan8720 &eth, Ph350Ctrl &pt) : eth_(eth), pt_(pt) {
    mu_ = xSemaphoreCreateMutex();
    auth_.begin();
}

HttpStatus &HttpStatus::self(httpd_req_t *req) {
    return *static_cast<HttpStatus *>(req->user_ctx);
}

esp_err_t HttpStatus::require_auth(httpd_req_t *req, bool slide)
{
    /* Фоновый /api/status не продлевает idle. Любой другой запрос — сдвигает. */
    if (req != nullptr && req->uri != nullptr && strcmp(req->uri, "/api/status") == 0) {
        slide = false;
    } else {
        slide = true;
    }
    if (auth_.authorized(req, slide)) {
        return ESP_OK;
    }
    httpd_resp_set_status(req, "401 Unauthorized");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    /* left>0 — наша cookie ещё в сроке (гонка парсинга).
     * sess_live — на сервере сессия есть (часто чужой логин / нет cookie). */
    const bool live = auth_.sessionActive();
    const uint32_t left = auth_.cookieMatches(req) ? auth_.sessionRemainSec() : 0;
    char deny[128];
    snprintf(deny, sizeof(deny),
             "{\"ok\":false,\"status\":\"auth\",\"sess_left_s\":%u,\"sess_ttl_s\":%u,"
             "\"sess_live\":%s}",
             static_cast<unsigned>(left),
             static_cast<unsigned>(auth_.sessionTtlSec()),
             live ? "true" : "false");
    (void)httpd_resp_send(req, deny, HTTPD_RESP_USE_STRLEN);
    return ESP_FAIL; /* ответ уже отправлен — хендлер дальше не идёт */
}

esp_err_t HttpStatus::on_root(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    // TEXT embed: null-terminated; end includes trailing NUL.
    const size_t len = static_cast<size_t>(status_html_end - status_html_start);
    const size_t send_len = (len > 0 && status_html_start[len - 1] == '\0') ? (len - 1) : len;
    return httpd_resp_send(req, status_html_start, send_len);
}

esp_err_t HttpStatus::on_auth(httpd_req_t *req)
{
    Q q{};
    load_q(req, q);
    char buf[384] = {};
    char cookie[96] = {};

    if (strcmp(q.cmd, "me") == 0) {
        const bool ok = auth_.authorized(req, true);
        const bool sess = ok && auth_.sessionActive();
        snprintf(buf, sizeof(buf),
                 "{\"ok\":%s,\"status\":\"%s\",\"user\":\"%s\",\"session\":%s,"
                 "\"sess_ttl_s\":%u,\"sess_left_s\":%u,\"sess_ttl_max_s\":%u,\"sess_ttl_min_s\":%u}",
                 ok ? "true" : "false", ok ? "ok" : "auth",
                 ok ? auth_.user() : "", sess ? "true" : "false",
                 static_cast<unsigned>(auth_.sessionTtlSec()),
                 static_cast<unsigned>(sess ? auth_.sessionRemainSec() : 0),
                 static_cast<unsigned>(HttpAuth::kSessTtlMaxSec),
                 static_cast<unsigned>(HttpAuth::kSessTtlMinSec));
        return send_json(req, buf);
    }
    if (strcmp(q.cmd, "touch") == 0) {
        /* явное продление idle от UI (клик/клавиша) */
        if (!auth_.authorized(req, true) || !auth_.sessionActive()) {
            (void)require_auth(req, false);
            return ESP_OK;
        }
        snprintf(buf, sizeof(buf),
                 "{\"ok\":true,\"status\":\"ok\",\"sess_ttl_s\":%u,\"sess_left_s\":%u}",
                 static_cast<unsigned>(auth_.sessionTtlSec()),
                 static_cast<unsigned>(auth_.sessionRemainSec()));
        return send_json(req, buf);
    }
    if (strcmp(q.cmd, "challenge") == 0) {
        if (!auth_.fromPage(req)) {
            httpd_resp_set_status(req, "401 Unauthorized");
            return send_json(req, "{\"ok\":false,\"status\":\"auth\"}");
        }
        char salt[40] = {};
        char nonce[40] = {};
        char newsalt[40] = {};
        if (!auth_.challenge(salt, sizeof(salt), nonce, sizeof(nonce), newsalt, sizeof(newsalt))) {
            return send_json(req, "{\"ok\":false,\"status\":\"param\"}");
        }
        const int n = snprintf(buf, sizeof(buf),
                               "{\"ok\":true,\"status\":\"ok\",\"salt\":\"%s\","
                               "\"nonce\":\"%s\",\"newsalt\":\"%s\"}",
                               salt, nonce, newsalt);
        if (n < 0 || static_cast<size_t>(n) >= sizeof(buf)) {
            return send_json(req, "{\"ok\":false,\"status\":\"param\"}");
        }
        return send_json(req, buf);
    }
    if (strcmp(q.cmd, "login") == 0) {
        if (!auth_.fromPage(req)) {
            httpd_resp_set_status(req, "401 Unauthorized");
            return send_json(req, "{\"ok\":false,\"status\":\"auth\"}");
        }
        char sess[HttpAuth::kSessLen + 1] = {};
        if (!auth_.login(q.user, q.proof, sess, sizeof(sess))) {
            httpd_resp_set_status(req, "401 Unauthorized");
            return send_json(req, "{\"ok\":false,\"status\":\"auth\"}");
        }
        snprintf(cookie, sizeof(cookie),
                 "esp_sess=%s; Path=/; HttpOnly; SameSite=Lax", sess);
        httpd_resp_set_hdr(req, "Set-Cookie", cookie);
        snprintf(buf, sizeof(buf),
                 "{\"ok\":true,\"status\":\"ok\",\"user\":\"%s\",\"session\":true,\"sess_ttl_s\":%u}",
                 auth_.user(), static_cast<unsigned>(auth_.sessionTtlSec()));
        return send_json(req, buf);
    }
    if (strcmp(q.cmd, "logout") == 0) {
        /* Тот же браузер: cookie сессии и запрос со страницы, без пароля. */
        if (!auth_.fromPage(req) || !auth_.cookieMatches(req)) {
            httpd_resp_set_status(req, "401 Unauthorized");
            return send_json(req, "{\"ok\":false,\"status\":\"auth\"}");
        }
        auth_.logout();
        httpd_resp_set_hdr(req, "Set-Cookie", "esp_sess=; Path=/; Max-Age=0; HttpOnly");
        return send_json(req, "{\"ok\":true,\"status\":\"ok\"}");
    }
    /* ниже — только для сессии UI (Bearer не меняет пароль/токен/TTL) */
    if (!auth_.authorized(req) || !auth_.sessionActive()) {
        (void)require_auth(req);
        return ESP_OK;
    }
    if (strcmp(q.cmd, "set_pass") == 0) {
        /* proof=SHA-256(nonce||старый хеш), hash=SHA-256(newsalt||новый пароль) */
        if (!auth_.setPassword(q.proof, q.hash)) {
            return send_json(req, "{\"ok\":false,\"status\":\"param\"}");
        }
        return send_json(req, "{\"ok\":true,\"status\":\"ok\"}");
    }
    if (strcmp(q.cmd, "set_sess_ttl") == 0) {
        /* v = секунды, 30…300 */
        const int sec = as_int(q.v, -1);
        if (sec < 0 || !auth_.setSessionTtlSec(static_cast<uint32_t>(sec))) {
            return send_json(req, "{\"ok\":false,\"status\":\"param\"}");
        }
        snprintf(buf, sizeof(buf),
                 "{\"ok\":true,\"status\":\"ok\",\"sess_ttl_s\":%u}",
                 static_cast<unsigned>(auth_.sessionTtlSec()));
        return send_json(req, buf);
    }
    if (strcmp(q.cmd, "token") == 0) {
        snprintf(buf, sizeof(buf),
                 "{\"ok\":true,\"status\":\"ok\",\"token\":\"%s\",\"user\":\"%s\",\"sess_ttl_s\":%u}",
                 auth_.apiToken(), auth_.user(),
                 static_cast<unsigned>(auth_.sessionTtlSec()));
        return send_json(req, buf);
    }
    if (strcmp(q.cmd, "reissue_token") == 0) {
        auth_.reissueToken();
        snprintf(buf, sizeof(buf),
                 "{\"ok\":true,\"status\":\"ok\",\"token\":\"%s\"}", auth_.apiToken());
        return send_json(req, buf);
    }
    return send_json(req, "{\"ok\":false,\"status\":\"param\"}");
}

esp_err_t HttpStatus::on_status(httpd_req_t *req) {
    /* Фоновый tick не продлевает cookie-сессию — только проверка. */
    if (require_auth(req, false) != ESP_OK) {
        return ESP_OK;
    }
    Ph350Ctrl::DeviceLock dev(pt_);
    char ip[16] = {};
    char mask[16] = {};
    char gw[16] = {};
    char mac[18] = {};
    eth_.ip_text(ip, sizeof(ip));
    eth_.mask_text(mask, sizeof(mask));
    eth_.gw_text(gw, sizeof(gw));
    eth_.mac_text(mac, sizeof(mac));
    const uint32_t uptime_s = static_cast<uint32_t>(xTaskGetTickCount() / configTICK_RATE_HZ);

    const pt::AxisInvert &inv = pt_.axisInvert();
    const ccam::devices::PtCaps pc = pt_.ptCaps();
    const ccam::devices::CamCaps cc = pt_.camCaps();
    const auto &hl = pt_.linkHealth();
    const unsigned pwr = hl.powerCode() == 0xFF ? 255u : static_cast<unsigned>(hl.powerCode());
    const unsigned pt_err = hl.errorCode() == 0xFF ? 255u : static_cast<unsigned>(hl.errorCode());
    char cam_qid[24] = {};
    {
        const char *src = hl.camId();
        size_t j = 0;
        for (size_t i = 0; src[i] != '\0' && j + 1 < sizeof(cam_qid); ++i) {
            const char c = src[i];
            if (c == '"' || c == '\\' || static_cast<unsigned char>(c) < 0x20) {
                continue;
            }
            cam_qid[j++] = c;
        }
        cam_qid[j] = '\0';
    }
    char buf[1408];
    const int wrote = snprintf(
        buf, sizeof(buf),
        "{\"link\":%s,\"ip\":\"%s\",\"mask\":\"%s\",\"gw\":\"%s\",\"mac\":\"%s\","
        "\"uptime_s\":%lu,\"sess_ttl_s\":%u,\"sess_left_s\":%u,\"model\":\"%s\",\"cam\":\"%s\","
        "\"pt_id\":\"%s\",\"cam_id\":\"%s\",\"rate\":%u,"
        "\"pt_alive\":%s,\"pt_power\":%u,\"pt_power_s\":\"%s\",\"pt_powered\":%s,"
        "\"pt_optics\":%s,\"pt_optics_fail\":%u,"
        "\"pt_fail\":%u,\"pt_error\":%u,"
        "\"cam_tx\":%s,\"cam_alive\":%s,\"cam_qid\":\"%s\",\"cam_fail\":%u,"
        "\"inv_pan\":%s,\"inv_tilt\":%s,\"inv_zoom\":%s,\"inv_focus\":%s,"
        "\"latest_preset\":%u,\"at_latest\":%s,"
        "\"pt_caps\":{\"abs_pt\":%s,\"abs_pt_speed\":%s,\"preset_map\":%s,\"preset_opts\":%s,"
        "\"limit_ctrl\":%s,\"limit_toggle\":%s,\"tally\":%s,\"fan\":%s,\"wiper\":%s,"
        "\"washer\":%s,\"wireless\":%s,\"option\":%s,\"tilt_range\":%s,"
        "\"heater\":%s,\"defroster\":%s,\"pos_disp\":%s,\"pt_pos\":%s,\"lens_q\":%s,"
        "\"exposure_q\":%s,\"landing\":%s,\"software_ver\":%s,\"power_cam_tx\":%s},"
        "\"cam_caps\":{\"hd_detail\":%s,\"neg_pos\":%s,\"field_frame\":%s,"
        "\"chroma\":%s,\"black_shade\":%s,\"synchro\":%s,\"abc\":%s,\"paint\":%s,"
        "\"genlock\":%s,\"gamma\":%s,\"agc_max\":%s,\"total_dtl\":%s,"
        "\"monitor\":%s,\"lens_h\":%s,\"menu_lr\":%s}}",
        eth_.link_up() ? "true" : "false", ip, mask, gw, mac,
        static_cast<unsigned long>(uptime_s),
        static_cast<unsigned>(auth_.sessionTtlSec()),
        static_cast<unsigned>(auth_.sessionRemainSec()),
        pt_.pt().modelLabel(), pt_.cam().modelLabel(),
        ccam::devices::ptModelKey(pt_.ptModel()), ccam::devices::camModelKey(pt_.camModel()),
        static_cast<unsigned>(pt_.defaultRate()), hl.alive() ? "true" : "false", pwr,
        hl.powerLabel(), hl.powered() ? "true" : "false",
        hl.opticsAlive() ? "true" : "false", static_cast<unsigned>(hl.opticsFailStreak()),
        static_cast<unsigned>(hl.failStreak()), pt_err,
        hl.camTxLikely() ? "true" : "false", hl.camAlive() ? "true" : "false", cam_qid,
        static_cast<unsigned>(hl.camFailStreak()),
        inv.pan ? "true" : "false", inv.tilt ? "true" : "false", inv.zoom ? "true" : "false",
        inv.focus ? "true" : "false",
        static_cast<unsigned>(pt_.latestPreset()), pt_.atLatest() ? "true" : "false",
        pc.abs_pt ? "true" : "false",
        pc.abs_pt_speed ? "true" : "false", pc.preset_map ? "true" : "false",
        pc.preset_opts ? "true" : "false",
        pc.limit_ctrl ? "true" : "false", pc.limit_toggle ? "true" : "false",
        pc.tally ? "true" : "false", pc.fan ? "true" : "false", pc.wiper ? "true" : "false",
        pc.washer ? "true" : "false", pc.wireless ? "true" : "false",
        pc.option ? "true" : "false", pc.tilt_range ? "true" : "false",
        pc.heater ? "true" : "false", pc.defroster ? "true" : "false",
        pc.pos_disp ? "true" : "false", pc.pt_pos ? "true" : "false",
        pc.lens_q ? "true" : "false", pc.exposure_q ? "true" : "false",
        pc.landing ? "true" : "false", pc.software_ver ? "true" : "false",
        pc.power_cam_tx ? "true" : "false",
        cc.hd_detail ? "true" : "false", cc.neg_pos ? "true" : "false",
        cc.field_frame ? "true" : "false", cc.chroma ? "true" : "false",
        cc.black_shade ? "true" : "false", cc.synchro ? "true" : "false",
        cc.abc ? "true" : "false", cc.paint ? "true" : "false",
        cc.genlock ? "true" : "false", cc.gamma ? "true" : "false",
        cc.agc_max ? "true" : "false", cc.total_dtl ? "true" : "false",
        cc.monitor ? "true" : "false", cc.lens_h ? "true" : "false",
        cc.menu_lr ? "true" : "false");
    if (wrote < 0 || static_cast<size_t>(wrote) >= sizeof(buf)) {
        return reply_st(req, ccam::Status::Buffer);
    }
    return send_json(req, buf);
}

esp_err_t reply_ota_busy(httpd_req_t *req) {
    httpd_resp_set_status(req, "503 Service Unavailable");
    return send_json(req, "{\"ok\":false,\"status\":\"ota\"}");
}

esp_err_t HttpStatus::on_ptz(httpd_req_t *req) {
    if (require_auth(req) != ESP_OK) {
        return ESP_OK;
    }
    if (otaIsBusy()) {
        return reply_ota_busy(req);
    }
    Q q;
    load_q(req, q);
    if (!q.cmd[0]) {
        return reply_st(req, ccam::Status::Param);
    }

    const bool preset_cmd = strcmp(q.cmd, "preset_recall") == 0 || strcmp(q.cmd, "preset_save") == 0 ||
                            strcmp(q.cmd, "preset_delete") == 0;
    const int preset_n = q.preset[0] ? as_int(q.preset, -1) : -1;
    if (preset_cmd && (preset_n < 1 || preset_n > 99)) {
        return reply_st(req, ccam::Status::Param);
    }

    Ph350Ctrl::DeviceLock dev(pt_);
    const uint8_t rate = clamp_rate(as_int(q.rate, -1), pt_.defaultRate());
    const uint8_t preset = static_cast<uint8_t>(preset_n >= 1 && preset_n <= 99 ? preset_n : 1);
    ccam::Status st = ccam::Status::Param;
    char extra[96] = {};
    char buf[384];

    if (strcmp(q.cmd, "set_rate") == 0) {
        pt_.setDefaultRate(rate);
        return reply_st(req, ccam::Status::Ok);
    }
    if (strcmp(q.cmd, "set_models") == 0) {
        const char *pt_key = q.data[0] ? q.data : q.v;
        const char *cam_key = q.data2[0] ? q.data2 : q.v2;
        const auto pt_id = ccam::devices::ptModelFromKey(pt_key);
        const auto cam_id = ccam::devices::camModelFromKey(cam_key);
        if (strcmp(ccam::devices::ptModelKey(pt_id), pt_key) != 0 ||
            strcmp(ccam::devices::camModelKey(cam_id), cam_key) != 0 ||
            !pt_.setModels(pt_id, cam_id, true)) {
            return reply_st(req, ccam::Status::Param);
        }
        snprintf(buf, sizeof(buf),
                 "{\"ok\":true,\"status\":\"ok\",\"pt_id\":\"%s\",\"cam_id\":\"%s\","
                 "\"model\":\"%s\",\"cam\":\"%s\"}",
                 ccam::devices::ptModelKey(pt_.ptModel()),
                 ccam::devices::camModelKey(pt_.camModel()), pt_.pt().modelLabel(),
                 pt_.cam().modelLabel());
        return send_json(req, buf);
    }
    if (strcmp(q.cmd, "set_invert") == 0) {
        // v=pan v2=tilt v3=zoom v4=focus — 0/1
        pt::AxisInvert inv = pt_.axisInvert();
        if (q.v[0]) {
            inv.pan = as_int(q.v) != 0;
        }
        if (q.v2[0]) {
            inv.tilt = as_int(q.v2) != 0;
        }
        if (q.v3[0]) {
            inv.zoom = as_int(q.v3) != 0;
        }
        if (q.v4[0]) {
            inv.focus = as_int(q.v4) != 0;
        }
        pt_.setAxisInvert(inv, true);
        st = ccam::Status::Ok;
        snprintf(buf, sizeof(buf),
                 "{\"ok\":true,\"status\":\"ok\",\"inv_pan\":%s,\"inv_tilt\":%s,"
                 "\"inv_zoom\":%s,\"inv_focus\":%s}",
                 inv.pan ? "true" : "false", inv.tilt ? "true" : "false",
                 inv.zoom ? "true" : "false", inv.focus ? "true" : "false");
        return send_json(req, buf);
    }

    auto &head = pt_.pt();
    const int8_t spd = Ph350Ctrl::rateToSigned(rate, +1);
    const int8_t nspd = Ph350Ctrl::rateToSigned(rate, -1);

    if (strcmp(q.cmd, "pan_left") == 0) {
        pt_.clearAtLatest();
        st = head.move(pt::Axis::Pan, nspd);
    } else if (strcmp(q.cmd, "pan_right") == 0) {
        pt_.clearAtLatest();
        st = head.move(pt::Axis::Pan, spd);
    } else if (strcmp(q.cmd, "tilt_up") == 0) {
        pt_.clearAtLatest();
        st = head.move(pt::Axis::Tilt, spd);
    } else if (strcmp(q.cmd, "tilt_down") == 0) {
        pt_.clearAtLatest();
        st = head.move(pt::Axis::Tilt, nspd);
    } else if (strcmp(q.cmd, "zoom_tele") == 0) {
        pt_.clearAtLatest();
        st = head.move(pt::Axis::Zoom, spd);
    } else if (strcmp(q.cmd, "zoom_wide") == 0) {
        pt_.clearAtLatest();
        st = head.move(pt::Axis::Zoom, nspd);
    } else if (strcmp(q.cmd, "focus_far") == 0) {
        st = head.move(pt::Axis::Focus, spd);
    } else if (strcmp(q.cmd, "focus_near") == 0) {
        st = head.move(pt::Axis::Focus, nspd);
    } else if (strcmp(q.cmd, "stop") == 0) {
        st = head.stop();
    } else if (strcmp(q.cmd, "home") == 0) {
        st = head.goHome();
        if (st == ccam::Status::Ok) {
            uint8_t p = 0;
            if (head.requestLatestPreset(p) == ccam::Status::Ok && p >= 1) {
                pt_.noteLatestPreset(p);
            }
        }
    } else if (strcmp(q.cmd, "power_off") == 0) {
        st = head.power(pt::PowerMode::Off);
    } else if (strcmp(q.cmd, "power_on") == 0) {
        st = head.power(pt::PowerMode::On);
    } else if (strcmp(q.cmd, "power_on_cam") == 0) {
        st = head.power(pt::PowerMode::OnWithCameraTx);
    } else if (strcmp(q.cmd, "power_on_nocam") == 0) {
        st = head.power(pt::PowerMode::OnWithoutCameraTx);
    } else if (strcmp(q.cmd, "set_pt") == 0) {
        if (head.capabilities().abs_pt) {
            pt_.clearAtLatest();
            auto *ipt = static_cast<ccam::devices::IntegratedPtzBase *>(&head);
            st = ipt->setPT(pt::PT{as_u16(q.v, 0x8000), as_u16(q.v2, 0x8000)});
        }
    } else if (strcmp(q.cmd, "preset_map") == 0) {
        if (head.capabilities().preset_map) {
            auto *ipt = static_cast<ccam::devices::IntegratedPtzBase *>(&head);
            bool occ[100] = {};
            st = ipt->requestPresetMap(occ);
            if (st == ccam::Status::Ok) {
                char mapbuf[640];
                size_t o = 0;
                int n = snprintf(mapbuf, sizeof(mapbuf), "{\"ok\":true,\"status\":\"ok\",\"presets\":[");
                if (n > 0) {
                    o = static_cast<size_t>(n);
                }
                bool first = true;
                for (int i = 0; i < 100 && o + 8 < sizeof(mapbuf); ++i) {
                    if (!occ[i]) {
                        continue;
                    }
                    n = snprintf(mapbuf + o, sizeof(mapbuf) - o, "%s%d", first ? "" : ",", i + 1);
                    if (n < 0) {
                        break;
                    }
                    o += static_cast<size_t>(n);
                    first = false;
                }
                if (o + 2 < sizeof(mapbuf)) {
                    mapbuf[o++] = ']';
                    mapbuf[o++] = '}';
                    mapbuf[o] = '\0';
                    return send_json(req, mapbuf);
                }
                st = ccam::Status::Buffer;
            }
        }
    } else if (strcmp(q.cmd, "preset_recall") == 0) {
        st = head.recallPreset(preset);
        if (st == ccam::Status::Ok) {
            pt_.noteLatestPreset(preset);
        }
    } else if (strcmp(q.cmd, "preset_save") == 0) {
        st = head.savePreset(preset);
        if (st == ccam::Status::Ok) {
            pt_.noteLatestPreset(preset);
        }
    } else if (strcmp(q.cmd, "preset_delete") == 0) {
        st = head.deletePreset(preset);
        if (st == ccam::Status::Ok && pt_.latestPreset() == preset) {
            pt_.noteLatestPreset(0);
        }
    } else if (strcmp(q.cmd, "preset_latest") == 0) {
        uint8_t p = 0;
        st = head.requestLatestPreset(p);
        if (st == ccam::Status::Ok) {
            pt_.noteLatestPreset(p);
            snprintf(buf, sizeof(buf),
                     "{\"ok\":true,\"status\":\"ok\",\"preset\":%u,\"latest_preset\":%u,\"at_latest\":true}",
                     p, p);
            return send_json(req, buf);
        }
    } else if (strcmp(q.cmd, "preset_mode") == 0) {
        st = head.setPresetMode(static_cast<ccam::PrMode>(as_int(q.v) & 1));
    } else if (strcmp(q.cmd, "preset_speed") == 0) {
        const int n = as_int(q.v);
        if (n >= 0 && n <= 2) {
            st = head.setPresetSpeed(static_cast<ccam::PrSpeed>(n));
        }
    } else if (strcmp(q.cmd, "preset_speed_unit") == 0) {
        st = head.setPresetSpeedUnit(q.data[0] ? q.data : "080");
    } else if (strcmp(q.cmd, "pos") == 0 || strcmp(q.cmd, "pos_disp") == 0) {
        pt::Position pos{};
        st = (strcmp(q.cmd, "pos") == 0) ? head.requestPosition(pos) : head.requestPositionDisplay(pos);
        if (st == ccam::Status::Ok) {
            snprintf(buf, sizeof(buf),
                     "{\"ok\":true,\"status\":\"ok\",\"pan\":%u,\"tilt\":%u,\"zoom\":%u,\"focus\":%u,"
                     "\"iris\":%u}",
                     pos.pt.pan, pos.pt.tilt, pos.lens.zoom, pos.lens.focus, pos.lens.iris);
            return send_json(req, buf);
        }
    } else if (strcmp(q.cmd, "set_lens") == 0) {
        pt::Lens lens{as_u16(q.v3), as_u16(q.data), as_u16(q.v4)};
        st = head.setLens(lens);
    } else if (strcmp(q.cmd, "set_zoom") == 0) {
        st = head.setZoom(as_u16(q.v));
    } else if (strcmp(q.cmd, "set_focus") == 0) {
        st = head.setFocus(as_u16(q.v));
    } else if (strcmp(q.cmd, "set_iris") == 0) {
        st = head.setIris(as_u16(q.v));
    } else if (strcmp(q.cmd, "req_zoom") == 0 || strcmp(q.cmd, "get_zoom") == 0
               || strcmp(q.cmd, "da_zoom") == 0) {
        // #GZ (не bare #AXZ — тот сбрасывает в 555h)
        uint16_t v = 0;
        st = head.getZoom(v);
        if (st == ccam::Status::Ok) {
            snprintf(buf, sizeof(buf), "{\"ok\":true,\"status\":\"ok\",\"zoom\":%u}", v);
            return send_json(req, buf);
        }
    } else if (strcmp(q.cmd, "req_focus") == 0 || strcmp(q.cmd, "get_focus") == 0
               || strcmp(q.cmd, "da_focus") == 0) {
        uint16_t v = 0;
        st = head.getFocus(v);
        if (st == ccam::Status::Ok) {
            snprintf(buf, sizeof(buf), "{\"ok\":true,\"status\":\"ok\",\"focus\":%u}", v);
            return send_json(req, buf);
        }
    } else if (strcmp(q.cmd, "req_iris") == 0 || strcmp(q.cmd, "get_iris") == 0
               || strcmp(q.cmd, "da_iris") == 0) {
        uint16_t v = 0;
        st = head.getIris(v);
        if (st == ccam::Status::Ok) {
            snprintf(buf, sizeof(buf), "{\"ok\":true,\"status\":\"ok\",\"iris\":%u}", v);
            return send_json(req, buf);
        }
    } else if (strcmp(q.cmd, "req_lens") == 0) {
        uint16_t z = 0, f = 0, i = 0;
        st = head.requestLensPositionInfo(z, f, i);
        if (st == ccam::Status::Ok) {
            snprintf(buf, sizeof(buf),
                     "{\"ok\":true,\"status\":\"ok\",\"zoom\":%u,\"focus\":%u,\"iris\":%u}", z, f, i);
            return send_json(req, buf);
        }
    } else if (strcmp(q.cmd, "lens_ctrl") == 0) {
        st = head.setLensCtrl(static_cast<ccam::Switch>(as_int(q.v) & 1));
    } else if (strcmp(q.cmd, "req_exposure") == 0) {
        pt::Exposure ex{};
        st = head.requestExposure(ex);
        if (st == ccam::Status::Ok) {
            snprintf(buf, sizeof(buf),
                     "{\"ok\":true,\"status\":\"ok\",\"gain\":%u,\"shutter\":%u,\"nd\":%u}", ex.gain,
                     ex.shutter, ex.nd);
            return send_json(req, buf);
        }
    } else if (strcmp(q.cmd, "req_error") == 0) {
        uint8_t err = 0;
        st = head.requestError(err);
        if (st == ccam::Status::Ok) {
            snprintf(buf, sizeof(buf), "{\"ok\":true,\"status\":\"ok\",\"error\":%u}", err);
            return send_json(req, buf);
        }
    } else if (strcmp(q.cmd, "req_landing") == 0) {
        ccam::LandingMode mode{};
        st = head.requestLandingMode(mode);
        if (st == ccam::Status::Ok) {
            snprintf(buf, sizeof(buf), "{\"ok\":true,\"status\":\"ok\",\"value\":%u}",
                     static_cast<unsigned>(mode));
            return send_json(req, buf);
        }
    } else if (strcmp(q.cmd, "req_ver") == 0) {
        st = head.requestSoftwareVersion(extra, sizeof(extra));
        return reply_st(req, st, extra);
    } else if (strcmp(q.cmd, "req_heater") == 0) {
        uint8_t v = 0;
        st = head.queryHeaterStatus(v);
        if (st == ccam::Status::Ok) {
            snprintf(buf, sizeof(buf), "{\"ok\":true,\"status\":\"ok\",\"value\":%u}", v);
            return send_json(req, buf);
        }
    } else if (strcmp(q.cmd, "req_defrost") == 0) {
        uint8_t v = 0;
        st = head.queryDefrosterStatus(v);
        if (st == ccam::Status::Ok) {
            snprintf(buf, sizeof(buf), "{\"ok\":true,\"status\":\"ok\",\"value\":%u}", v);
            return send_json(req, buf);
        }
    } else if (strcmp(q.cmd, "limitation") == 0 || strcmp(q.cmd, "limit_toggle") == 0) {
        // PDF: #L[1..4] toggle → l0/l1. PH350: да. Подъехать к упору перед Set.
        const int ax = as_int(q.v);
        if (ax >= 1 && ax <= 4) {
            ccam::Limitation state = ccam::Limitation::Release;
            st = head.toggleLimitation(static_cast<ccam::LimitAxis>(ax), &state);
            if (st == ccam::Status::Ok) {
                snprintf(buf, sizeof(buf),
                         "{\"ok\":true,\"status\":\"ok\",\"axis\":%d,\"limit\":\"%s\",\"data\":\"#L%d\"}",
                         ax, state == ccam::Limitation::Set ? "set" : "release", ax);
                return send_json(req, buf);
            }
        }
    } else if (strcmp(q.cmd, "limit_ctrl") == 0) {
        // #LC[axis][0|1] — только IntegratedPtz
        const int ax = as_int(q.v);
        const int act = as_int(q.v2, 1);
        if (head.capabilities().limit_ctrl && ax >= 1 && ax <= 4 && (act == 0 || act == 1)) {
            auto *ipt = static_cast<ccam::devices::IntegratedPtzBase *>(&head);
            st = ipt->setLimitationControl(static_cast<ccam::LimitAxis>(ax),
                                           static_cast<ccam::Limitation>(act));
            if (st == ccam::Status::Ok) {
    snprintf(buf, sizeof(buf),
                         "{\"ok\":true,\"status\":\"ok\",\"axis\":%d,\"limit\":\"%s\",\"data\":\"#LC\"}",
                         ax, act ? "set" : "release");
                return send_json(req, buf);
            }
        }
    } else if (strcmp(q.cmd, "tilt_range") == 0) {
        st = head.setTiltRange(static_cast<ccam::TiltRange>(as_int(q.v) & 1));
    } else if (strcmp(q.cmd, "tally") == 0) {
        st = head.setTallyEnable(static_cast<ccam::Switch>(as_int(q.v) & 1));
    } else if (strcmp(q.cmd, "wireless") == 0) {
        st = head.setWirelessControl(static_cast<ccam::Switch>(as_int(q.v) & 1));
    } else if (strcmp(q.cmd, "washer") == 0) {
        st = head.setWasher(static_cast<ccam::Switch>(as_int(q.v) & 1));
    } else if (strcmp(q.cmd, "option") == 0) {
        st = head.setOption(static_cast<ccam::Switch>(as_int(q.v) & 1));
    } else if (strcmp(q.cmd, "fan") == 0 || strcmp(q.cmd, "fan2") == 0) {
        const int n = as_int(q.v);
        if (n >= 0 && n <= 3) {
            const auto mode = static_cast<ccam::FanMode>(n);
            st = (strcmp(q.cmd, "fan") == 0) ? head.setFan(mode) : head.setFan2(mode);
        }
    } else if (strcmp(q.cmd, "wiper") == 0) {
        const int n = as_int(q.v);
        if (n >= 0 && n <= 2) {
            st = head.setWiper(static_cast<ccam::WiperMode>(n));
        }
    } else if (strcmp(q.cmd, "raw") == 0 || strcmp(q.cmd, "raw_send") == 0) {
        const char *cmd = q.data[0] ? q.data : "O";
        if (cmd[0] == '#') {
            ++cmd;
        }
        const char *payload = q.data2[0] ? q.data2 : nullptr;
        if (strcmp(q.cmd, "raw_send") == 0) {
            st = head.sendCommand(cmd, payload);
        } else {
            uint8_t rx[ccam::kPtFrameMax] = {};
            size_t n = 0;
            st = head.transactCommand(cmd, payload, rx, sizeof(rx), &n);
            if (st == ccam::Status::Ok) {
                ascii_rx(rx, n, extra, sizeof(extra));
            }
        }
        return reply_st(req, st, extra);
    }

    return reply_st(req, st);
}

esp_err_t HttpStatus::on_cam(httpd_req_t *req) {
    if (require_auth(req) != ESP_OK) {
        return ESP_OK;
    }
    if (otaIsBusy()) {
        return reply_ota_busy(req);
    }
    Q q;
    load_q(req, q);
    Ph350Ctrl::DeviceLock dev(pt_);
    if (!q.cmd[0]) {
        return reply_cam(req, pt_, ccam::Status::Param);
    }

    auto &cam = pt_.cam();
    ccam::Status st = ccam::Status::Param;
    char extra[96] = {};

    if (strcmp(q.cmd, "ranges") == 0) {
        // Каталог + menu_deps из ccam (не HTML). Без шины. Heap — как osd_ranges.
        constexpr size_t kCap = 6144;
        char *json = static_cast<char *>(malloc(kCap));
        if (!json) {
            return reply_st(req, ccam::Status::Buffer);
        }
        const auto &cat = ccam::devices::camParamCatalog(pt_.camModel());
        int n = snprintf(
            json, kCap,
            "{\"ok\":true,\"status\":\"ok\",\"cam_id\":\"%s\",\"ranges\":{",
            ccam::devices::camModelKey(pt_.camModel()));
        if (n < 0 || static_cast<size_t>(n) >= kCap) {
            free(json);
            return reply_st(req, ccam::Status::Buffer);
        }
        const int m = ccam::devices::camParamCatalogToJson(
            cat, json + n, kCap - static_cast<size_t>(n) - 64);
        if (m < 0) {
            free(json);
            return reply_st(req, ccam::Status::Buffer);
        }
        n += m;
        if (static_cast<size_t>(n) + 32 >= kCap) {
            free(json);
            return reply_st(req, ccam::Status::Buffer);
        }
        json[n++] = '}';
        const int dlen = snprintf(json + n, kCap - static_cast<size_t>(n), ",\"menu_deps\":");
        if (dlen < 0 || static_cast<size_t>(n) + static_cast<size_t>(dlen) >= kCap) {
            free(json);
            return reply_st(req, ccam::Status::Buffer);
        }
        n += dlen;
        const int md = ccam::devices::camMenuDepsToJson(json + n, kCap - static_cast<size_t>(n) - 2);
        if (md < 0) {
            free(json);
            return reply_st(req, ccam::Status::Buffer);
        }
        n += md;
        if (static_cast<size_t>(n) + 2 >= kCap) {
            free(json);
            return reply_st(req, ccam::Status::Buffer);
        }
        json[n++] = '}';
        json[n] = '\0';
        const esp_err_t rc = send_json(req, json);
        free(json);
        return rc;
    }
    if (strcmp(q.cmd, "osd_ranges") == 0) {
        char *json = static_cast<char *>(malloc(20480));
        if (!json) {
            return reply_st(req, ccam::Status::Buffer);
        }
        int n = snprintf(json, 20480, "{\"ok\":true,\"status\":\"ok\",\"items\":");
        if (n < 0 || n >= 20480) {
            free(json);
            return reply_st(req, ccam::Status::Buffer);
        }
        const int m = ccam::devices::osdParamCatalogToJson(
            pt_.camModel(), json + n, 20480 - static_cast<size_t>(n) - 2);
        if (m < 0) {
            free(json);
            return reply_st(req, ccam::Status::Buffer);
        }
        n += m;
        if (static_cast<size_t>(n) + 2 >= 20480) {
            free(json);
            return reply_st(req, ccam::Status::Buffer);
        }
        json[n++] = '}';
        json[n] = '\0';
        const esp_err_t rc = send_json(req, json);
        free(json);
        return rc;
    }
    if (strcmp(q.cmd, "model") == 0) {
        st = cam.requestModel(extra, sizeof(extra));
        return reply_cam(req, pt_, st, extra);
    }
    if (strcmp(q.cmd, "version") == 0) {
        st = cam.requestSoftwareVersion(extra, sizeof(extra));
        return reply_cam(req, pt_, st, extra);
    }
    if (strcmp(q.cmd, "params") == 0) {
        const bool deps_only = (strcmp(q.scope, "deps") == 0);
        httpd_resp_set_type(req, "application/x-ndjson");
        httpd_resp_set_hdr(req, "Cache-Control", "no-store");
        httpd_resp_set_hdr(req, "X-Content-Type-Options", "nosniff");

        bool any_ok = false;
        auto gap = []() { vTaskDelay(pdMS_TO_TICKS(20)); };
        auto emit = [&](const char *line) -> bool {
            if (!line) {
                return false;
            }
            const size_t n = strlen(line);
            if (httpd_resp_send_chunk(req, line, n) != ESP_OK) {
                return false;
            }
            return httpd_resp_send_chunk(req, "\n", 1) == ESP_OK;
        };
        auto emit_raw = [&](const char *key, ccam::Status qs, const char *val) -> bool {
            char line[160];
            int n;
            if (qs == ccam::Status::Ok) {
                any_ok = true;
                n = snprintf(line, sizeof(line), "{\"%s\":%s}", key, val);
            } else {
                n = snprintf(line, sizeof(line), "{\"%s\":\"%s\"}", key, Ph350Ctrl::statusText(qs));
            }
            if (n <= 0 || static_cast<size_t>(n) >= sizeof(line)) {
                return false;
            }
            return emit(line);
        };
        auto emit_str = [&](const char *key, ccam::Status qs, const char *s) -> bool {
            char esc[64];
            char quoted[80];
            if (qs == ccam::Status::Ok) {
                json_escape(s, esc, sizeof(esc));
                snprintf(quoted, sizeof(quoted), "\"%s\"", esc);
                return emit_raw(key, qs, quoted);
            }
            return emit_raw(key, qs, "\"\"");
        };
        auto emit_u = [&](const char *key, ccam::Status qs, unsigned v) -> bool {
            char num[16];
            snprintf(num, sizeof(num), "%u", v);
            return emit_raw(key, qs, num);
        };
        auto emit_i = [&](const char *key, ccam::Status qs, int v) -> bool {
            char num[16];
            snprintf(num, sizeof(num), "%d", v);
            return emit_raw(key, qs, num);
        };
        auto emit_osd_wire = [&](const char *key, ccam::catalog::OsdItem item) -> bool {
            char raw[40] = {};
            ccam::Status qs = cam.requestOsd(item, raw, sizeof(raw));
            const char *wire = raw;
            for (const char *p = raw; *p; ++p) {
                if (*p == ':') {
                    wire = p + 1;
                }
            }
            return emit_str(key, qs, (qs == ccam::Status::Ok) ? wire : "");
        };
        auto emit_osd_u = [&](const char *key, ccam::catalog::OsdItem item) -> bool {
            char raw[40] = {};
            ccam::Status qs = cam.requestOsd(item, raw, sizeof(raw));
            const char *wire = raw;
            for (const char *p = raw; *p; ++p) {
                if (*p == ':') {
                    wire = p + 1;
                }
            }
            unsigned v = 0;
            if (qs == ccam::Status::Ok && wire && *wire) {
                v = static_cast<unsigned>(strtoul(wire, nullptr, 16));
            }
            return emit_u(key, qs, v);
        };
        auto emit_video_wire = [&](const char *key, ccam::catalog::VideoMenuItem item) -> bool {
            char raw[40] = {};
            ccam::Status qs = cam.requestVideoMenu(item, raw, sizeof(raw));
            const char *wire = raw;
            for (const char *p = raw; *p; ++p) {
                if (*p == ':') {
                    wire = p + 1;
                }
            }
            return emit_str(key, qs, (qs == ccam::Status::Ok) ? wire : "");
        };

        if (!emit(deps_only ? "{\"ok\":true,\"stream\":true,\"deps\":true,\"status\":\"ok\"}"
                            : "{\"ok\":true,\"stream\":true,\"status\":\"ok\"}")) {
            httpd_resp_send_chunk(req, nullptr, 0);
            return ESP_FAIL;
        }

        uint8_t u8 = 0;
        uint16_t u16 = 0;
        const auto &cc = cam.capabilities();
        ccam::devices::CamMenuPrimaryState pst{};
        pst.he_awc = ccam::devices::camMenuUsesHeAwc(pt_.camModel());
        {
            const auto &cat = ccam::devices::camParamCatalog(pt_.camModel());
            if (cat.shutter.kind == ccam::devices::CamParamKind::Choices && cat.shutter.choices) {
                for (size_t i = 0; i < cat.shutter.choice_count; ++i) {
                    const auto &ch = cat.shutter.choices[i];
                    if (ch.label && ch.wire && strcmp(ch.label, "Synchro") == 0) {
                        pst.shutter_synchro = static_cast<uint8_t>(strtoul(ch.wire, nullptr, 10));
                        break;
                    }
                }
            }
        }

        auto emit_slot = [&](const ccam::devices::CamMenuPollEntry &e) -> bool {
            using Fmt = ccam::devices::CamMenuFmt;
            using Bank = ccam::devices::CamMenuBank;
            ccam::Status qs = ccam::Status::Ok;
            const char *key = e.key;

            auto as_cam = [&]() { return static_cast<ccam::catalog::CameraCmd>(e.item); };
            auto as_osd = [&]() { return static_cast<ccam::catalog::OsdItem>(e.item); };
            auto as_vid = [&]() { return static_cast<ccam::catalog::VideoMenuItem>(e.item); };

            switch (e.fmt) {
            case Fmt::GainMode: {
                int gain_db = 0;
                bool gain_agc = false;
                qs = cam.requestGain(gain_db, gain_agc);
                pst.gain_agc = gain_agc;
                if (deps_only) {
                    return true;
                }
                if (qs == ccam::Status::Ok) {
                    if (gain_agc) {
                        return emit_raw("gain_mode", qs, "\"agc\"") && emit_raw("gain", qs, "\"agc\"");
                    }
                    return emit_raw("gain_mode", qs, "\"manual\"") && emit_i("gain_db", qs, gain_db);
                }
                return emit_str("gain_mode", qs, "");
            }
            case Fmt::GainDb: {
                int db = 0;
                bool agc = false;
                qs = cam.requestGain(db, agc);
                if (qs == ccam::Status::Ok && !agc) {
                    return emit_i(key, qs, db);
                }
                return emit_str(key, qs, "");
            }
            case Fmt::TrackShutter:
                qs = cam.requestShutter(u8);
                pst.shutter = u8;
                return deps_only ? true : emit_u(key, qs, u8);
            case Fmt::IrisAm: {
                ccam::IrisAutoManual iris_am = ccam::IrisAutoManual::Manual;
                qs = cam.requestIrisAutoManual(iris_am);
                pst.iris_am = static_cast<uint8_t>(iris_am);
                return deps_only ? true : emit_u(key, qs, static_cast<unsigned>(iris_am));
            }
            case Fmt::TrackAwc:
                qs = cam.requestAwcMode(u8);
                pst.awc = u8;
                return deps_only ? true : emit_u(key, qs, u8);
            case Fmt::IrisNorm:
                qs = cam.requestManualIris(u16);
                return emit_u(key, qs, u16);
            case Fmt::Synchro:
                qs = cam.requestSynchroScan(u16);
                return emit_u(key, qs, u16);
            case Fmt::HexU16:
                switch (as_cam()) {
                case ccam::catalog::CameraCmd::RGain:
                    qs = cam.requestRGain(u16);
                    break;
                case ccam::catalog::CameraCmd::BGain:
                    qs = cam.requestBGain(u16);
                    break;
                case ccam::catalog::CameraCmd::RPedestal:
                    qs = cam.requestRPedestal(u16);
                    break;
                case ccam::catalog::CameraCmd::BPedestal:
                    qs = cam.requestBPedestal(u16);
                    break;
                case ccam::catalog::CameraCmd::TPedestal:
                    qs = cam.requestTPedestal(u16);
                    break;
                default:
                    return true;
                }
                return emit_u(key, qs, u16);
            case Fmt::DecU8:
                if (deps_only && e.phase == ccam::devices::CamMenuPhase::Primary) {
                    return true;
                }
                switch (as_cam()) {
                case ccam::catalog::CameraCmd::SceneFile:
                    qs = cam.requestSceneFile(u8);
                    break;
                case ccam::catalog::CameraCmd::DigitalGainUp:
                    qs = cam.requestDigitalGainUp(u8);
                    break;
                case ccam::catalog::CameraCmd::LightPeakAvg:
                    qs = cam.requestLightPeakAvg(u8);
                    break;
                case ccam::catalog::CameraCmd::LightArea:
                    qs = cam.requestLightArea(u8);
                    break;
                case ccam::catalog::CameraCmd::Detail:
                    qs = cam.requestDetail(u8);
                    break;
                case ccam::catalog::CameraCmd::HdDetail:
                    qs = cam.requestHdDetail(u8);
                    break;
                case ccam::catalog::CameraCmd::Dnr3d:
                    qs = cam.requestDnr3d(u8);
                    break;
                case ccam::catalog::CameraCmd::FilterNd:
                    qs = cam.requestFilter(u8);
                    break;
                default:
                    return true;
                }
                return emit_u(key, qs, u8);
            case Fmt::ChromaSigned:
                qs = cam.requestChromaLevel(u8);
                return emit_i(key, qs, static_cast<int>(u8) - 3);
            case Fmt::AirisSigned: {
                char raw[40] = {};
                qs = cam.requestOsd(as_osd(), raw, sizeof(raw));
                const char *wire = raw;
                for (const char *p = raw; *p; ++p) {
                    if (*p == ':') {
                        wire = p + 1;
                    }
                }
                int airis_ui = 0;
                if (qs == ccam::Status::Ok && wire && *wire) {
                    airis_ui = static_cast<int>(strtoul(wire, nullptr, 16)) - 50;
                    if (airis_ui < -50) {
                        airis_ui = -50;
                    }
                    if (airis_ui > 50) {
                        airis_ui = 50;
                    }
                }
                return emit_i(key, qs, airis_ui);
            }
            case Fmt::OsdWire:
                return emit_osd_wire(key, as_osd());
            case Fmt::OsdHexU:
                return emit_osd_u(key, as_osd());
            case Fmt::VideoWire:
                return emit_video_wire(key, as_vid());
            case Fmt::ColorBar: {
                ccam::ColorBarMode cbar = ccam::ColorBarMode::Camera;
                qs = cam.requestColorBar(cbar);
                return emit_u(key, qs, static_cast<unsigned>(cbar));
            }
            case Fmt::Switch: {
                ccam::Switch sw = ccam::Switch::Off;
                switch (as_cam()) {
                case ccam::catalog::CameraCmd::AutoFocus:
                    qs = cam.requestAutoFocus(sw);
                    break;
                case ccam::catalog::CameraCmd::NegPos:
                    qs = cam.requestNegPos(sw);
                    break;
                case ccam::catalog::CameraCmd::MenuOnOff:
                    qs = cam.requestMenuOnOff(sw);
                    break;
                default:
                    return true;
                }
                return emit_u(key, qs, static_cast<unsigned>(sw));
            }
            case Fmt::FieldFrame: {
                ccam::FieldFrameMode ff = ccam::FieldFrameMode::Field;
                qs = cam.requestFieldFrame(ff);
                return emit_u(key, qs, static_cast<unsigned>(ff));
            }
            case Fmt::Model: {
                char model[24] = {};
                qs = cam.requestModel(model, sizeof(model));
                return emit_str(key, qs, model);
            }
            case Fmt::Version: {
                char version[24] = {};
                qs = cam.requestSoftwareVersion(version, sizeof(version));
                return emit_str(key, qs, version);
            }
            default:
                (void)Bank::Cam;
                return true;
            }
        };

        size_t n = 0;
        const auto *poll = ccam::devices::camMenuPoll(n);
        unsigned emitted_phase = 0;

        for (size_t i = 0; i < n; ++i) {
            const auto &e = poll[i];
            if (deps_only) {
                if (e.phase == ccam::devices::CamMenuPhase::Always) {
                    continue;
                }
                if (e.phase == ccam::devices::CamMenuPhase::Dep
                    && !ccam::devices::camMenuEntryWanted(e, pst, cc, q.only, q.gates)) {
                    continue;
                }
            } else {
                if (e.phase == ccam::devices::CamMenuPhase::Dep) {
                    if (e.fmt == ccam::devices::CamMenuFmt::GainDb) {
                        continue; /* уже из GainMode */
                    }
                    if (!ccam::devices::camMenuEntryWanted(e, pst, cc, nullptr, nullptr)) {
                        continue;
                    }
                } else if (e.phase == ccam::devices::CamMenuPhase::Always) {
                    if (!ccam::devices::camMenuCapOk(e.cap, cc)) {
                        continue;
                    }
                }
                const unsigned ph = static_cast<unsigned>(e.phase);
                if (ph != emitted_phase) {
                    emitted_phase = ph;
                    char line[32];
                    snprintf(line, sizeof(line), "{\"phase\":%u}", ph);
                    if (!emit(line)) {
                        httpd_resp_send_chunk(req, nullptr, 0);
                        return ESP_FAIL;
                    }
                }
            }

            if (!emit_slot(e)) {
                httpd_resp_send_chunk(req, nullptr, 0);
                return ESP_FAIL;
            }
            gap();
        }

        char done[64];
        snprintf(done, sizeof(done), "{\"done\":true,\"ok\":%s}", any_ok ? "true" : "false");
        if (!emit(done)) {
            httpd_resp_send_chunk(req, nullptr, 0);
            return ESP_FAIL;
        }
        return httpd_resp_send_chunk(req, nullptr, 0);
    }
    if (strcmp(q.cmd, "awc_start") == 0) {
        st = cam.awcStart();
    } else if (strcmp(q.cmd, "abc_start") == 0) {
        st = cam.abcBalanceStart();
    } else if (strcmp(q.cmd, "awc_mode") == 0) {
        const int n = as_int(q.v);
        if (n >= 0 && n <= 9) {
            st = cam.setAwcMode(static_cast<uint8_t>(n));
        }
    } else if (strcmp(q.cmd, "scene") == 0) {
        const int n = as_int(q.v);
        if (n >= 0 && n <= 3) {
            st = cam.setSceneFile(static_cast<uint8_t>(n));
        }
    } else if (strcmp(q.cmd, "shutter_mode") == 0) {
        st = cam.setShutterMode(static_cast<uint8_t>(as_int(q.v)));
    } else if (strcmp(q.cmd, "synchro") == 0) {
        st = cam.setSynchroScan(q.data[0] ? q.data : "000");
    } else if (strcmp(q.cmd, "gain") == 0) {
        st = cam.setGain(as_int(q.v));
    } else if (strcmp(q.cmd, "agc") == 0) {
        st = cam.setAGC();
    } else if (strcmp(q.cmd, "detail") == 0) {
        st = cam.setDetail(static_cast<uint8_t>(as_int(q.v)));
    } else if (strcmp(q.cmd, "hd_detail") == 0) {
        st = cam.setHdDetail(static_cast<uint8_t>(as_int(q.v)));
    } else if (strcmp(q.cmd, "iris_am") == 0) {
        st = cam.setIrisAutoManual(static_cast<ccam::IrisAutoManual>(as_int(q.v) & 1));
    } else if (strcmp(q.cmd, "man_iris") == 0) {
        st = cam.setManualIris(as_u16(q.v, 0x8000));
    } else if (strcmp(q.cmd, "filter") == 0) {
        st = cam.setFilter(static_cast<uint8_t>(as_int(q.v)));
    } else if (strcmp(q.cmd, "colorbar") == 0) {
        const int n = as_int(q.v);
        if (n >= 0 && n <= static_cast<int>(ccam::ColorBarMode::ColorBar)) {
            st = cam.setColorBar(static_cast<ccam::ColorBarMode>(n));
        }
    } else if (strcmp(q.cmd, "menu") == 0) {
        st = cam.setMenuOnOff(static_cast<ccam::Switch>(as_int(q.v) & 1));
    } else if (strcmp(q.cmd, "menu_sw") == 0) {
        st = cam.menuSw();
    } else if (strcmp(q.cmd, "item_sw") == 0) {
        st = cam.itemSw();
    } else if (strcmp(q.cmd, "yes_sw") == 0) {
        st = cam.yesSw();
    } else if (strcmp(q.cmd, "no_sw") == 0) {
        st = cam.noSw();
    } else if (strcmp(q.cmd, "left_sw") == 0 || strcmp(q.cmd, "right_sw") == 0) {
        if (pt_.camModel() == ccam::devices::CameraModelId::He130) {
            auto &he = static_cast<ccam::devices::He130Camera &>(cam);
            st = (strcmp(q.cmd, "left_sw") == 0) ? he.leftSw() : he.rightSw();
        } else {
            st = ccam::Status::Param;
        }
    } else if (strcmp(q.cmd, "af") == 0) {
        st = cam.setAutoFocus(static_cast<ccam::Switch>(as_int(q.v) & 1));
    } else if (strcmp(q.cmd, "negpos") == 0) {
        st = cam.setNegPos(as_int(q.v) != 0);
    } else if (strcmp(q.cmd, "fieldframe") == 0) {
        const int n = as_int(q.v);
        if (n >= 0 && n <= 2) {
            st = cam.setFieldFrame(static_cast<ccam::FieldFrameMode>(n));
        }
    } else if (strcmp(q.cmd, "chroma") == 0) {
        // OCG: 00=-3 .. 03=0 .. 06=+3; UI шлёт −3…+3 (или legacy 0…6)
        int n = as_int(q.v);
        if (n >= -3 && n <= 3) {
            n += 3;
        }
        if (n >= 0 && n <= 6) {
            st = cam.setChromaLevel(static_cast<ccam::Levels>(n));
        }
    } else if (strcmp(q.cmd, "black_shade") == 0) {
        st = cam.setBlackShadingCorrect(as_int(q.v) != 0);
    } else if (strcmp(q.cmd, "r_gain") == 0) {
        // ORG: 00h=-30 .. 1Eh=0 .. 3Ch=+30 (reply OGR)
        st = cam.setRGain(q.data[0] ? q.data : "1E");
    } else if (strcmp(q.cmd, "b_gain") == 0) {
        st = cam.setBGain(q.data[0] ? q.data : "1E");
    } else if (strcmp(q.cmd, "r_ped") == 0) {
        // ORD: 00h=-30 .. 1Eh=0 .. 3Ch=+30
        st = cam.setRPedestal(q.data[0] ? q.data : "1E");
    } else if (strcmp(q.cmd, "b_ped") == 0) {
        st = cam.setBPedestal(q.data[0] ? q.data : "1E");
    } else if (strcmp(q.cmd, "t_ped") == 0) {
        st = cam.setTPedestal(q.data[0] ? q.data : "1E");
    } else if (strcmp(q.cmd, "dig_gain") == 0) {
        const int n = as_int(q.v);
        if (n >= 0 && n <= 5) {
            st = cam.setDigitalGainUp(static_cast<uint8_t>(n));
        }
    } else if (strcmp(q.cmd, "light_peak") == 0) {
        st = cam.setLightPeakAvg(q.data[0] ? q.data : "32");
    } else if (strcmp(q.cmd, "light_area") == 0) {
        st = cam.setLightArea(static_cast<uint8_t>(as_int(q.v)));
    } else if (strcmp(q.cmd, "dnr3d") == 0) {
        const int n = as_int(q.v);
        if (n >= 0 && n <= 2) {
            st = cam.setDnr3d(static_cast<uint8_t>(n));
        }
    } else if (strcmp(q.cmd, "gamma") == 0) {
        st = cam.setGamma(q.data[0] ? q.data : "0");
    } else if (strcmp(q.cmd, "agc_max") == 0) {
        st = cam.setAgcMax(q.data[0] ? q.data : "0");
    } else if (strcmp(q.cmd, "total_dtl") == 0) {
        st = cam.setTotalDtlLevel(q.data[0] ? q.data : "0");
    } else if (strcmp(q.cmd, "genlock") == 0) {
        st = cam.setGenlockInput(q.data[0] ? q.data : "0");
    } else if (strcmp(q.cmd, "monitor") == 0) {
        st = cam.setMonitor(q.data[0] ? q.data : "00", q.data2[0] ? q.data2 : "0");
    } else if (strcmp(q.cmd, "lens_h") == 0) {
        st = cam.lensContact(q.data[0] ? q.data : "00");
    } else if (strcmp(q.cmd, "scene_data") == 0) {
        st = cam.selectScene(q.data[0] ? q.data : "0");
    } else if (strcmp(q.cmd, "raw") == 0 || strcmp(q.cmd, "raw_send") == 0) {
        // data = cmd3 (OAW/QAW/OWS…), data2 = payload после ':' (пусто → trigger/query)
        const char *cmd3 = q.data[0] ? q.data : "";
        const char *payload = q.data2[0] ? q.data2 : nullptr;
        if (strcmp(q.cmd, "raw_send") == 0) {
            st = payload ? cam.sendRaw(cmd3, payload) : cam.triggerRaw(cmd3);
        } else if (payload) {
            st = cam.sendRaw(cmd3, payload);
        } else {
            st = cam.requestRaw(cmd3, extra, sizeof(extra));
            return reply_cam(req, pt_, st, extra);
        }
    } else if (strcmp(q.cmd, "osd_get") == 0) {
        st = ccam::devices::getOsdParam(cam, q.v, extra, sizeof(extra));
        return reply_cam(req, pt_, st, extra);
    } else if (strcmp(q.cmd, "osd_set") == 0) {
        st = ccam::devices::setOsdParam(cam, q.v, q.data);
    }

    return reply_cam(req, pt_, st);
}

esp_err_t HttpStatus::root_get(httpd_req_t *req) {
    return self(req).on_root(req);
}

esp_err_t HttpStatus::status_get(httpd_req_t *req) {
    return self(req).on_status(req);
}

esp_err_t HttpStatus::ptz_get(httpd_req_t *req) {
    return self(req).on_ptz(req);
}

esp_err_t HttpStatus::cam_get(httpd_req_t *req) {
    return self(req).on_cam(req);
}

esp_err_t HttpStatus::auth_get(httpd_req_t *req) {
    return self(req).on_auth(req);
}

namespace {

void net_reboot_cb(void *)
{
    esp_restart();
}

bool arm_net_reboot()
{
    static esp_timer_handle_t timer = nullptr;
    if (timer == nullptr) {
        esp_timer_create_args_t args = {};
        args.callback = &net_reboot_cb;
        args.name = "net_reboot";
        if (esp_timer_create(&args, &timer) != ESP_OK) {
            timer = nullptr;
        }
    }
    if (timer != nullptr) {
        if (esp_timer_is_active(timer)) {
            return true;
        }
        if (esp_timer_start_once(timer, 800000) == ESP_OK) {
            return true;
        }
    }
    return xTaskCreate(
               [](void *) {
                   vTaskDelay(pdMS_TO_TICKS(800));
                   esp_restart();
               },
               "net_reboot", 3072, nullptr, 5, nullptr) == pdPASS;
}

} // namespace

esp_err_t HttpStatus::on_net(httpd_req_t *req) {
    if (require_auth(req) != ESP_OK) {
        return ESP_OK;
    }
    Q q;
    load_q(req, q);
    if (strcmp(q.cmd, "set") != 0) {
        return send_json(req, "{\"ok\":false,\"status\":\"param\"}");
    }
    uint32_t ip = 0;
    uint32_t mask = 0;
    uint32_t gw = 0;
    if (!parse_ip4_str(q.v, &ip) || !parse_ip4_str(q.v2, &mask)) {
        return send_json(req, "{\"ok\":false,\"status\":\"param\"}");
    }
    if (q.v3[0] && !parse_ip4_str(q.v3, &gw)) {
        return send_json(req, "{\"ok\":false,\"status\":\"param\"}");
    }
    uint32_t prev_ip = 0;
    uint32_t prev_mask = 0;
    uint32_t prev_gw = 0;
    uint32_t prev_dns = 0;
    eth_.readCfg(&prev_ip, &prev_mask, &prev_gw, &prev_dns);
    if (!eth_.configure(ip, mask, gw)) {
        return send_json(req, "{\"ok\":false,\"status\":\"fail\"}");
    }
    if (!arm_net_reboot()) {
        (void)eth_.commitCfg(prev_ip, prev_mask, prev_gw, prev_dns);
        return send_json(req, "{\"ok\":false,\"status\":\"fail\"}");
    }
    char ip_s[16] = {};
    char mask_s[16] = {};
    char gw_s[16] = {};
    eth_.cfg_ip_text(ip_s, sizeof(ip_s));
    eth_.cfg_mask_text(mask_s, sizeof(mask_s));
    eth_.cfg_gw_text(gw_s, sizeof(gw_s));
    char buf[192];
    snprintf(buf, sizeof(buf),
             "{\"ok\":true,\"status\":\"ok\",\"reboot\":true,\"ip\":\"%s\",\"mask\":\"%s\",\"gw\":\"%s\"}",
             ip_s, mask_s, gw_s);
    /* Ответ уходит до таймера перезагрузки (800 мс). NVS уже записан. */
    return send_json(req, buf);
}

esp_err_t HttpStatus::net_get(httpd_req_t *req) {
    return self(req).on_net(req);
}

void HttpStatus::stop() {
    HttpMu guard(mu_);
    if (!srv_) {
        return;
    }
    httpd_stop(srv_);
    srv_ = nullptr;
}

void HttpStatus::start() {
    HttpMu guard(mu_);
    if (srv_) {
        return;
    }
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.lru_purge_enable = true;
    cfg.max_uri_handlers = 12;
    cfg.stack_size = 10240;
    if (httpd_start(&srv_, &cfg) != ESP_OK) {
        ESP_LOGE(TAG, "httpd start failed");
        return;
    }
    httpd_uri_t root = {};
    root.uri = "/";
    root.method = HTTP_GET;
    root.handler = root_get;
    root.user_ctx = this;
    httpd_uri_t status = {};
    status.uri = "/api/status";
    status.method = HTTP_GET;
    status.handler = status_get;
    status.user_ctx = this;
    httpd_uri_t ptz = {};
    ptz.uri = "/api/ptz";
    ptz.method = HTTP_GET;
    ptz.handler = ptz_get;
    ptz.user_ctx = this;
    httpd_uri_t cam = {};
    cam.uri = "/api/cam";
    cam.method = HTTP_GET;
    cam.handler = cam_get;
    cam.user_ctx = this;
    httpd_uri_t auth = {};
    auth.uri = "/api/auth";
    auth.method = HTTP_GET;
    auth.handler = auth_get;
    auth.user_ctx = this;
    httpd_uri_t net = {};
    net.uri = "/api/net";
    net.method = HTTP_GET;
    net.handler = net_get;
    net.user_ctx = this;
    httpd_register_uri_handler(srv_, &root);
    httpd_register_uri_handler(srv_, &status);
    httpd_register_uri_handler(srv_, &ptz);
    httpd_register_uri_handler(srv_, &cam);
    httpd_register_uri_handler(srv_, &auth);
    httpd_register_uri_handler(srv_, &net);
    ESP_LOGI(TAG, "HTTP listen :80");
}
