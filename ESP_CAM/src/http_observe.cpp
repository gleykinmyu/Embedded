#include "http_observe.h"

#include <cstdio>
#include <cstring>

#include "esp_timer.h"
#include "eth_lan8720.h"
#include "http_gate.h"
#include "http_reply.h"
#include "http_status.h"
#include "ph350_ctrl.h"
#include "protocol/pt_types.hpp"

namespace {

struct Buf {
    char *data;
    size_t cap;
    size_t len{0};
    bool ok{true};

    void add(const char *text)
    {
        if (!ok || text == nullptr) {
            ok = false;
            return;
        }
        const size_t n = strlen(text);
        if (len + n >= cap) {
            ok = false;
            return;
        }
        memcpy(data + len, text, n);
        len += n;
        data[len] = '\0';
    }

    void addf(int64_t value)
    {
        char tmp[32];
        const int n = snprintf(tmp, sizeof(tmp), "%lld", static_cast<long long>(value));
        if (n <= 0 || static_cast<size_t>(n) >= sizeof(tmp)) {
            ok = false;
            return;
        }
        add(tmp);
    }
};

void add_esc(Buf &buf, const char *text)
{
    char out[160];
    size_t o = 0;
    if (text == nullptr) {
        text = "";
    }
    for (size_t i = 0; text[i] != '\0'; ++i) {
        const unsigned char c = static_cast<unsigned char>(text[i]);
        const char *extra = nullptr;
        char one[7];
        if (c == '"' || c == '\\') {
            one[0] = '\\';
            one[1] = static_cast<char>(c);
            one[2] = '\0';
            extra = one;
        } else if (c < 0x20) {
            snprintf(one, sizeof(one), "\\u%04X", c);
            extra = one;
        }
        const char *src = extra != nullptr ? extra : reinterpret_cast<const char *>(&text[i]);
        const size_t n = extra != nullptr ? strlen(extra) : 1;
        if (o + n >= sizeof(out)) {
            buf.ok = false;
            return;
        }
        memcpy(out + o, src, n);
        o += n;
        if (extra == nullptr) {
            continue;
        }
    }
    out[o] = '\0';
    buf.add("\"");
    buf.add(out);
    buf.add("\"");
}

void add_bool(Buf &buf, bool value)
{
    buf.add(value ? "true" : "false");
}

bool known_model(const char *key, const char *const *table, size_t n)
{
    if (key == nullptr) {
        return false;
    }
    for (size_t i = 0; i < n; ++i) {
        if (strcmp(key, table[i]) == 0) {
            return true;
        }
    }
    return false;
}

const char *power_state(uint8_t code)
{
    switch (code) {
    case 0:
        return "off";
    case 1:
        return "on";
    case 2:
        return "on_camera_tx";
    case 3:
        return "on_no_camera_tx";
    case 14:
        return "starting";
    case 15:
        return "control_off";
    default:
        return "unknown";
    }
}

const char *limit_name(Ph350Ctrl::LimitState state)
{
    switch (state) {
    case Ph350Ctrl::LimitState::Release:
        return "clear";
    case Ph350Ctrl::LimitState::Set:
        return "set";
    default:
        return "unknown";
    }
}

bool slide_requested(httpd_req_t *req)
{
    char act[8] = {};
    if (httpd_req_get_hdr_value_str(req, "X-Esp-Active", act, sizeof(act)) != ESP_OK) {
        return false;
    }
    return strcmp(act, "1") == 0;
}

void add_head_caps(Buf &buf, const ccam::devices::PtCaps &c)
{
    buf.add("\"capabilities\":{\"absolute_aim\":");
    add_bool(buf, c.abs_pt);
    buf.add(",\"absolute_aim_speed\":");
    add_bool(buf, c.abs_pt_speed);
    buf.add(",\"preset_map\":");
    add_bool(buf, c.preset_map);
    buf.add(",\"preset_options\":");
    add_bool(buf, c.preset_opts);
    buf.add(",\"limit_set\":");
    add_bool(buf, c.limit_ctrl);
    buf.add(",\"limit_toggle\":");
    add_bool(buf, c.limit_toggle);
    buf.add(",\"tally\":");
    add_bool(buf, c.tally);
    buf.add(",\"fan\":");
    add_bool(buf, c.fan);
    buf.add(",\"wiper\":");
    add_bool(buf, c.wiper);
    buf.add(",\"washer\":");
    add_bool(buf, c.washer);
    buf.add(",\"wireless\":");
    add_bool(buf, c.wireless);
    buf.add(",\"option\":");
    add_bool(buf, c.option);
    buf.add(",\"tilt_span\":");
    add_bool(buf, c.tilt_range);
    buf.add(",\"heater\":");
    add_bool(buf, c.heater);
    buf.add(",\"defroster\":");
    add_bool(buf, c.defroster);
    buf.add(",\"aim_control\":");
    add_bool(buf, c.pt_pos);
    buf.add(",\"aim_displayed\":");
    add_bool(buf, c.pos_disp);
    buf.add(",\"optics\":");
    add_bool(buf, c.lens_q);
    buf.add(",\"exposure_reading\":");
    add_bool(buf, c.exposure_q);
    buf.add(",\"landing\":");
    add_bool(buf, c.landing);
    buf.add(",\"head_version\":");
    add_bool(buf, c.software_ver);
    buf.add(",\"power_camera_link\":");
    add_bool(buf, c.power_cam_tx);
    buf.add("}");
}

void add_cam_caps(Buf &buf, const ccam::devices::CamCaps &c)
{
    buf.add("\"capabilities\":{\"hd_detail\":");
    add_bool(buf, c.hd_detail);
    buf.add(",\"negative\":");
    add_bool(buf, c.neg_pos);
    buf.add(",\"scan\":");
    add_bool(buf, c.field_frame);
    buf.add(",\"chroma\":");
    add_bool(buf, c.chroma);
    buf.add(",\"black_shade\":");
    add_bool(buf, c.black_shade);
    buf.add(",\"synchro\":");
    add_bool(buf, c.synchro);
    buf.add(",\"black_balance\":");
    add_bool(buf, c.abc);
    buf.add(",\"paint\":");
    add_bool(buf, c.paint);
    buf.add(",\"genlock\":");
    add_bool(buf, c.genlock);
    buf.add(",\"gamma\":");
    add_bool(buf, c.gamma);
    buf.add(",\"agc_ceiling\":");
    add_bool(buf, c.agc_max);
    buf.add(",\"master_detail\":");
    add_bool(buf, c.total_dtl);
    buf.add(",\"monitor\":");
    add_bool(buf, c.monitor);
    buf.add(",\"lens_interface\":");
    add_bool(buf, c.lens_h);
    buf.add(",\"menu_left_right\":");
    add_bool(buf, c.menu_lr);
    buf.add("}");
}

} // namespace

