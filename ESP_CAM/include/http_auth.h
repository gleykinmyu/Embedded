#pragma once

#include "esp_http_server.h"

#include <cstddef>
#include <cstdint>

/** Логин/сессия cookie + API Bearer token (NVS). По умолчанию admin/admin. */
class HttpAuth {
public:
    static constexpr size_t kUserMax = 32;
    static constexpr size_t kPassMax = 32;
    static constexpr size_t kTokenLen = 32;
    static constexpr size_t kSessLen = 32;
    static constexpr uint32_t kSessTtlMinSec = 30;
    static constexpr uint32_t kSessTtlMaxSec = 300; /* ≤ 5 мин */
    static constexpr uint32_t kSessTtlDefSec = 300;

    void begin();

    /**
     * Cookie-сессия или Bearer. Cookie истекает по idle TTL; Bearer — нет.
     * slide=true — сдвинуть idle (действие пользователя); false — только проверить
     * (фоновый poll /api/status не должен продлевать сессию).
     */
    bool authorized(httpd_req_t *req, bool slide = true);
    bool login(const char *user, const char *pass, char *sess_out, size_t sess_cap);
    void logout();
    bool setPassword(const char *old_pass, const char *new_pass);
    void reissueToken();
    bool setSessionTtlSec(uint32_t sec);

    const char *user() const { return user_; }
    const char *apiToken() const { return api_token_; }
    bool sessionActive() const;
    const char *session() const { return sess_; }
    uint32_t sessionTtlSec() const { return sess_ttl_sec_; }
    /** Остаток cookie-сессии, сек (0 если нет). */
    uint32_t sessionRemainSec() const;

private:
    void load();
    void save() const;
    void mintSession();
    void mintToken();
    void touchSession();
    bool sessionValid() const;
    void clearSession();

    char user_[kUserMax]{};
    char pass_[kPassMax]{};
    char api_token_[kTokenLen + 1]{};
    char sess_[kSessLen + 1]{};
    uint32_t sess_ttl_sec_{kSessTtlDefSec};
    int64_t sess_touch_us_{0};
};
