#include "http_auth.h"

#include <cstdio>
#include <cstring>

#include "esp_log.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "nvs.h"
#include "nvs_flash.h"

static const char *TAG = "auth";
static const char *kNs = "auth";
static const char *kUser = "user";
static const char *kPass = "pass";
static const char *kTok = "token";
static const char *kTtl = "ttl";

namespace {

void fillAlnum(char *out, size_t len)
{
    static const char kAbc[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
    uint8_t rnd[64];
    if (len > sizeof(rnd)) {
        len = sizeof(rnd);
    }
    esp_fill_random(rnd, len);
    for (size_t i = 0; i < len; ++i) {
        out[i] = kAbc[rnd[i] % (sizeof(kAbc) - 1)];
    }
    out[len] = '\0';
}

bool constEq(const char *a, const char *b)
{
    if (!a || !b) {
        return false;
    }
    const size_t na = strlen(a);
    const size_t nb = strlen(b);
    if (na != nb) {
        return false;
    }
    unsigned diff = 0;
    for (size_t i = 0; i < na; ++i) {
        diff |= static_cast<unsigned>(static_cast<unsigned char>(a[i]) ^
                                      static_cast<unsigned char>(b[i]));
    }
    return diff == 0;
}

} // namespace

void HttpAuth::mintSession()
{
    fillAlnum(sess_, kSessLen);
    touchSession();
}

void HttpAuth::mintToken()
{
    fillAlnum(api_token_, kTokenLen);
}

void HttpAuth::touchSession()
{
    sess_touch_us_ = esp_timer_get_time();
}

void HttpAuth::clearSession()
{
    sess_[0] = '\0';
    sess_touch_us_ = 0;
}

bool HttpAuth::sessionValid() const
{
    if (sess_[0] == '\0' || sess_touch_us_ <= 0) {
        return false;
    }
    const int64_t age_us = esp_timer_get_time() - sess_touch_us_;
    const int64_t ttl_us = static_cast<int64_t>(sess_ttl_sec_) * 1000000LL;
    return age_us >= 0 && age_us <= ttl_us;
}

bool HttpAuth::sessionActive() const
{
    return sessionValid();
}

uint32_t HttpAuth::sessionRemainSec() const
{
    if (!sessionValid()) {
        return 0;
    }
    const int64_t age_us = esp_timer_get_time() - sess_touch_us_;
    const int64_t ttl_us = static_cast<int64_t>(sess_ttl_sec_) * 1000000LL;
    const int64_t left = ttl_us - age_us;
    if (left <= 0) {
        return 0;
    }
    return static_cast<uint32_t>((left + 999999) / 1000000);
}

void HttpAuth::load()
{
    snprintf(user_, sizeof(user_), "admin");
    snprintf(pass_, sizeof(pass_), "admin");
    api_token_[0] = '\0';
    sess_ttl_sec_ = kSessTtlDefSec;

    nvs_handle_t h{};
    if (nvs_open(kNs, NVS_READONLY, &h) != ESP_OK) {
        return;
    }
    size_t n = sizeof(user_);
    if (nvs_get_str(h, kUser, user_, &n) != ESP_OK || user_[0] == '\0') {
        snprintf(user_, sizeof(user_), "admin");
    }
    n = sizeof(pass_);
    if (nvs_get_str(h, kPass, pass_, &n) != ESP_OK || pass_[0] == '\0') {
        snprintf(pass_, sizeof(pass_), "admin");
    }
    n = sizeof(api_token_);
    if (nvs_get_str(h, kTok, api_token_, &n) != ESP_OK) {
        api_token_[0] = '\0';
    }
    uint32_t ttl = 0;
    if (nvs_get_u32(h, kTtl, &ttl) == ESP_OK) {
        if (ttl < kSessTtlMinSec) {
            ttl = kSessTtlMinSec;
        }
        if (ttl > kSessTtlMaxSec) {
            ttl = kSessTtlMaxSec;
        }
        sess_ttl_sec_ = ttl;
    }
    nvs_close(h);
}

void HttpAuth::save() const
{
    nvs_handle_t h{};
    if (nvs_open(kNs, NVS_READWRITE, &h) != ESP_OK) {
        ESP_LOGW(TAG, "nvs open fail");
        return;
    }
    (void)nvs_set_str(h, kUser, user_);
    (void)nvs_set_str(h, kPass, pass_);
    (void)nvs_set_str(h, kTok, api_token_);
    (void)nvs_set_u32(h, kTtl, sess_ttl_sec_);
    (void)nvs_commit(h);
    nvs_close(h);
}

void HttpAuth::begin()
{
    load();
    if (api_token_[0] == '\0') {
        mintToken();
        save();
        ESP_LOGI(TAG, "API token minted");
    }
    clearSession();
    ESP_LOGI(TAG, "auth ready user=%s ttl=%us", user_, static_cast<unsigned>(sess_ttl_sec_));
}

bool HttpAuth::authorized(httpd_req_t *req, bool slide)
{
    if (!req) {
        return false;
    }
    char hdr[96] = {};
    if (httpd_req_get_hdr_value_str(req, "Authorization", hdr, sizeof(hdr)) == ESP_OK) {
        if (strncmp(hdr, "Bearer ", 7) == 0 && api_token_[0] != '\0' &&
            constEq(hdr + 7, api_token_)) {
            return true; /* Bearer без idle */
        }
    }
    if (!sessionValid()) {
        if (sess_[0] != '\0') {
            clearSession();
        }
        return false;
    }
    char cookie[kSessLen + 4] = {};
    size_t sz = sizeof(cookie);
    if (httpd_req_get_cookie_val(req, "esp_sess", cookie, &sz) != ESP_OK) {
        return false;
    }
    if (!constEq(cookie, sess_)) {
        return false;
    }
    if (slide) {
        touchSession(); /* idle только при реальном действии */
    }
    return true;
}

bool HttpAuth::login(const char *user, const char *pass, char *sess_out, size_t sess_cap)
{
    if (!user || !pass || user[0] == '\0' || pass[0] == '\0') {
        return false;
    }
    if (!constEq(user, user_) || !constEq(pass, pass_)) {
        return false;
    }
    mintSession();
    if (sess_out && sess_cap > 0) {
        snprintf(sess_out, sess_cap, "%s", sess_);
    }
    return true;
}

void HttpAuth::logout()
{
    clearSession();
}

bool HttpAuth::setPassword(const char *old_pass, const char *new_pass)
{
    if (!old_pass || !new_pass || new_pass[0] == '\0') {
        return false;
    }
    if (strlen(new_pass) >= kPassMax) {
        return false;
    }
    if (!constEq(old_pass, pass_)) {
        return false;
    }
    snprintf(pass_, sizeof(pass_), "%s", new_pass);
    save();
    return true;
}

void HttpAuth::reissueToken()
{
    mintToken();
    save();
}

bool HttpAuth::setSessionTtlSec(uint32_t sec)
{
    if (sec < kSessTtlMinSec || sec > kSessTtlMaxSec) {
        return false;
    }
    sess_ttl_sec_ = sec;
    save();
    return true;
}
