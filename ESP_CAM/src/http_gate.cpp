#include "http_gate.h"

#include "esp_err.h"
#include "http_auth.h"
#include "http_status.h"
#include "ota_busy.h"

namespace {

HttpAuth *auth_of(httpd_req_t *req)
{
    if (!req || !req->user_ctx) {
        return nullptr;
    }
    return &static_cast<HttpStatus *>(req->user_ctx)->auth();
}

bool authorization_present(httpd_req_t *req)
{
    if (!req) {
        return false;
    }
    char hdr[96] = {};
    const esp_err_t rc = httpd_req_get_hdr_value_str(req, "Authorization", hdr, sizeof(hdr));
    return rc == ESP_OK || rc == ESP_ERR_HTTPD_RESULT_TRUNC;
}

/* Только задача httpd. Мьютекс не нужен, пока нет второго писателя. */
RawAccess g_raw = RawAccess::Operator;

} // namespace

Caller http_caller(httpd_req_t *req)
{
    HttpAuth *auth = auth_of(req);
    if (!auth) {
        return Caller::None;
    }
    if (authorization_present(req)) {
        return auth->bearerMatches(req) ? Caller::Service : Caller::None;
    }
    return auth->operatorSession(req, false) ? Caller::Operator : Caller::None;
}

Caller http_presented(httpd_req_t *req)
{
    HttpAuth *auth = auth_of(req);
    if (!auth) {
        return Caller::None;
    }
    if (auth->operatorSession(req, false)) {
        return Caller::Operator;
    }
    if (auth->bearerMatches(req)) {
        return Caller::Service;
    }
    return Caller::None;
}

bool http_operator(httpd_req_t *req)
{
    HttpAuth *auth = auth_of(req);
    return auth && auth->operatorSession(req, false);
}

void http_slide_operator(httpd_req_t *req)
{
    HttpAuth *auth = auth_of(req);
    if (!auth) {
        return;
    }
    (void)auth->operatorSession(req, true);
}

bool http_ui_client(httpd_req_t *req)
{
    HttpAuth *auth = auth_of(req);
    return auth && auth->fromPage(req);
}

bool http_head_cap(const ccam::devices::PtCaps &caps, Cap cap)
{
    switch (cap) {
    case Cap::AbsoluteAim:
        return caps.abs_pt;
    case Cap::AbsoluteAimSpeed:
        return caps.abs_pt_speed;
    case Cap::PresetMap:
        return caps.preset_map;
    case Cap::PresetOptions:
        return caps.preset_opts;
    case Cap::LimitSet:
        return caps.limit_ctrl;
    case Cap::LimitToggle:
        return caps.limit_toggle;
    case Cap::Tally:
        return caps.tally;
    case Cap::Fan:
        return caps.fan;
    case Cap::Wiper:
        return caps.wiper;
    case Cap::Washer:
        return caps.washer;
    case Cap::Wireless:
        return caps.wireless;
    case Cap::Option:
        return caps.option;
    case Cap::TiltSpan:
        return caps.tilt_range;
    case Cap::Heater:
        return caps.heater;
    case Cap::Defroster:
        return caps.defroster;
    case Cap::AimControl:
        return caps.pt_pos;
    case Cap::AimDisplayed:
        return caps.pos_disp;
    case Cap::Optics:
        return caps.lens_q;
    case Cap::ExposureReading:
        return caps.exposure_q;
    case Cap::Landing:
        return caps.landing;
    case Cap::HeadVersion:
        return caps.software_ver;
    case Cap::PowerCameraLink:
        return caps.power_cam_tx;
    default:
        return false;
    }
}

bool http_cam_cap(const ccam::devices::CamCaps &caps, Cap cap)
{
    switch (cap) {
    case Cap::HdDetail:
        return caps.hd_detail;
    case Cap::Negative:
        return caps.neg_pos;
    case Cap::Scan:
        return caps.field_frame;
    case Cap::Chroma:
        return caps.chroma;
    case Cap::BlackShade:
        return caps.black_shade;
    case Cap::Synchro:
        return caps.synchro;
    case Cap::BlackBalance:
        return caps.abc;
    case Cap::Paint:
        return caps.paint;
    case Cap::Genlock:
        return caps.genlock;
    case Cap::Gamma:
        return caps.gamma;
    case Cap::AgcCeiling:
        return caps.agc_max;
    case Cap::MasterDetail:
        return caps.total_dtl;
    case Cap::Monitor:
        return caps.monitor;
    case Cap::LensInterface:
        return caps.lens_h;
    case Cap::MenuLeftRight:
        return caps.menu_lr;
    default:
        return false;
    }
}

bool http_ota_busy()
{
    return otaIsBusy();
}

RawAccess http_raw_access()
{
    return g_raw;
}

void http_set_raw_access(RawAccess access)
{
    g_raw = access;
}

bool http_raw_allowed(Caller caller)
{
    switch (g_raw) {
    case RawAccess::Disabled:
        return false;
    case RawAccess::Operator:
        return caller == Caller::Operator;
    case RawAccess::All:
        return caller == Caller::Operator || caller == Caller::Service;
    }
    return false;
}
