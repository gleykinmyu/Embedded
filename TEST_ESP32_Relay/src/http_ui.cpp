#include "http_ui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "board_pins.h"
#include "eth_w5500.h"
#include "esp_log.h"
#include "relay_board.h"

static const char *TAG = "http";

extern const char index_html_start[] asm("_binary_index_html_start");
extern const char index_html_end[] asm("_binary_index_html_end");

HttpUi::HttpUi(RelayBoard &relays, EthW5500 &eth) : relays_(relays), eth_(eth) {}

HttpUi &HttpUi::self(httpd_req_t *req) {
    return *static_cast<HttpUi *>(req->user_ctx);
}

void HttpUi::add(const char *uri, esp_err_t (*handler)(httpd_req_t *)) {
    httpd_uri_t u = {};
    u.uri = uri;
    u.method = HTTP_GET;
    u.handler = handler;
    u.user_ctx = this;
    httpd_register_uri_handler(srv_, &u);
}

esp_err_t HttpUi::send_json(httpd_req_t *req) {
    char ip[16];
    eth_.ip_text(ip, sizeof(ip));
    const auto snap = relays_.snapshot();

    char buf[192];
    if (snap.busy) {
        snprintf(buf, sizeof(buf), "{\"ip\":\"%s\",\"running\":false,\"relays\":[],\"err\":\"busy\"}",
                 ip);
    } else {
        int n = snprintf(buf, sizeof(buf), "{\"ip\":\"%s\",\"running\":%s,\"relays\":[", ip,
                         snap.chase ? "true" : "false");
        for (size_t ch = 0; ch < RELAY_COUNT && n > 0 && static_cast<size_t>(n) < sizeof(buf); ++ch) {
            n += snprintf(buf + n, sizeof(buf) - static_cast<size_t>(n), "%s%d", ch ? "," : "",
                          snap.on[ch] ? 1 : 0);
        }
        if (n > 0 && static_cast<size_t>(n) < sizeof(buf)) {
            snprintf(buf + n, sizeof(buf) - static_cast<size_t>(n), "]}");
        }
    }
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_send(req, buf, HTTPD_RESP_USE_STRLEN);
}

esp_err_t HttpUi::on_root(httpd_req_t *req) {
    size_t len = static_cast<size_t>(index_html_end - index_html_start);
    if (len > 0 && index_html_start[len - 1] == '\0') {
        --len;
    }
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_send(req, index_html_start, static_cast<ssize_t>(len));
}

esp_err_t HttpUi::on_relays(httpd_req_t *req) {
    return send_json(req);
}

esp_err_t HttpUi::on_relay(httpd_req_t *req) {
    char query[48] = {};
    char chs[8] = {};
    char ons[8] = {};
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) != ESP_OK ||
        httpd_query_key_value(query, "ch", chs, sizeof(chs)) != ESP_OK ||
        httpd_query_key_value(query, "on", ons, sizeof(ons)) != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "ch,on");
        return ESP_FAIL;
    }
    const int ch = atoi(chs);
    const int on = atoi(ons);
    if (ch < 0 || static_cast<size_t>(ch) >= RELAY_COUNT || (on != 0 && on != 1)) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "ch,on");
        return ESP_FAIL;
    }
    if (!relays_.set(static_cast<size_t>(ch), on == 1)) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "busy");
        return ESP_FAIL;
    }
    return send_json(req);
}

esp_err_t HttpUi::on_chase_start(httpd_req_t *req) {
    if (!relays_.chase_start()) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "busy");
        return ESP_FAIL;
    }
    return send_json(req);
}

esp_err_t HttpUi::on_chase_stop(httpd_req_t *req) {
    if (!relays_.chase_stop()) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "busy");
        return ESP_FAIL;
    }
    return send_json(req);
}

esp_err_t HttpUi::root_get(httpd_req_t *req) {
    auto &ui = self(req);
    ui.eth_.note_http_rx();
    return ui.on_root(req);
}

esp_err_t HttpUi::relays_get(httpd_req_t *req) {
    auto &ui = self(req);
    ui.eth_.note_http_rx();
    return ui.on_relays(req);
}

esp_err_t HttpUi::relay_get(httpd_req_t *req) {
    auto &ui = self(req);
    ui.eth_.note_http_rx();
    return ui.on_relay(req);
}

esp_err_t HttpUi::chase_start_get(httpd_req_t *req) {
    auto &ui = self(req);
    ui.eth_.note_http_rx();
    return ui.on_chase_start(req);
}

esp_err_t HttpUi::chase_stop_get(httpd_req_t *req) {
    auto &ui = self(req);
    ui.eth_.note_http_rx();
    return ui.on_chase_stop(req);
}

void HttpUi::stop() {
    if (!srv_) {
        return;
    }
    httpd_stop(srv_);
    srv_ = nullptr;
}

void HttpUi::start() {
    if (srv_) {
        return;
    }
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.lru_purge_enable = true;
    cfg.max_uri_handlers = 8;
    if (httpd_start(&srv_, &cfg) != ESP_OK) {
        ESP_LOGE(TAG, "httpd start failed");
        return;
    }
    add("/", root_get);
    add("/api/relays", relays_get);
    add("/api/relay", relay_get);
    add("/api/chase/start", chase_start_get);
    add("/api/chase/stop", chase_stop_get);
    ESP_LOGI(TAG, "HTTP listen :80");
}
