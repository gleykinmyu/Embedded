#include "ota_espota.h"

#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "board_eth01.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_random.h"
#include "esp_rom_md5.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/inet.h"
#include "lwip/sockets.h"

static const char *TAG = "ota";
static volatile bool g_ota_busy = false;

namespace {

constexpr int kOtaPort = 3232;
constexpr int kCmdFlash = 0;
constexpr int kCmdAuth = 200;
constexpr size_t kChunk = 1460;
/** Выше pt_health/http (обычно 5), ниже lwIP/timer. */
constexpr UBaseType_t kOtaFlashPriority = 12;

struct OtaFlashGuard {
    TaskHandle_t self;
    UBaseType_t prev_prio;

    OtaFlashGuard() {
        self = xTaskGetCurrentTaskHandle();
        prev_prio = uxTaskPriorityGet(self);
        g_ota_busy = true;
        vTaskPrioritySet(self, kOtaFlashPriority);
        ESP_LOGI(TAG, "flash mode on (prio %u→%u)", static_cast<unsigned>(prev_prio),
                 static_cast<unsigned>(kOtaFlashPriority));
    }

    ~OtaFlashGuard() {
        vTaskPrioritySet(self, prev_prio);
        g_ota_busy = false;
        ESP_LOGI(TAG, "flash mode off");
    }

    OtaFlashGuard(const OtaFlashGuard &) = delete;
    OtaFlashGuard &operator=(const OtaFlashGuard &) = delete;
};

void digest_to_hex(const uint8_t dig[16], char out[33]) {
    for (int i = 0; i < 16; ++i) {
        snprintf(out + i * 2, 3, "%02x", dig[i]);
    }
}

void md5_bytes(const uint8_t *data, size_t len, char out[33]) {
    md5_context_t ctx;
    uint8_t dig[16] = {};
    esp_rom_md5_init(&ctx);
    esp_rom_md5_update(&ctx, data, len);
    esp_rom_md5_final(dig, &ctx);
    digest_to_hex(dig, out);
}

void md5_str(const char *s, char out[33]) {
    md5_bytes(reinterpret_cast<const uint8_t *>(s), strlen(s), out);
}

void udp_reply(int sock, const struct sockaddr_in *to, socklen_t tolen, const char *msg) {
    (void)sendto(sock, msg, strlen(msg), 0, reinterpret_cast<const struct sockaddr *>(to), tolen);
}

bool run_flash(const struct sockaddr_in *host, uint16_t host_port, uint32_t size,
               const char md5_hex_expect[33]) {
    OtaFlashGuard flash_guard;
    // дать pt_health увидеть otaIsBusy() и выйти из tick
    vTaskDelay(pdMS_TO_TICKS(300));

    const esp_partition_t *part = esp_ota_get_next_update_partition(nullptr);
    if (!part) {
        ESP_LOGE(TAG, "no OTA partition");
        return false;
    }
    if (size == 0 || size > part->size) {
        ESP_LOGE(TAG, "bad size %lu (part %lu)", static_cast<unsigned long>(size),
                 static_cast<unsigned long>(part->size));
        return false;
    }

    // Сначала TCP: host ждёт accept ~10с. esp_ota_begin стирает раздел и может
    // занять дольше (особенно при активном опросе камеры) — раньше connect не успевал.
    const int tcp = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (tcp < 0) {
        ESP_LOGE(TAG, "TCP socket failed");
        return false;
    }

    struct sockaddr_in dest = {};
    dest.sin_family = AF_INET;
    dest.sin_addr = host->sin_addr;
    dest.sin_port = htons(host_port);

    timeval tv = {};
    tv.tv_sec = 15;
    setsockopt(tcp, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(tcp, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    if (connect(tcp, reinterpret_cast<struct sockaddr *>(&dest), sizeof(dest)) != 0) {
        ESP_LOGE(TAG, "TCP connect failed");
        close(tcp);
        return false;
    }
    ESP_LOGI(TAG, "TCP connected, begin flash erase/write");

    esp_ota_handle_t handle = 0;
    esp_err_t err = esp_ota_begin(part, size, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_begin: %s", esp_err_to_name(err));
        (void)send(tcp, "ERROR", 5, 0);
        close(tcp);
        return false;
    }

    md5_context_t md5;
    esp_rom_md5_init(&md5);

    uint32_t received = 0;
    uint8_t buf[kChunk];
    bool ok = true;

    while (received < size) {
        const size_t need = size - received;
        const size_t want = need > kChunk ? kChunk : need;
        const int n = recv(tcp, buf, want, 0);
        if (n <= 0) {
            ESP_LOGE(TAG, "recv fail at %lu/%lu", static_cast<unsigned long>(received),
                     static_cast<unsigned long>(size));
            ok = false;
            break;
        }
        err = esp_ota_write(handle, buf, static_cast<size_t>(n));
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "esp_ota_write: %s", esp_err_to_name(err));
            ok = false;
            break;
        }
        esp_rom_md5_update(&md5, buf, static_cast<size_t>(n));
        received += static_cast<uint32_t>(n);

        char ack[16];
        const int alen = snprintf(ack, sizeof(ack), "%d", n);
        if (send(tcp, ack, alen, 0) < 0) {
            ESP_LOGE(TAG, "ack send failed");
            ok = false;
            break;
        }
    }

    if (ok) {
        uint8_t dig[16] = {};
        char got[33] = {};
        esp_rom_md5_final(dig, &md5);
        digest_to_hex(dig, got);
        if (strcasecmp(got, md5_hex_expect) != 0) {
            ESP_LOGE(TAG, "MD5 mismatch");
            ok = false;
        }
    }

    if (!ok) {
        (void)send(tcp, "ERROR", 5, 0);
        close(tcp);
        esp_ota_abort(handle);
        return false;
    }

    err = esp_ota_end(handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_end: %s", esp_err_to_name(err));
        (void)send(tcp, "ERROR", 5, 0);
        close(tcp);
        return false;
    }
    err = esp_ota_set_boot_partition(part);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "set_boot: %s", esp_err_to_name(err));
        (void)send(tcp, "ERROR", 5, 0);
        close(tcp);
        return false;
    }

    (void)send(tcp, "OK", 2, 0);
    close(tcp);
    ESP_LOGI(TAG, "OTA ok, reboot to %s", part->label);
    vTaskDelay(pdMS_TO_TICKS(200));
    esp_restart();
    return true;
}

