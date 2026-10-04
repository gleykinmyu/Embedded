#include "http_status.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_log.h"
#include "eth_lan8720.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ph350_ctrl.h"
#include "protocol/pt_types.hpp"

static const char *TAG = "http";

static const char kPage[] = R"html(<!DOCTYPE html>
<html lang="ru">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,user-scalable=no">
<title>AW-PH350E</title>
<style>
:root{--bg:#121418;--fg:#e8eaed;--muted:#8b93a0;--line:#2a3038;--acc:#3d8bfd;--danger:#e74c3c}
*{box-sizing:border-box}
body{margin:0;font-family:Segoe UI,system-ui,sans-serif;background:var(--bg);color:var(--fg)}
main{max-width:520px;margin:0 auto;padding:20px}
h1{font-size:22px;margin:0 0 4px}
.sub{color:var(--muted);margin:0 0 18px;font-size:14px}
.pad{display:grid;grid-template-columns:1fr 1fr 1fr;gap:10px;max-width:280px;margin:0 auto 18px}
.pad button,.row button{appearance:none;border:1px solid var(--line);background:#1c2128;color:var(--fg);
  border-radius:10px;padding:16px 10px;font-size:16px;cursor:pointer}
.pad button:active,.row button:active{background:var(--acc);border-color:var(--acc)}
.pad .sp{visibility:hidden}
.row{display:flex;gap:10px;flex-wrap:wrap;margin-bottom:14px}
.row label{display:flex;align-items:center;gap:8px;color:var(--muted)}
input[type=range]{width:140px}
table{border-collapse:collapse;width:100%;margin-top:8px}
td{padding:8px 0;border-bottom:1px solid var(--line);font-size:14px}
td:first-child{color:var(--muted);width:7em}
#err{color:var(--danger);min-height:1.2em}
.danger{border-color:#6b2a2a!important;background:#2a1717!important}
</style>
</head>
<body>
<main>
<h1>AW-PH350E</h1>
<p class="sub">ESP32-ETH01 · RS-422 UART1 · ccam</p>
<p id="err"></p>
<div class="pad">
  <button class="sp">·</button>
  <button ontouchstart="ptz('tilt_up');return false" onmousedown="ptz('tilt_up')"
          ontouchend="ptz('stop');return false" onmouseup="ptz('stop')" onmouseleave="ptz('stop')">▲</button>
  <button class="sp">·</button>
  <button ontouchstart="ptz('pan_left');return false" onmousedown="ptz('pan_left')"
          ontouchend="ptz('stop');return false" onmouseup="ptz('stop')" onmouseleave="ptz('stop')">◀</button>
  <button class="danger" onclick="ptz('stop')">■</button>
  <button ontouchstart="ptz('pan_right');return false" onmousedown="ptz('pan_right')"
          ontouchend="ptz('stop');return false" onmouseup="ptz('stop')" onmouseleave="ptz('stop')">▶</button>
  <button class="sp">·</button>
  <button ontouchstart="ptz('tilt_down');return false" onmousedown="ptz('tilt_down')"
          ontouchend="ptz('stop');return false" onmouseup="ptz('stop')" onmouseleave="ptz('stop')">▼</button>
  <button class="sp">·</button>
</div>
<div class="row">
  <label>скорость <span id="spd">30</span>
    <input id="rate" type="range" min="1" max="49" value="30" oninput="spd.textContent=this.value">
  </label>
  <button onclick="ptz('home')">Home</button>
  <button onclick="ptz('power_on')">Power ON</button>
  <button onclick="ptz('power_off')">Power OFF</button>
</div>
<div class="row">
  <label>пресет <input id="pr" type="number" min="1" max="99" value="1" style="width:4em;background:#1c2128;color:var(--fg);border:1px solid var(--line);border-radius:8px;padding:8px"></label>
  <button onclick="ptz('preset_recall')">Recall</button>
  <button onclick="ptz('preset_save')">Save</button>
  <button onclick="ptz('pos')">Pos</button>
</div>
<table>
<tr><td>Link</td><td id="link">…</td></tr>
<tr><td>IP</td><td id="ip">…</td></tr>
<tr><td>PT</td><td id="pt">…</td></tr>
<tr><td>Pos</td><td id="pos">—</td></tr>
<tr><td>Uptime</td><td id="up">…</td></tr>
</table>
</main>
<script>
async function tick(){
  try{
    const r=await fetch('/api/status',{cache:'no-store'});
    const d=await r.json();
    err.textContent='';
    link.textContent=d.link?'up':'down';
    ip.textContent=d.ip;
    pt.textContent=d.model+' · rate '+d.rate;
    up.textContent=d.uptime_s+' s';
    if(d.rate){rate.value=d.rate;spd.textContent=d.rate}
  }catch(e){err.textContent='нет ответа'}
}
async function ptz(cmd){
  const q=new URLSearchParams({cmd,rate:rate.value,preset:pr.value});
  try{
    const r=await fetch('/api/ptz?'+q,{cache:'no-store'});
    const d=await r.json();
    err.textContent=d.ok?'':(d.status||'error');
    if(d.pan!==undefined){
      pos.textContent='pan 0x'+d.pan.toString(16).padStart(4,'0')+
        ' tilt 0x'+d.tilt.toString(16).padStart(4,'0');
    }
  }catch(e){err.textContent='ptz fail'}
}
tick();
setInterval(tick,3000);
</script>
</body>
</html>
)html";

HttpStatus::HttpStatus(EthLan8720 &eth, Ph350Ctrl &pt) : eth_(eth), pt_(pt) {}

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

    char buf[320];
    snprintf(buf, sizeof(buf),
             "{\"link\":%s,\"ip\":\"%s\",\"mask\":\"%s\",\"gw\":\"%s\",\"mac\":\"%s\","
             "\"uptime_s\":%lu,\"model\":\"%s\",\"rate\":%u}",
             eth_.link_up() ? "true" : "false", ip, mask, gw, mac,
             static_cast<unsigned long>(uptime_s), pt_.model(),
             static_cast<unsigned>(pt_.defaultRate()));
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_send(req, buf, HTTPD_RESP_USE_STRLEN);
}

