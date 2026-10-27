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
    if (n >= 1 && n <= 49) {
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
            n = snprintf(out + o, cap - o, "\\x%02X", b);
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

esp_err_t reply_st(httpd_req_t *req, ccam::Status st, const char *extra = nullptr) {
    char buf[384];
    if (extra && extra[0]) {
        snprintf(buf, sizeof(buf), "{\"ok\":%s,\"status\":\"%s\",\"data\":\"%s\"}",
                 st == ccam::Status::Ok ? "true" : "false", Ph350Ctrl::statusText(st), extra);
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
    char buf[512];
    if (extra && extra[0]) {
        snprintf(buf, sizeof(buf),
                 "{\"ok\":%s,\"status\":\"%s\",\"data\":\"%s\",\"tx\":\"%s\",\"rx\":\"%s\"}",
                 st == ccam::Status::Ok ? "true" : "false", Ph350Ctrl::statusText(st), extra, tx,
                 rx);
    } else {
        snprintf(buf, sizeof(buf), "{\"ok\":%s,\"status\":\"%s\",\"tx\":\"%s\",\"rx\":\"%s\"}",
                 st == ccam::Status::Ok ? "true" : "false", Ph350Ctrl::statusText(st), tx, rx);
    }
    return send_json(req, buf);
}

} // namespace

static const char kPage[] =
#include "status.html.inc"
;

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

    char buf[384];
    snprintf(buf, sizeof(buf),
             "{\"link\":%s,\"ip\":\"%s\",\"mask\":\"%s\",\"gw\":\"%s\",\"mac\":\"%s\","
             "\"uptime_s\":%lu,\"model\":\"%s\",\"cam\":\"%s\",\"rate\":%u}",
             eth_.link_up() ? "true" : "false", ip, mask, gw, mac,
             static_cast<unsigned long>(uptime_s), pt_.pt().modelLabel(), pt_.cam().modelLabel(),
             static_cast<unsigned>(pt_.defaultRate()));
    return send_json(req, buf);
}

