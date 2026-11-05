#include "http_auth.h"

#include <cstdio>
#include <cstring>

#include "esp_log.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "mbedtls/sha256.h"
#include "nvs.h"
#include "nvs_flash.h"

static const char *TAG = "auth";
static const char *kNs = "auth";
static const char *kUser = "user";
static const char *kPass = "pass"; /* старый открытый пароль, после миграции стирается */
static const char *kSalt = "salt";
static const char *kHash = "ph";
static const char *kTok = "token";
static const char *kTtl = "ttl";
static constexpr int64_t kNonceTtlUs = 60LL * 1000000LL;

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

int hexVal(char c)
{
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}

bool hexToRaw(const char *hex, uint8_t *out, size_t out_len)
{
    if (!hex || !out || strlen(hex) != out_len * 2) {
        return false;
    }
    for (size_t i = 0; i < out_len; ++i) {
        const int hi = hexVal(hex[i * 2]);
        const int lo = hexVal(hex[i * 2 + 1]);
        if (hi < 0 || lo < 0) {
            return false;
        }
        out[i] = static_cast<uint8_t>((hi << 4) | lo);
    }
    return true;
}

void rawToHex(const uint8_t *in, size_t n, char *out, size_t cap)
{
    static const char kHex[] = "0123456789abcdef";
    if (!out || cap < n * 2 + 1) {
        if (out && cap > 0) {
            out[0] = '\0';
        }
        return;
    }
    for (size_t i = 0; i < n; ++i) {
        out[i * 2] = kHex[in[i] >> 4];
        out[i * 2 + 1] = kHex[in[i] & 0x0F];
    }
    out[n * 2] = '\0';
}

bool sha256Cat(const uint8_t *a, size_t na, const uint8_t *b, size_t nb, uint8_t out[32])
{
    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);
    int rc = mbedtls_sha256_starts(&ctx, 0);
    if (rc == 0 && na > 0) {
        rc = mbedtls_sha256_update(&ctx, a, na);
    }
    if (rc == 0 && nb > 0) {
        rc = mbedtls_sha256_update(&ctx, b, nb);
    }
    if (rc == 0) {
        rc = mbedtls_sha256_finish(&ctx, out);
    }
    mbedtls_sha256_free(&ctx);
    return rc == 0;
}

