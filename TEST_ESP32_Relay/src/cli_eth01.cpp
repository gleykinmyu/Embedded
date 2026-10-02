#include "cli_eth01.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "esp_console.h"
#include "eth_lan8720.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "http_status.h"
#include "lwip/inet.h"
#include "lwip/ip_addr.h"
#include "ping/ping_sock.h"

CliEth01::CliEth01(EthLan8720 &eth, HttpStatus &http) : eth_(eth), http_(http) {}

int CliEth01::cmd_status(void *ctx, int argc, char **argv) {
    (void)argc;
    (void)argv;
    auto *cli = static_cast<CliEth01 *>(ctx);
    char ip[16] = {};
    char mac[18] = {};
    cli->eth_.ip_text(ip, sizeof(ip));
    cli->eth_.mac_text(mac, sizeof(mac));
    printf("eth=%s ip=%s mac=%s http=%s\n", cli->eth_.link_up() ? "up" : "down", ip, mac,
           cli->http_.listening() ? "listen" : "down");
    cli->eth_.print();
    return 0;
}

int CliEth01::cmd_ethreset(void *ctx, int argc, char **argv) {
    (void)argc;
    (void)argv;
    static_cast<CliEth01 *>(ctx)->eth_.reset();
    printf("phy reset\n");
    return 0;
}

namespace {

bool parse_ip4(const char *s, uint32_t *out) {
    ip_addr_t a{};
    if (s == nullptr || out == nullptr || !ipaddr_aton(s, &a) || !IP_IS_V4(&a)) {
        return false;
    }
    *out = ip4_addr_get_u32(ip_2_ip4(&a));
    return true;
}

struct PingWait {
    SemaphoreHandle_t done;
};

void ping_ok(esp_ping_handle_t hdl, void *) {
    uint16_t seq = 0;
    uint8_t ttl = 0;
    uint32_t elapsed = 0;
    uint32_t size = 0;
    ip_addr_t addr{};
    (void)esp_ping_get_profile(hdl, ESP_PING_PROF_SEQNO, &seq, sizeof(seq));
    (void)esp_ping_get_profile(hdl, ESP_PING_PROF_TTL, &ttl, sizeof(ttl));
    (void)esp_ping_get_profile(hdl, ESP_PING_PROF_TIMEGAP, &elapsed, sizeof(elapsed));
    (void)esp_ping_get_profile(hdl, ESP_PING_PROF_SIZE, &size, sizeof(size));
    (void)esp_ping_get_profile(hdl, ESP_PING_PROF_IPADDR, &addr, sizeof(addr));
    printf("%lu bytes from %s: seq=%u ttl=%u time=%lums\n", static_cast<unsigned long>(size),
           ipaddr_ntoa(&addr), static_cast<unsigned>(seq), static_cast<unsigned>(ttl),
           static_cast<unsigned long>(elapsed));
}

void ping_timeout(esp_ping_handle_t hdl, void *) {
    uint16_t seq = 0;
    (void)esp_ping_get_profile(hdl, ESP_PING_PROF_SEQNO, &seq, sizeof(seq));
    printf("timeout seq=%u\n", static_cast<unsigned>(seq));
}

void ping_end(esp_ping_handle_t hdl, void *args) {
    uint32_t sent = 0;
    uint32_t recv = 0;
    uint32_t ms = 0;
    (void)esp_ping_get_profile(hdl, ESP_PING_PROF_REQUEST, &sent, sizeof(sent));
    (void)esp_ping_get_profile(hdl, ESP_PING_PROF_REPLY, &recv, sizeof(recv));
    (void)esp_ping_get_profile(hdl, ESP_PING_PROF_DURATION, &ms, sizeof(ms));
    printf("%lu sent, %lu recv, %lu ms\n", static_cast<unsigned long>(sent),
           static_cast<unsigned long>(recv), static_cast<unsigned long>(ms));
    auto *wait = static_cast<PingWait *>(args);
    if (wait && wait->done) {
        xSemaphoreGive(wait->done);
    }
}

} // namespace

