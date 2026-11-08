#pragma once

#include "esp_http_server.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include <atomic>
#include <cstddef>
#include <cstdint>

/**
 * Логин/сессия cookie + API Bearer token (NVS).
 * Пароль в NVS не пишется: соль и SHA-256(salt||password).
 * Браузер шлёт proof = SHA-256(nonce||hash). Заводской пароль admin.
 */
class HttpAuth {
public:
    static constexpr size_t kUserMax = 32;
    static constexpr size_t kPassMax = 64;
    static constexpr size_t kTokenLen = 32;
    static constexpr size_t kSessLen = 32;
    static constexpr size_t kSaltLen = 16;
    static constexpr size_t kHashLen = 32;
    static constexpr uint32_t kSessTtlMinSec = 30;
    static constexpr uint32_t kSessTtlMaxSec = 300; /* ≤ 5 мин */
    static constexpr uint32_t kSessTtlDefSec = 300;

    void begin();

    /**
     * Cookie-сессия или Bearer. Cookie истекает по idle TTL; Bearer — нет.
     * slide=true — сдвинуть idle. false только у фонового /api/status.
     */
    bool authorized(httpd_req_t *req, bool slide = true);
    /** Страница: fetch с заголовком X-Esp-Client. Адресная строка его не шлёт. */
    bool fromPage(httpd_req_t *req) const;
    /** Cookie этой сессии (и сессия ещё жива). Не сдвигает idle. */
    bool cookieMatches(httpd_req_t *req) const;
    /** Сравнение cookie с sess_, даже если idle уже истёк. */
    bool cookieEquals(httpd_req_t *req) const;
    /** Соль текущего пароля, одноразовый nonce и соль для нового пароля. Hex. */
    bool challenge(char *salt_hex, size_t salt_cap, char *nonce_hex, size_t nonce_cap,
                   char *newsalt_hex, size_t newsalt_cap);
    /** proof = hex SHA-256(nonce || hash). Пароль сюда не приходит. */
    bool login(const char *user, const char *proof_hex, char *sess_out, size_t sess_cap);
    void logout();
    /** proof старого пароля, hash = hex SHA-256(newsalt || новый пароль). */
    bool setPassword(const char *proof_hex, const char *new_hash_hex);
    void reissueToken();
    bool setSessionTtlSec(uint32_t sec);

    const char *user() const { return user_; }
    const char *apiToken() const { return api_token_; }
    bool sessionActive() const;
    const char *session() const { return sess_; }
    uint32_t sessionTtlSec() const { return sess_ttl_sec_; }
    /** Остаток cookie-сессии, сек (0 если нет). */
    uint32_t sessionRemainSec() const;
    /** Периодически: истёкшая по idle сессия очищается и помечается как оборванная. */
    void expireIdle();
    /** true один раз после обрыва сессии (idle, logout, вход с другого места). */
    bool takeEnded();

private:
    void load();
    void save() const;
    void mintSession();
    void mintToken();
    void touchSession();
    bool sessionValid() const;
    void clearSession();
    void clearNonce();
    bool nonceFresh() const;
    /** Съедает nonce. true, если proof совпал с текущим хешем. */
    bool consumeProof(const char *proof_hex);
    void sealPlain(const char *pass);

    char user_[kUserMax]{};
    uint8_t salt_[kSaltLen]{};
    uint8_t hash_[kHashLen]{};
    bool hash_ok_{false};
    uint8_t nonce_[kSaltLen]{};
    uint8_t newsalt_[kSaltLen]{};
    int64_t nonce_us_{0};
    bool nonce_arm_{false};
    char api_token_[kTokenLen + 1]{};
    char sess_[kSessLen + 1]{};
    uint32_t sess_ttl_sec_{kSessTtlDefSec};
    /** atomic: на ESP32 int64_t иначе читается «рвано» → ложный expiry. */
    std::atomic<int64_t> sess_touch_us_{0};
    std::atomic<bool> sess_ended_{false};
    mutable SemaphoreHandle_t mu_{nullptr};
};