esp_err_t http_get_status(httpd_req_t *req)
{
    HttpStatus *self = req && req->user_ctx ? static_cast<HttpStatus *>(req->user_ctx) : nullptr;
    if (self == nullptr) {
        return http_send_error(req, HttpError::Internal, "internal");
    }
    const Caller caller = http_caller(req);
    if (caller == Caller::None) {
        return http_send_error(req, HttpError::Unauthorized, "unauthorized");
    }

    Ph350Ctrl &pt = self->pt();
    Ph350Ctrl::HealthView health{};
    pt.copyHealth(health);
    EthLan8720::Live net{};
    self->eth().copyLive(net);

    const char *head_key = ccam::devices::ptModelKey(pt.ptModel());
    const char *cam_key = ccam::devices::camModelKey(pt.camModel());
    static const char *kHeads[] = {
        "ph350", "ph360", "ph650", "ph405", "he130", "he870", "ue150", "iptz",
    };
    static const char *kCams[] = {"e650", "e600", "he130", "he870", "ue150"};
    const uint8_t rate = pt.defaultRate();
    if (!known_model(head_key, kHeads, sizeof(kHeads) / sizeof(kHeads[0])) ||
        !known_model(cam_key, kCams, sizeof(kCams) / sizeof(kCams[0])) || rate < 1 || rate > 100) {
        return http_send_error(req, HttpError::Internal, "internal");
    }

    const ccam::devices::PtCaps head_caps = pt.ptCaps();
    const ccam::devices::CamCaps cam_caps = pt.camCaps();
    const pt::AxisInvert invert = pt.axisInvert();
    const uint8_t latest = pt.latestPreset();
    const bool active = pt.atLatest();
    const char *head_name = pt.pt().modelLabel();
    const char *cam_name = pt.cam().modelLabel();
    const int64_t uptime = esp_timer_get_time() / 1000000LL;
    const uint32_t ttl = self->auth().sessionTtlSec();
    const bool service = caller == Caller::Service;

    char storage[3072];
    Buf buf{storage, sizeof(storage)};
    buf.add("{\"network\":{\"link\":");
    add_bool(buf, net.link);
    buf.add(",\"address\":");
    add_esc(buf, net.address);
    buf.add(",\"mask\":");
    add_esc(buf, net.mask);
    buf.add(",\"gateway\":");
    add_esc(buf, net.gateway);
    buf.add(",\"mac\":");
    add_esc(buf, net.mac);
    buf.add("},\"uptime_seconds\":");
    buf.addf( uptime < 0 ? 0 : uptime);
    buf.add(",\"head\":{\"model\":");
    add_esc(buf, head_key);
    buf.add(",\"name\":");
    add_esc(buf, head_name);
    buf.add(",\"alive\":");
    add_bool(buf, health.alive);
    buf.add(",\"power\":{\"state\":");
    add_esc(buf, power_state(health.power_code));
    buf.add(",\"label\":");
    add_esc(buf, ccam::powerStatusLabel(health.power_code));
    buf.add(",\"code\":");
    if (health.power_code == 0xFF) {
        buf.add("null");
    } else {
        buf.addf( health.power_code);
    }
    buf.add("},\"powered\":");
    add_bool(buf, health.powered);
    buf.add(",\"optics_alive\":");
    add_bool(buf, health.optics_alive);
    buf.add(",\"optics_fail_count\":");
    buf.addf( health.optics_fail);
    buf.add(",\"fail_count\":");
    buf.addf( health.fail_count);
    buf.add(",\"error_code\":null,");
    add_head_caps(buf, head_caps);
    buf.add("},\"camera\":{\"model\":");
    add_esc(buf, cam_key);
    buf.add(",\"name\":");
    add_esc(buf, cam_name);
    buf.add(",\"transmitter\":");
    add_bool(buf, health.cam_tx);
    buf.add(",\"alive\":");
    add_bool(buf, health.cam_alive);
    buf.add(",\"identifier\":");
    add_esc(buf, health.cam_id);
    buf.add(",\"fail_count\":");
    buf.addf( health.cam_fail);
    buf.add(",");
    add_cam_caps(buf, cam_caps);
    buf.add("},\"motion\":{\"rate\":");
    buf.addf( rate);
    buf.add(",\"orientation\":{\"pan\":");
    add_bool(buf, invert.pan);
    buf.add(",\"tilt\":");
    add_bool(buf, invert.tilt);
    buf.add(",\"zoom\":");
    add_bool(buf, invert.zoom);
    buf.add(",\"focus\":");
    add_bool(buf, invert.focus);
    buf.add("}},\"presets\":{\"latest\":");
    if (latest >= 1 && latest <= 99) {
        buf.addf( latest);
    } else {
        buf.add("null");
    }
    buf.add(",\"active\":");
    add_bool(buf, active);
    buf.add("},\"limits\":{\"tilt_up\":");
    add_esc(buf, limit_name(pt.limitState(ccam::LimitAxis::TiltUp)));
    buf.add(",\"tilt_down\":");
    add_esc(buf, limit_name(pt.limitState(ccam::LimitAxis::TiltDown)));
    buf.add(",\"pan_left\":");
    add_esc(buf, limit_name(pt.limitState(ccam::LimitAxis::PanLeft)));
    buf.add(",\"pan_right\":");
    add_esc(buf, limit_name(pt.limitState(ccam::LimitAxis::PanRight)));
    buf.add("},\"session\":{\"ttl_seconds\":");
    buf.addf( ttl);
    buf.add(",\"seconds_left\":");
    /* Хвост — null или число и две скобки. Место проверяется до продления. */
    if (!buf.ok || buf.len + 24 >= buf.cap) {
        return http_send_error(req, HttpError::Buffer, "response buffer");
    }
    if (!service && slide_requested(req)) {
        http_slide_operator(req);
    }
    if (service) {
        buf.add("null");
    } else {
        buf.addf( self->auth().sessionRemainSec());
    }
    buf.add("}}");
    if (!buf.ok) {
        return http_send_error(req, HttpError::Buffer, "response buffer");
    }
    return http_send_ok(req, storage);
}