esp_err_t HttpStatus::on_ptz(httpd_req_t *req) {
    char query[128] = {};
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "query");
        return ESP_FAIL;
    }

    char cmd[32] = {};
    char rate_s[8] = {};
    char preset_s[8] = {};
    (void)httpd_query_key_value(query, "cmd", cmd, sizeof(cmd));
    (void)httpd_query_key_value(query, "rate", rate_s, sizeof(rate_s));
    (void)httpd_query_key_value(query, "preset", preset_s, sizeof(preset_s));

    uint8_t rate = pt_.defaultRate();
    if (rate_s[0]) {
        const int n = atoi(rate_s);
        if (n >= 1 && n <= 49) {
            rate = static_cast<uint8_t>(n);
            pt_.setDefaultRate(rate);
        }
    }
    uint8_t preset = 1;
    if (preset_s[0]) {
        const int n = atoi(preset_s);
        if (n >= 1 && n <= 99) {
            preset = static_cast<uint8_t>(n);
        }
    }

    ccam::Status st = ccam::Status::Param;
    ccam::PtPosition pos{};
    bool have_pos = false;

    if (strcmp(cmd, "pan_left") == 0) {
        st = pt_.pan(ccam::PtAxisDir::Left, rate);
    } else if (strcmp(cmd, "pan_right") == 0) {
        st = pt_.pan(ccam::PtAxisDir::Right, rate);
    } else if (strcmp(cmd, "tilt_up") == 0) {
        st = pt_.tilt(ccam::PtAxisDir::Up, rate);
    } else if (strcmp(cmd, "tilt_down") == 0) {
        st = pt_.tilt(ccam::PtAxisDir::Down, rate);
    } else if (strcmp(cmd, "stop") == 0) {
        st = pt_.stop();
    } else if (strcmp(cmd, "home") == 0) {
        st = pt_.home();
    } else if (strcmp(cmd, "power_on") == 0) {
        st = pt_.power(true);
    } else if (strcmp(cmd, "power_off") == 0) {
        st = pt_.power(false);
    } else if (strcmp(cmd, "preset_recall") == 0) {
        st = pt_.recallPreset(preset);
    } else if (strcmp(cmd, "preset_save") == 0) {
        st = pt_.savePreset(preset);
    } else if (strcmp(cmd, "pos") == 0) {
        st = pt_.queryPosition(pos);
        have_pos = (st == ccam::Status::Ok);
    }

    char buf[192];
    if (have_pos) {
        snprintf(buf, sizeof(buf),
                 "{\"ok\":true,\"status\":\"ok\",\"pan\":%u,\"tilt\":%u,\"zoom\":%u,\"focus\":%u,"
                 "\"iris\":%u}",
                 pos.pan, pos.tilt, pos.zoom, pos.focus, pos.iris);
    } else {
        snprintf(buf, sizeof(buf), "{\"ok\":%s,\"status\":\"%s\"}",
                 st == ccam::Status::Ok ? "true" : "false", Ph350Ctrl::statusText(st));
    }
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

esp_err_t HttpStatus::ptz_get(httpd_req_t *req) {
    return self(req).on_ptz(req);
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
    cfg.max_uri_handlers = 6;
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
    httpd_uri_t ptz = {};
    ptz.uri = "/api/ptz";
    ptz.method = HTTP_GET;
    ptz.handler = ptz_get;
    ptz.user_ctx = this;
    httpd_register_uri_handler(srv_, &root);
    httpd_register_uri_handler(srv_, &status);
    httpd_register_uri_handler(srv_, &ptz);
    ESP_LOGI(TAG, "HTTP listen :80");
}