int CliEth01::cmd_ip(void *ctx, int argc, char **argv) {
    auto *cli = static_cast<CliEth01 *>(ctx);
    if (argc < 2) {
        cli->eth_.print();
        return 0;
    }

    uint32_t addr = 0;
    if (argc == 2) {
        if (!parse_ip4(argv[1], &addr) || !cli->eth_.set_ip(addr)) {
            printf("ip <a.b.c.d> | ip mask <a.b.c.d> | ip gw <a.b.c.d>\n");
            return 1;
        }
        cli->eth_.print();
        return 0;
    }

    if (argc != 3 || !parse_ip4(argv[2], &addr)) {
        printf("ip <a.b.c.d> | ip mask <a.b.c.d> | ip gw <a.b.c.d>\n");
        return 1;
    }
    if (strcmp(argv[1], "mask") == 0) {
        if (!cli->eth_.set_mask(addr)) {
            printf("bad mask\n");
            return 1;
        }
    } else if (strcmp(argv[1], "gw") == 0) {
        if (!cli->eth_.set_gw(addr)) {
            printf("bad gw\n");
            return 1;
        }
    } else {
        printf("ip <a.b.c.d> | ip mask <a.b.c.d> | ip gw <a.b.c.d>\n");
        return 1;
    }
    cli->eth_.print();
    return 0;
}

int CliEth01::cmd_ping(void *ctx, int argc, char **argv) {
    auto *cli = static_cast<CliEth01 *>(ctx);
    if (argc < 2) {
        printf("ping <ip> [count]\n");
        return 1;
    }
    if (!cli->eth_.has_ip()) {
        printf("no ip\n");
        return 1;
    }

    ip_addr_t target{};
    if (!ipaddr_aton(argv[1], &target)) {
        printf("bad ip\n");
        return 1;
    }

    uint32_t count = 4;
    if (argc >= 3) {
        const int n = atoi(argv[2]);
        if (n < 1 || n > 20) {
            printf("count 1..20\n");
            return 1;
        }
        count = static_cast<uint32_t>(n);
    }

    PingWait wait{};
    wait.done = xSemaphoreCreateBinary();
    if (!wait.done) {
        return 1;
    }

    esp_ping_config_t cfg = ESP_PING_DEFAULT_CONFIG();
    cfg.count = count;
    cfg.interval_ms = 500;
    cfg.timeout_ms = 1000;
    cfg.target_addr = target;

    esp_ping_callbacks_t cbs = {};
    cbs.cb_args = &wait;
    cbs.on_ping_success = ping_ok;
    cbs.on_ping_timeout = ping_timeout;
    cbs.on_ping_end = ping_end;

    esp_ping_handle_t ping = nullptr;
    if (esp_ping_new_session(&cfg, &cbs, &ping) != ESP_OK || ping == nullptr) {
        printf("ping start failed\n");
        vSemaphoreDelete(wait.done);
        return 1;
    }
    if (esp_ping_start(ping) != ESP_OK) {
        printf("ping start failed\n");
        esp_ping_delete_session(ping);
        vSemaphoreDelete(wait.done);
        return 1;
    }

    const TickType_t wait_ticks = pdMS_TO_TICKS(count * (cfg.interval_ms + cfg.timeout_ms) + 2000);
    if (xSemaphoreTake(wait.done, wait_ticks) != pdTRUE) {
        printf("ping hung\n");
        esp_ping_stop(ping);
    }
    esp_ping_delete_session(ping);
    vSemaphoreDelete(wait.done);
    return 0;
}

void CliEth01::reg(const char *name, const char *help, const char *hint, int (*fn)(void *, int, char **)) {
    esp_console_cmd_t cmd = {};
    cmd.command = name;
    cmd.help = help;
    cmd.hint = hint;
    cmd.func_w_context = fn;
    cmd.context = this;
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}

void CliEth01::begin() {
    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    repl_config.prompt = "eth01>";
    repl_config.max_cmdline_length = 80;
    esp_console_dev_uart_config_t hw_config = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
    esp_console_repl_t *repl = nullptr;
    ESP_ERROR_CHECK(esp_console_new_repl_uart(&hw_config, &repl_config, &repl));
    ESP_ERROR_CHECK(esp_console_register_help_command());
    reg("status", "Link, IP, HTTP", nullptr, cmd_status);
    reg("ip", "Static IP / mask / gw", "[addr|mask|gw] [a.b.c.d]", cmd_ip);
    reg("ping", "ICMP ping", "<ip> [count]", cmd_ping);
    reg("ethreset", "Power-cycle LAN8720", nullptr, cmd_ethreset);
    ESP_ERROR_CHECK(esp_console_start_repl(repl));
}