bool rawEq(const uint8_t *a, const uint8_t *b, size_t n)
{
    unsigned diff = 0;
    for (size_t i = 0; i < n; ++i) {
        diff |= static_cast<unsigned>(a[i] ^ b[i]);
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
    sess_touch_us_.store(esp_timer_get_time(), std::memory_order_release);
}

void HttpAuth::clearSession()
{
    sess_[0] = '\0';
    sess_touch_us_.store(0, std::memory_order_release);
}

bool HttpAuth::sessionValid() const
{
    if (sess_[0] == '\0') {
        return false;
    }
    const int64_t touch = sess_touch_us_.load(std::memory_order_acquire);
    if (touch <= 0) {
        return false;
    }
    const int64_t age_us = esp_timer_get_time() - touch;
    const int64_t ttl_us = static_cast<int64_t>(sess_ttl_sec_) * 1000000LL;
    /* Отрицательный age — скачок таймера, сессию не рвём. */
    if (age_us < 0) {
        return true;
    }
    return age_us <= ttl_us;
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
    const int64_t touch = sess_touch_us_.load(std::memory_order_acquire);
    const int64_t age_us = esp_timer_get_time() - touch;
    const int64_t ttl_us = static_cast<int64_t>(sess_ttl_sec_) * 1000000LL;
    if (age_us < 0) {
        return sess_ttl_sec_;
    }
    const int64_t left = ttl_us - age_us;
    if (left <= 0) {
        return 0;
    }
    return static_cast<uint32_t>((left + 999999) / 1000000);
}

void HttpAuth::clearNonce()
{
    nonce_arm_ = false;
    nonce_us_ = 0;
    memset(nonce_, 0, sizeof(nonce_));
    memset(newsalt_, 0, sizeof(newsalt_));
}

bool HttpAuth::nonceFresh() const
{
    if (!nonce_arm_) {
        return false;
    }
    const int64_t age = esp_timer_get_time() - nonce_us_;
    if (age < 0) {
        return true;
    }
    return age <= kNonceTtlUs;
}

void HttpAuth::sealPlain(const char *pass)
{
    if (!pass || pass[0] == '\0' || strlen(pass) >= kPassMax) {
        return;
    }
    esp_fill_random(salt_, sizeof(salt_));
    hash_ok_ = sha256Cat(salt_, sizeof(salt_), reinterpret_cast<const uint8_t *>(pass),
                          strlen(pass), hash_);
    if (hash_ok_) {
        save();
    }
}

bool HttpAuth::consumeProof(const char *proof_hex)
{
    uint8_t got[kHashLen] = {};
    uint8_t expect[kHashLen] = {};
    const bool fresh = hash_ok_ && nonceFresh();
    const bool dec = fresh && hexToRaw(proof_hex, got, sizeof(got));
    const bool hashed = dec && sha256Cat(nonce_, sizeof(nonce_), hash_, sizeof(hash_), expect);
    clearNonce();
    if (!hashed) {
        return false;
    }
    return rawEq(got, expect, sizeof(got));
}

void HttpAuth::load()
{
    snprintf(user_, sizeof(user_), "admin");
    api_token_[0] = '\0';
    sess_ttl_sec_ = kSessTtlDefSec;
    hash_ok_ = false;
    memset(salt_, 0, sizeof(salt_));
    memset(hash_, 0, sizeof(hash_));

    nvs_handle_t h{};
    if (nvs_open(kNs, NVS_READONLY, &h) != ESP_OK) {
        return;
    }
    size_t n = sizeof(user_);
    if (nvs_get_str(h, kUser, user_, &n) != ESP_OK || user_[0] == '\0') {
        snprintf(user_, sizeof(user_), "admin");
    }
    n = sizeof(salt_);
    const bool salt_ok = nvs_get_blob(h, kSalt, salt_, &n) == ESP_OK && n == sizeof(salt_);
    n = sizeof(hash_);
    const bool hash_ok = nvs_get_blob(h, kHash, hash_, &n) == ESP_OK && n == sizeof(hash_);
    hash_ok_ = salt_ok && hash_ok;
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
    if (hash_ok_) {
        (void)nvs_set_blob(h, kSalt, salt_, sizeof(salt_));
        (void)nvs_set_blob(h, kHash, hash_, sizeof(hash_));
    }
    (void)nvs_erase_key(h, kPass);
    (void)nvs_set_str(h, kTok, api_token_);
    (void)nvs_set_u32(h, kTtl, sess_ttl_sec_);
    (void)nvs_commit(h);
    nvs_close(h);
}

void HttpAuth::begin()
{
    if (mu_ == nullptr) {
        mu_ = xSemaphoreCreateMutex();
    }
    load();
    if (!hash_ok_) {
        char plain[kPassMax] = {};
        snprintf(plain, sizeof(plain), "admin");
        nvs_handle_t h{};
        if (nvs_open(kNs, NVS_READONLY, &h) == ESP_OK) {
            size_t n = sizeof(plain);
            if (nvs_get_str(h, kPass, plain, &n) != ESP_OK || plain[0] == '\0') {
                snprintf(plain, sizeof(plain), "admin");
            }
            nvs_close(h);
        }
        sealPlain(plain);
        memset(plain, 0, sizeof(plain));
        if (hash_ok_) {
            ESP_LOGI(TAG, "password sealed");
        }
    } else {
        /* Хеш уже есть — старый ключ pass больше не нужен. */
        nvs_handle_t h{};
        if (nvs_open(kNs, NVS_READWRITE, &h) == ESP_OK) {
            if (nvs_erase_key(h, kPass) == ESP_OK) {
                (void)nvs_commit(h);
            }
            nvs_close(h);
        }
    }
    if (api_token_[0] == '\0') {
        mintToken();
        save();
        ESP_LOGI(TAG, "API token minted");
    }
    clearSession();
    clearNonce();
    ESP_LOGI(TAG, "auth ready user=%s ttl=%us", user_, static_cast<unsigned>(sess_ttl_sec_));
}

bool HttpAuth::fromPage(httpd_req_t *req) const
{
    if (!req) {
        return false;
    }
    char mark[8] = {};
    if (httpd_req_get_hdr_value_str(req, "X-Esp-Client", mark, sizeof(mark)) != ESP_OK) {
        return false;
    }
    return strcmp(mark, "ui") == 0;
}

bool HttpAuth::cookieEquals(httpd_req_t *req) const
{
    if (!req || sess_[0] == '\0') {
        return false;
    }
    char cookie[kSessLen + 8] = {};
    size_t sz = sizeof(cookie);
    if (httpd_req_get_cookie_val(req, "esp_sess", cookie, &sz) != ESP_OK) {
        return false;
    }
    return constEq(cookie, sess_);
}

bool HttpAuth::cookieMatches(httpd_req_t *req) const
{
    return sessionValid() && cookieEquals(req);
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
            return true; /* Bearer без idle и без заголовка страницы */
        }
    }
    /* Адресная строка и чужой хост cookie не предъявляют как страницу. */
    if (!fromPage(req)) {
        return false;
    }
    if (mu_ != nullptr) {
        xSemaphoreTake(mu_, portMAX_DELAY);
    }
    bool ok = false;
    if (!sessionValid()) {
        const int64_t touch = sess_touch_us_.load(std::memory_order_acquire);
        if (sess_[0] != '\0' && touch > 0) {
            const int64_t age_s = (esp_timer_get_time() - touch) / 1000000LL;
            ESP_LOGW(TAG, "session end age=%llds ttl=%us",
                     static_cast<long long>(age_s),
                     static_cast<unsigned>(sess_ttl_sec_));
        }
        /* Не clearSession здесь — гонка с новым login/touch. */
    } else if (!cookieEquals(req)) {
        ESP_LOGW(TAG, "session cookie reject, left=%us",
                 static_cast<unsigned>(sessionRemainSec()));
    } else {
        if (slide) {
            touchSession(); /* idle только при реальном действии */
        }
        ok = true;
    }
    if (mu_ != nullptr) {
        xSemaphoreGive(mu_);
    }
    return ok;
}

