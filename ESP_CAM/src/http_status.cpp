#include "http_status.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_log.h"
#include "eth_lan8720.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "catalog/osd_menu.hpp"
#include "devices/cam_param_ranges.hpp"
#include "devices/osd_params.hpp"
#include "devices/integrated_ptz.hpp"
#include "devices/models/he130.hpp"
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
};

void load_q(httpd_req_t *req, Q &q) {
    char query[256] = {};
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
}

int as_int(const char *s, int def = 0) {
    return s && s[0] ? atoi(s) : def;
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
}

HttpStatus &HttpStatus::self(httpd_req_t *req) {
    return *static_cast<HttpStatus *>(req->user_ctx);
}

esp_err_t HttpStatus::on_root(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    // TEXT embed: null-terminated; end includes trailing NUL.
    const size_t len = static_cast<size_t>(status_html_end - status_html_start);
    const size_t send_len = (len > 0 && status_html_start[len - 1] == '\0') ? (len - 1) : len;
    return httpd_resp_send(req, status_html_start, send_len);
}

esp_err_t HttpStatus::on_status(httpd_req_t *req) {
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
    char buf[1280];
    const int wrote = snprintf(
        buf, sizeof(buf),
        "{\"link\":%s,\"ip\":\"%s\",\"mask\":\"%s\",\"gw\":\"%s\",\"mac\":\"%s\","
        "\"uptime_s\":%lu,\"model\":\"%s\",\"cam\":\"%s\","
        "\"pt_id\":\"%s\",\"cam_id\":\"%s\",\"rate\":%u,"
        "\"pt_alive\":%s,\"pt_power\":%u,\"pt_power_s\":\"%s\",\"pt_powered\":%s,"
        "\"pt_optics\":%s,\"pt_optics_fail\":%u,"
        "\"pt_fail\":%u,\"pt_error\":%u,"
        "\"cam_tx\":%s,\"cam_alive\":%s,\"cam_qid\":\"%s\",\"cam_fail\":%u,"
        "\"inv_pan\":%s,\"inv_tilt\":%s,\"inv_zoom\":%s,\"inv_focus\":%s,"
        "\"pt_caps\":{\"abs_pt\":%s,\"abs_pt_speed\":%s,\"preset_map\":%s,"
        "\"limit_ctrl\":%s,\"limit_toggle\":%s,\"tally\":%s,\"fan\":%s,\"wiper\":%s,"
        "\"washer\":%s,\"wireless\":%s,\"option\":%s,\"tilt_range\":%s,"
        "\"heater\":%s,\"defroster\":%s,\"pos_disp\":%s,\"pt_pos\":%s,\"lens_q\":%s,"
        "\"exposure_q\":%s,\"landing\":%s,\"software_ver\":%s,\"power_cam_tx\":%s},"
        "\"cam_caps\":{\"hd_detail\":%s,\"neg_pos\":%s,\"field_frame\":%s,"
        "\"chroma\":%s,\"black_shade\":%s,\"synchro\":%s,\"abc\":%s,\"paint\":%s,"
        "\"genlock\":%s,\"gamma\":%s,\"agc_max\":%s,\"total_dtl\":%s,"
        "\"monitor\":%s,\"lens_h\":%s,\"menu_lr\":%s}}",
        eth_.link_up() ? "true" : "false", ip, mask, gw, mac,
        static_cast<unsigned long>(uptime_s), pt_.pt().modelLabel(), pt_.cam().modelLabel(),
        ccam::devices::ptModelKey(pt_.ptModel()), ccam::devices::camModelKey(pt_.camModel()),
        static_cast<unsigned>(pt_.defaultRate()), hl.alive() ? "true" : "false", pwr,
        hl.powerLabel(), hl.powered() ? "true" : "false",
        hl.opticsAlive() ? "true" : "false", static_cast<unsigned>(hl.opticsFailStreak()),
        static_cast<unsigned>(hl.failStreak()), pt_err,
        hl.camTxLikely() ? "true" : "false", hl.camAlive() ? "true" : "false", cam_qid,
        static_cast<unsigned>(hl.camFailStreak()),
        inv.pan ? "true" : "false", inv.tilt ? "true" : "false", inv.zoom ? "true" : "false",
        inv.focus ? "true" : "false", pc.abs_pt ? "true" : "false",
        pc.abs_pt_speed ? "true" : "false", pc.preset_map ? "true" : "false",
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
        st = head.move(pt::Axis::Pan, nspd);
    } else if (strcmp(q.cmd, "pan_right") == 0) {
        st = head.move(pt::Axis::Pan, spd);
    } else if (strcmp(q.cmd, "tilt_up") == 0) {
        st = head.move(pt::Axis::Tilt, spd);
    } else if (strcmp(q.cmd, "tilt_down") == 0) {
        st = head.move(pt::Axis::Tilt, nspd);
    } else if (strcmp(q.cmd, "zoom_tele") == 0) {
        st = head.move(pt::Axis::Zoom, spd);
    } else if (strcmp(q.cmd, "zoom_wide") == 0) {
        st = head.move(pt::Axis::Zoom, nspd);
    } else if (strcmp(q.cmd, "focus_far") == 0) {
        st = head.move(pt::Axis::Focus, spd);
    } else if (strcmp(q.cmd, "focus_near") == 0) {
        st = head.move(pt::Axis::Focus, nspd);
    } else if (strcmp(q.cmd, "stop") == 0) {
        st = head.stop();
    } else if (strcmp(q.cmd, "home") == 0) {
        st = head.goHome();
    } else if (strcmp(q.cmd, "boot") == 0) {
        // Модельный power(On): PH → #O1; Integrated → #O1
        st = head.power(pt::PowerMode::On);
        if (st == ccam::Status::Ok) {
            vTaskDelay(pdMS_TO_TICKS(900));
            char model[32] = {};
            const ccam::Status qs = pt_.cam().requestModel(model, sizeof(model));
            if (qs == ccam::Status::Ok) {
                char esc[64];
                json_escape(model, esc, sizeof(esc));
                snprintf(buf, sizeof(buf),
                         "{\"ok\":true,\"status\":\"ok\",\"cam_model\":\"%s\"}", esc);
                return send_json(req, buf);
            }
            st = qs;
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
    } else if (strcmp(q.cmd, "preset_save") == 0) {
        st = head.savePreset(preset);
    } else if (strcmp(q.cmd, "preset_delete") == 0) {
        st = head.deletePreset(preset);
    } else if (strcmp(q.cmd, "preset_latest") == 0) {
        uint8_t p = 0;
        st = head.requestLatestPreset(p);
        if (st == ccam::Status::Ok) {
            snprintf(buf, sizeof(buf), "{\"ok\":true,\"status\":\"ok\",\"preset\":%u}", p);
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
        // Каталог из ccam (не HTML). Без шины.
        char json[4096];
        const auto &cat = ccam::devices::camParamCatalog(pt_.camModel());
        int n = snprintf(
            json, sizeof(json),
            "{\"ok\":true,\"status\":\"ok\",\"cam_id\":\"%s\",\"ranges\":{",
            ccam::devices::camModelKey(pt_.camModel()));
        if (n < 0 || static_cast<size_t>(n) >= sizeof(json)) {
            return reply_st(req, ccam::Status::Buffer);
        }
        const int m = ccam::devices::camParamCatalogToJson(
            cat, json + n, sizeof(json) - static_cast<size_t>(n) - 2);
        if (m < 0) {
            return reply_st(req, ccam::Status::Buffer);
        }
        n += m;
        if (static_cast<size_t>(n) + 2 >= sizeof(json)) {
            return reply_st(req, ccam::Status::Buffer);
        }
        json[n++] = '}';
        json[n++] = '}';
        json[n] = '\0';
        return send_json(req, json);
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
        // NDJSON stream: одна строка JSON на поле, сразу после Qxx (UI заполняет на лету).
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

        if (!emit("{\"ok\":true,\"stream\":true,\"status\":\"ok\"}")) {
            httpd_resp_send_chunk(req, nullptr, 0);
            return ESP_FAIL;
        }

        /* Phase1: поля-зависимости UI → Phase2: только открытые/остальные. */
        uint8_t u8 = 0;
        uint16_t u16 = 0;
        ccam::Status qs;
        const auto &cc = cam.capabilities();

        auto emit_osd_wire = [&](const char *key, ccam::catalog::OsdItem item) -> bool {
            char raw[40] = {};
            qs = cam.requestOsd(item, raw, sizeof(raw));
            const char *wire = raw;
            for (const char *p = raw; *p; ++p) {
                if (*p == ':') {
                    wire = p + 1;
                }
            }
            return emit_str(key, qs, (qs == ccam::Status::Ok) ? wire : "");
        };

        if (!emit("{\"phase\":1}")) {
            httpd_resp_send_chunk(req, nullptr, 0);
            return ESP_FAIL;
        }

        int gain_db = 0;
        bool gain_agc = false;
        qs = cam.requestGain(gain_db, gain_agc);
        if (qs == ccam::Status::Ok) {
            if (gain_agc) {
                if (!emit_raw("gain_mode", qs, "\"agc\"") || !emit_raw("gain", qs, "\"agc\"")) {
                    httpd_resp_send_chunk(req, nullptr, 0);
                    return ESP_FAIL;
                }
            } else {
                if (!emit_raw("gain_mode", qs, "\"manual\"") || !emit_i("gain_db", qs, gain_db)) {
                    httpd_resp_send_chunk(req, nullptr, 0);
                    return ESP_FAIL;
                }
            }
        } else if (!emit_str("gain_mode", qs, "")) {
            httpd_resp_send_chunk(req, nullptr, 0);
            return ESP_FAIL;
        }
        gap();

        uint8_t shutter = 0;
        qs = cam.requestShutter(shutter);
        if (!emit_u("shutter", qs, shutter)) {
            httpd_resp_send_chunk(req, nullptr, 0);
            return ESP_FAIL;
        }
        gap();

        ccam::IrisAutoManual iris_am = ccam::IrisAutoManual::Manual;
        qs = cam.requestIrisAutoManual(iris_am);
        if (!emit_u("iris_am", qs, static_cast<unsigned>(iris_am))) {
            httpd_resp_send_chunk(req, nullptr, 0);
            return ESP_FAIL;
        }
        gap();

        uint8_t awc = 0;
        qs = cam.requestAwcMode(awc);
        if (!emit_u("awc", qs, awc)) {
            httpd_resp_send_chunk(req, nullptr, 0);
            return ESP_FAIL;
        }
        gap();
        qs = cam.requestSceneFile(u8);
        if (!emit_u("scene", qs, u8)) {
            httpd_resp_send_chunk(req, nullptr, 0);
            return ESP_FAIL;
        }
        gap();

        if (!emit("{\"phase\":2}")) {
            httpd_resp_send_chunk(req, nullptr, 0);
            return ESP_FAIL;
        }

        /* Зависимые: iris volume / dig_gain / agc_max / atw / R·B Gain */
        if (iris_am == ccam::IrisAutoManual::Manual) {
            qs = cam.requestManualIris(u16);
            if (!emit_u("iris", qs, u16)) {
                httpd_resp_send_chunk(req, nullptr, 0);
                return ESP_FAIL;
            }
            gap();
        }
        if (!gain_agc) {
            qs = cam.requestDigitalGainUp(u8);
            if (!emit_u("dig_gain", qs, u8)) {
                httpd_resp_send_chunk(req, nullptr, 0);
                return ESP_FAIL;
            }
            gap();
        }
        if (gain_agc && cc.agc_max) {
            if (!emit_osd_wire("agc_max", ccam::catalog::OsdItem::AgcMax)) {
                httpd_resp_send_chunk(req, nullptr, 0);
                return ESP_FAIL;
            }
            gap();
        }
        if (awc == 0) {
            if (!emit_osd_wire("atw", ccam::catalog::OsdItem::AtwSpeed)) {
                httpd_resp_send_chunk(req, nullptr, 0);
                return ESP_FAIL;
            }
            gap();
        }
        const bool awc_mem = (awc == 1 || awc == 2 || awc == 3);
        if (cc.paint && awc_mem) {
            qs = cam.requestRGain(u16);
            if (!emit_u("r_gain", qs, u16)) {
                httpd_resp_send_chunk(req, nullptr, 0);
                return ESP_FAIL;
            }
            gap();
            qs = cam.requestBGain(u16);
            if (!emit_u("b_gain", qs, u16)) {
                httpd_resp_send_chunk(req, nullptr, 0);
                return ESP_FAIL;
            }
            gap();
        }
        qs = cam.requestLightPeakAvg(u8);
        if (!emit_u("light_peak", qs, u8)) {
            httpd_resp_send_chunk(req, nullptr, 0);
            return ESP_FAIL;
        }
        gap();
        qs = cam.requestLightArea(u8);
        if (!emit_u("light_area", qs, u8)) {
            httpd_resp_send_chunk(req, nullptr, 0);
            return ESP_FAIL;
        }
        gap();
        {
            uint16_t tped = 0;
            qs = cam.requestTPedestal(tped);
            if (!emit_u("t_ped", qs, tped)) {
                httpd_resp_send_chunk(req, nullptr, 0);
                return ESP_FAIL;
            }
            gap();
        }

        qs = cam.requestDnr3d(u8);
        if (!emit_u("dnr3d", qs, u8)) {
            httpd_resp_send_chunk(req, nullptr, 0);
            return ESP_FAIL;
        }
        gap();
        if (cc.gamma) {
            if (!emit_osd_wire("gamma", ccam::catalog::OsdItem::Gamma)) {
                httpd_resp_send_chunk(req, nullptr, 0);
                return ESP_FAIL;
            }
            gap();
        }
        if (!emit_osd_wire("knee", ccam::catalog::OsdItem::KneePoint)) {
            httpd_resp_send_chunk(req, nullptr, 0);
            return ESP_FAIL;
        }
        gap();
        if (!emit_osd_wire("cdnr", ccam::catalog::OsdItem::CleanDnr)) {
            httpd_resp_send_chunk(req, nullptr, 0);
            return ESP_FAIL;
        }
        gap();
        {
            char wclip[40] = {};
            qs = cam.requestOsd(ccam::catalog::OsdItem::WhiteClip, wclip, sizeof(wclip));
            const char *wire = wclip;
            for (const char *p = wclip; *p; ++p) {
                if (*p == ':') {
                    wire = p + 1;
                }
            }
            unsigned wv = 15;
            if (qs == ccam::Status::Ok && wire && *wire) {
                wv = static_cast<unsigned>(strtoul(wire, nullptr, 16));
            }
            if (!emit_u("wclip", qs, wv)) {
                httpd_resp_send_chunk(req, nullptr, 0);
                return ESP_FAIL;
            }
            gap();
        }
        {
            char air[40] = {};
            qs = cam.requestOsd(ccam::catalog::OsdItem::AirisLevel, air, sizeof(air));
            const char *wire = air;
            for (const char *p = air; *p; ++p) {
                if (*p == ':') {
                    wire = p + 1;
                }
            }
            unsigned av = 50;
            if (qs == ccam::Status::Ok && wire && *wire) {
                av = static_cast<unsigned>(strtoul(wire, nullptr, 16));
            }
            if (!emit_u("airis", qs, av)) {
                httpd_resp_send_chunk(req, nullptr, 0);
                return ESP_FAIL;
            }
            gap();
        }

        char model[24] = {};
        char version[24] = {};
        qs = cam.requestModel(model, sizeof(model));
        if (!emit_str("model", qs, model)) {
            httpd_resp_send_chunk(req, nullptr, 0);
            return ESP_FAIL;
        }
        gap();
        qs = cam.requestSoftwareVersion(version, sizeof(version));
        if (!emit_str("version", qs, version)) {
            httpd_resp_send_chunk(req, nullptr, 0);
            return ESP_FAIL;
        }
        gap();

        qs = cam.requestDetail(u8);
        if (!emit_u("detail", qs, u8)) {
            httpd_resp_send_chunk(req, nullptr, 0);
            return ESP_FAIL;
        }
        gap();

        ccam::ColorBarMode cbar = ccam::ColorBarMode::Camera;
        qs = cam.requestColorBar(cbar);
        if (!emit_u("colorbar", qs, static_cast<unsigned>(cbar))) {
            httpd_resp_send_chunk(req, nullptr, 0);
            return ESP_FAIL;
        }
        gap();
        ccam::Switch sw = ccam::Switch::Off;
        qs = cam.requestMenuOnOff(sw);
        if (!emit_u("menu", qs, static_cast<unsigned>(sw))) {
            httpd_resp_send_chunk(req, nullptr, 0);
            return ESP_FAIL;
        }
        gap();
        qs = cam.requestAutoFocus(sw);
        if (!emit_u("af", qs, static_cast<unsigned>(sw))) {
            httpd_resp_send_chunk(req, nullptr, 0);
            return ESP_FAIL;
        }
        gap();
        qs = cam.requestFilter(u8);
        if (!emit_u("filter", qs, u8)) {
            httpd_resp_send_chunk(req, nullptr, 0);
            return ESP_FAIL;
        }
        gap();

        if (cc.neg_pos) {
            qs = cam.requestNegPos(sw);
            if (!emit_u("negpos", qs, static_cast<unsigned>(sw))) {
                httpd_resp_send_chunk(req, nullptr, 0);
                return ESP_FAIL;
            }
            gap();
        }
        if (cc.field_frame) {
            ccam::FieldFrameMode ff = ccam::FieldFrameMode::Field;
            qs = cam.requestFieldFrame(ff);
            if (!emit_u("fieldframe", qs, static_cast<unsigned>(ff))) {
                httpd_resp_send_chunk(req, nullptr, 0);
                return ESP_FAIL;
            }
            gap();
        }
        if (cc.chroma) {
            qs = cam.requestChromaLevel(u8);
            if (!emit_u("chroma", qs, u8)) {
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
        // OCG: 00=-3 .. 03=0 .. 06=+3 (E650)
        const int n = as_int(q.v);
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
    cfg.max_uri_handlers = 8;
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
    httpd_register_uri_handler(srv_, &root);
    httpd_register_uri_handler(srv_, &status);
    httpd_register_uri_handler(srv_, &ptz);
    httpd_register_uri_handler(srv_, &cam);
    ESP_LOGI(TAG, "HTTP listen :80");
}