esp_err_t HttpStatus::on_ptz(httpd_req_t *req) {
    Q q;
    load_q(req, q);
    if (!q.cmd[0]) {
        return reply_st(req, ccam::Status::Param);
    }

    uint8_t rate = clamp_rate(as_int(q.rate, -1), pt_.defaultRate());
    if (q.rate[0]) {
        pt_.setDefaultRate(rate);
    }
    uint8_t preset = 1;
    {
        const int n = as_int(q.preset, 1);
        if (n >= 1 && n <= 99) {
            preset = static_cast<uint8_t>(n);
        }
    }

    auto &head = pt_.pt();
    ccam::Status st = ccam::Status::Param;
    char extra[96] = {};
    char buf[384];

    const int8_t spd = static_cast<int8_t>(rate);
    const int8_t nspd = static_cast<int8_t>(-rate);

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
    } else if (strcmp(q.cmd, "power_off") == 0) {
        st = head.power(pt::PowerMode::Off);
    } else if (strcmp(q.cmd, "power_on") == 0) {
        st = head.power(pt::PowerMode::On);
    } else if (strcmp(q.cmd, "power_on_cam") == 0) {
        st = head.power(pt::PowerMode::OnWithCameraTx);
    } else if (strcmp(q.cmd, "power_on_nocam") == 0) {
        st = head.power(pt::PowerMode::OnWithoutCameraTx);
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
    } else if (strcmp(q.cmd, "set_pt") == 0) {
        pt::PT p{as_u16(q.v, ccam::kPtPosCenter), as_u16(q.v2, ccam::kPtPosCenter)};
        st = head.setPT(p);
    } else if (strcmp(q.cmd, "rel_pt") == 0) {
        st = head.relativePosition(as_u16(q.v), as_u16(q.v2));
    } else if (strcmp(q.cmd, "set_lens") == 0) {
        pt::Lens lens{as_u16(q.v3), as_u16(q.data), as_u16(q.v4)};
        st = head.setLens(lens);
    } else if (strcmp(q.cmd, "set_zoom") == 0) {
        st = head.setZoom(as_u16(q.v));
    } else if (strcmp(q.cmd, "set_focus") == 0) {
        st = head.setFocus(as_u16(q.v));
    } else if (strcmp(q.cmd, "set_iris") == 0) {
        st = head.setIris(as_u16(q.v));
    } else if (strcmp(q.cmd, "req_zoom") == 0) {
        uint16_t v = 0;
        st = head.requestZoom(v);
        if (st == ccam::Status::Ok) {
            snprintf(buf, sizeof(buf), "{\"ok\":true,\"status\":\"ok\",\"zoom\":%u}", v);
            return send_json(req, buf);
        }
    } else if (strcmp(q.cmd, "req_focus") == 0) {
        uint16_t v = 0;
        st = head.requestFocus(v);
        if (st == ccam::Status::Ok) {
            snprintf(buf, sizeof(buf), "{\"ok\":true,\"status\":\"ok\",\"focus\":%u}", v);
            return send_json(req, buf);
        }
    } else if (strcmp(q.cmd, "req_iris") == 0) {
        uint16_t v = 0;
        st = head.requestIris(v);
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
    } else if (strcmp(q.cmd, "set_pt_w_spd") == 0 || strcmp(q.cmd, "rel_pt_w_spd") == 0) {
        const uint8_t ps = static_cast<uint8_t>(as_int(q.v3, 15));
        const int tab = as_int(q.v4);
        if (tab >= 0 && tab <= 2) {
            const auto table = static_cast<ccam::PrSpeed>(tab);
            st = (strcmp(q.cmd, "set_pt_w_spd") == 0)
                     ? head.setPTwSpeed(as_u16(q.v, ccam::kPtPosCenter), as_u16(q.v2, ccam::kPtPosCenter),
                                        ps, table)
                     : head.relPTwSpeed(as_u16(q.v), as_u16(q.v2), ps, table);
        }
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
    } else if (strcmp(q.cmd, "limitation") == 0) {
        st = head.setLimitation(static_cast<ccam::Limitation>(as_int(q.v) & 1));
    } else if (strcmp(q.cmd, "limit_ctrl") == 0) {
        const int ax = as_int(q.v);
        if (ax >= 1 && ax <= 4) {
            st = head.setLimitationControl(static_cast<ccam::LimitAxis>(ax),
                                           static_cast<ccam::Limitation>(as_int(q.v2) & 1));
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
    Q q;
    load_q(req, q);
    if (!q.cmd[0]) {
        return reply_cam(req, pt_, ccam::Status::Param);
    }

    auto &cam = pt_.cam();
    ccam::Status st = ccam::Status::Param;
    char extra[96] = {};

    using ccam::devices::E650AwcMode;
    using ccam::devices::E650Scene;
    using ccam::devices::E650ShutterMode;

    if (strcmp(q.cmd, "model") == 0) {
        st = cam.requestModel(extra, sizeof(extra));
        return reply_cam(req, pt_, st, extra);
    }
    if (strcmp(q.cmd, "version") == 0) {
        st = cam.requestSoftwareVersion(extra, sizeof(extra));
        return reply_cam(req, pt_, st, extra);
    }
    if (strcmp(q.cmd, "awc_start") == 0) {
        st = cam.awcStart();
    } else if (strcmp(q.cmd, "abc_start") == 0) {
        st = cam.abcBalanceStart();
    } else if (strcmp(q.cmd, "awc_mode") == 0) {
        const int n = as_int(q.v);
        if (n >= 0 && n <= 9) {
            st = cam.setAwcMode(static_cast<E650AwcMode>(n));
        }
    } else if (strcmp(q.cmd, "scene") == 0) {
        const int n = as_int(q.v);
        if (n >= 0 && n <= 3) {
            st = cam.selectScene(static_cast<E650Scene>(n));
        }
    } else if (strcmp(q.cmd, "shutter_mode") == 0) {
        st = cam.setShutterMode(static_cast<E650ShutterMode>(as_int(q.v)));
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
        st = cam.setColorBar(static_cast<ccam::Switch>(as_int(q.v) & 1));
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
    } else if (strcmp(q.cmd, "af") == 0) {
        st = cam.setAutoFocus(static_cast<ccam::Switch>(as_int(q.v) & 1));
    } else if (strcmp(q.cmd, "negpos") == 0) {
        st = cam.setNegPos(as_int(q.v) != 0);
    } else if (strcmp(q.cmd, "fieldframe") == 0) {
        st = cam.setFieldFrame(static_cast<ccam::FieldFrameMode>(as_int(q.v) & 1));
    } else if (strcmp(q.cmd, "chroma") == 0) {
        const int n = as_int(q.v);
        if (n >= 0 && n <= 9) {
            st = cam.setChromaLevel(static_cast<ccam::Levels>(n));
        }
    } else if (strcmp(q.cmd, "black_shade") == 0) {
        st = cam.setBlackShadingCorrect(as_int(q.v) != 0);
    } else if (strcmp(q.cmd, "r_gain") == 0) {
        st = cam.setRGain(q.data[0] ? q.data : "80");
    } else if (strcmp(q.cmd, "b_gain") == 0) {
        st = cam.setBGain(q.data[0] ? q.data : "80");
    } else if (strcmp(q.cmd, "r_ped") == 0) {
        st = cam.setRPedestal(q.data[0] ? q.data : "80");
    } else if (strcmp(q.cmd, "b_ped") == 0) {
        st = cam.setBPedestal(q.data[0] ? q.data : "80");
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
        st = static_cast<ccam::devices::CameraDeviceBase &>(cam).selectScene(
            q.data[0] ? q.data : "0");
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