bool HttpAuth::challenge(char *salt_hex, size_t salt_cap, char *nonce_hex, size_t nonce_cap,
                         char *newsalt_hex, size_t newsalt_cap)
{
    if (!hash_ok_ || !salt_hex || !nonce_hex || !newsalt_hex) {
        return false;
    }
    esp_fill_random(nonce_, sizeof(nonce_));
    esp_fill_random(newsalt_, sizeof(newsalt_));
    nonce_us_ = esp_timer_get_time();
    nonce_arm_ = true;
    rawToHex(salt_, sizeof(salt_), salt_hex, salt_cap);
    rawToHex(nonce_, sizeof(nonce_), nonce_hex, nonce_cap);
    rawToHex(newsalt_, sizeof(newsalt_), newsalt_hex, newsalt_cap);
    return salt_hex[0] != '\0' && nonce_hex[0] != '\0' && newsalt_hex[0] != '\0';
}

bool HttpAuth::login(const char *user, const char *proof_hex, char *sess_out, size_t sess_cap)
{
    if (!user || user[0] == '\0' || !consumeProof(proof_hex) || !constEq(user, user_)) {
        return false;
    }
    if (mu_ != nullptr) {
        xSemaphoreTake(mu_, portMAX_DELAY);
    }
    mintSession();
    if (sess_out && sess_cap > 0) {
        snprintf(sess_out, sess_cap, "%s", sess_);
    }
    if (mu_ != nullptr) {
        xSemaphoreGive(mu_);
    }
    return true;
}

void HttpAuth::logout()
{
    if (mu_ != nullptr) {
        xSemaphoreTake(mu_, portMAX_DELAY);
    }
    clearSession();
    if (mu_ != nullptr) {
        xSemaphoreGive(mu_);
    }
}

bool HttpAuth::setPassword(const char *proof_hex, const char *new_hash_hex)
{
    uint8_t ns[kSaltLen];
    memcpy(ns, newsalt_, sizeof(ns));
    if (!consumeProof(proof_hex)) {
        return false;
    }
    uint8_t nh[kHashLen];
    if (!hexToRaw(new_hash_hex, nh, sizeof(nh))) {
        return false;
    }
    memcpy(salt_, ns, sizeof(salt_));
    memcpy(hash_, nh, sizeof(hash_));
    hash_ok_ = true;
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