bool auth_ok(int sock, const struct sockaddr_in *from, socklen_t fromlen) {
    if (ESPOTA_PASSWORD[0] == '\0') {
        return true;
    }

    uint8_t rnd[16] = {};
    esp_fill_random(rnd, sizeof(rnd));
    char nonce[33] = {};
    digest_to_hex(rnd, nonce);

    char auth_msg[48];
    snprintf(auth_msg, sizeof(auth_msg), "AUTH %s", nonce);
    udp_reply(sock, from, fromlen, auth_msg);

    const TickType_t t0 = xTaskGetTickCount();
    const TickType_t limit = pdMS_TO_TICKS(10000);
    char areq[128] = {};
    struct sockaddr_in peer = {};
    int n = -1;
    while (static_cast<TickType_t>(xTaskGetTickCount() - t0) < limit) {
        const uint32_t left_ms =
            (limit - static_cast<TickType_t>(xTaskGetTickCount() - t0)) * portTICK_PERIOD_MS;
        if (left_ms == 0) {
            break;
        }
        timeval tv = {};
        tv.tv_sec = static_cast<time_t>(left_ms / 1000);
        tv.tv_usec = static_cast<long>((left_ms % 1000) * 1000);
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

        socklen_t peerlen = sizeof(peer);
        n = recvfrom(sock, areq, sizeof(areq) - 1, 0, reinterpret_cast<struct sockaddr *>(&peer),
                     &peerlen);
        if (n <= 0) {
            n = -1;
            break;
        }
        if (peer.sin_addr.s_addr != from->sin_addr.s_addr) {
            ESP_LOGW(TAG, "auth from other host ignored");
            n = -1;
            continue;
        }
        break;
    }

    timeval tv = {};
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    if (n <= 0) {
        ESP_LOGW(TAG, "auth timeout");
        return false;
    }
    areq[n] = '\0';

    int acmd = 0;
    char cnonce[40] = {};
    char result[40] = {};
    if (sscanf(areq, "%d %39s %39s", &acmd, cnonce, result) != 3 || acmd != kCmdAuth) {
        udp_reply(sock, from, fromlen, "Authentication Failed");
        return false;
    }

    char pass_md5[33] = {};
    md5_str(ESPOTA_PASSWORD, pass_md5);
    char mix[128];
    snprintf(mix, sizeof(mix), "%s:%s:%s", pass_md5, nonce, cnonce);
    char expect[33] = {};
    md5_str(mix, expect);
    if (strcasecmp(expect, result) != 0) {
        udp_reply(sock, from, fromlen, "Authentication Failed");
        ESP_LOGW(TAG, "auth failed");
        return false;
    }
    return true;
}

void handle_invite(int sock, char *msg, const struct sockaddr_in *from, socklen_t fromlen) {
    int cmd = 0;
    int host_port = 0;
    unsigned long size = 0;
    char md5[40] = {};
    if (sscanf(msg, "%d %d %lu %39s", &cmd, &host_port, &size, md5) != 4) {
        ESP_LOGW(TAG, "bad invite: %s", msg);
        return;
    }
    if (cmd != kCmdFlash || strlen(md5) != 32 || host_port <= 0 || host_port > 65535 || size == 0) {
        udp_reply(sock, from, fromlen, "ERR");
        return;
    }

    // Пауза health уже на auth — до erase/TCP.
    g_ota_busy = true;
    vTaskDelay(pdMS_TO_TICKS(300));

    if (!auth_ok(sock, from, fromlen)) {
        g_ota_busy = false;
        return;
    }

    udp_reply(sock, from, fromlen, "OK");
    ESP_LOGI(TAG, "OTA invite %lu bytes from %s:%d", size, inet_ntoa(from->sin_addr), host_port);
    (void)run_flash(from, static_cast<uint16_t>(host_port), static_cast<uint32_t>(size), md5);
}

void ota_task(void *) {
    const int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock < 0) {
        ESP_LOGE(TAG, "UDP socket failed");
        vTaskDelete(nullptr);
        return;
    }

    int yes = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(kOtaPort);
    if (bind(sock, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr)) != 0) {
        ESP_LOGE(TAG, "UDP bind :%d failed", kOtaPort);
        close(sock);
        vTaskDelete(nullptr);
        return;
    }

    ESP_LOGI(TAG, "espota listen UDP :%d", kOtaPort);

    char buf[160];
    for (;;) {
        struct sockaddr_in from = {};
        socklen_t fromlen = sizeof(from);
        const int n =
            recvfrom(sock, buf, sizeof(buf) - 1, 0, reinterpret_cast<struct sockaddr *>(&from), &fromlen);
        if (n <= 0) {
            continue;
        }
        buf[n] = '\0';
        handle_invite(sock, buf, &from, fromlen);
    }
}

} // namespace

bool otaIsBusy() {
    return g_ota_busy;
}

void OtaEspota::begin() {
    if (xTaskCreate(ota_task, "espota", 8192, nullptr, 5, nullptr) != pdPASS) {
        ESP_LOGE(TAG, "task create failed");
    }
}
