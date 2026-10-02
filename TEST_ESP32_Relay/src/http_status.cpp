#include "http_status.h"

#include <stdio.h>

#include "esp_log.h"
#include "eth_lan8720.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "http";

static const char kPage[] = R"html(<!DOCTYPE html>
<html lang="ru">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>ESP32-ETH01</title>
<style>
body{margin:0;font-family:Segoe UI,sans-serif;background:#111;color:#eee}
main{max-width:640px;margin:0 auto;padding:24px}
h1{font-size:22px;margin:0 0 16px}
table{border-collapse:collapse;width:100%}
td{padding:8px 0;border-bottom:1px solid #333}
td:first-child{color:#9aa;width:8em}
#err{color:#e74c3c}
</style>
</head>
<body>
<main>
<h1>ESP32-ETH01</h1>
<p id="err"></p>
<table>
<tr><td>Link</td><td id="link">…</td></tr>
<tr><td>IP</td><td id="ip">…</td></tr>
<tr><td>Mask</td><td id="mask">…</td></tr>
<tr><td>GW</td><td id="gw">…</td></tr>
<tr><td>MAC</td><td id="mac">…</td></tr>
<tr><td>Uptime</td><td id="up">…</td></tr>
</table>
</main>
<script>
async function tick(){
  try{
    const r=await fetch('/api/status',{cache:'no-store'});
    const d=await r.json();
    document.getElementById('err').textContent='';
    document.getElementById('link').textContent=d.link?'up':'down';
    document.getElementById('ip').textContent=d.ip;
    document.getElementById('mask').textContent=d.mask;
    document.getElementById('gw').textContent=d.gw;
    document.getElementById('mac').textContent=d.mac;
    document.getElementById('up').textContent=d.uptime_s+' s';
  }catch(e){
    document.getElementById('err').textContent='нет ответа';
  }
}
tick();
setInterval(tick,2000);
</script>
</body>
</html>
)html";

HttpStatus::HttpStatus(EthLan8720 &eth) : eth_(eth) {}

HttpStatus &HttpStatus::self(httpd_req_t *req) {
    return *static_cast<HttpStatus *>(req->user_ctx);
}

esp_err_t HttpStatus::on_root(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_send(req, kPage, HTTPD_RESP_USE_STRLEN);
}

esp_err_t HttpStatus::on_status(httpd_req_t *req) {
    char ip[16] = {};
    char mask[16] = {};
    char gw[16] = {};
    char mac[18] = {};
    eth_.ip_text(ip, sizeof(ip));
    eth_.mask_text(mask, sizeof(mask));
    eth_.gw_text(gw, sizeof(gw));
    eth_.mac_text(mac, sizeof(mac));
    const uint32_t uptime_s = static_cast<uint32_t>(xTaskGetTickCount() / configTICK_RATE_HZ);

    char buf[256];
    snprintf(buf, sizeof(buf),
             "{\"link\":%s,\"ip\":\"%s\",\"mask\":\"%s\",\"gw\":\"%s\",\"mac\":\"%s\",\"uptime_s\":%lu}",
             eth_.link_up() ? "true" : "false", ip, mask, gw, mac, static_cast<unsigned long>(uptime_s));
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_send(req, buf, HTTPD_RESP_USE_STRLEN);
}

esp_err_t HttpStatus::root_get(httpd_req_t *req) {
    return self(req).on_root(req);
}

esp_err_t HttpStatus::status_get(httpd_req_t *req) {
    return self(req).on_status(req);
}

void HttpStatus::stop() {
    if (!srv_) {
        return;
    }
    httpd_stop(srv_);
    srv_ = nullptr;
}

void HttpStatus::start() {
    if (srv_) {
        return;
    }
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.lru_purge_enable = true;
    cfg.max_uri_handlers = 4;
    cfg.stack_size = 8192;
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
    httpd_register_uri_handler(srv_, &root);
    httpd_register_uri_handler(srv_, &status);
    ESP_LOGI(TAG, "HTTP listen :80");
}
