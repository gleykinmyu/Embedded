#include "uart_cli.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "board_pins.h"
#include "esp_console.h"
#include "eth_w5500.h"
#include "http_ui.h"
#include "relay_board.h"

UartCli::UartCli(RelayBoard &relays, EthW5500 &eth, HttpUi &http)
    : relays_(relays), eth_(eth), http_(http) {}

bool UartCli::parse_ch(int argc, char **argv, size_t *ch) {
    if (argc < 2) {
        printf("channel 1..%u\n", static_cast<unsigned>(RELAY_COUNT));
        return false;
    }
    const int n = atoi(argv[1]);
    if (n < 1 || static_cast<size_t>(n) > RELAY_COUNT) {
        printf("channel 1..%u\n", static_cast<unsigned>(RELAY_COUNT));
        return false;
    }
    *ch = static_cast<size_t>(n - 1);
    return true;
}

int UartCli::set_ch(void *ctx, int argc, char **argv, bool on) {
    auto *cli = static_cast<UartCli *>(ctx);
    size_t ch = 0;
    if (!parse_ch(argc, argv, &ch) || !cli->relays_.set(ch, on)) {
        return 1;
    }
    cli->relays_.print();
    return 0;
}

int UartCli::cmd_status(void *ctx, int argc, char **argv) {
    (void)argc;
    (void)argv;
    auto *cli = static_cast<UartCli *>(ctx);
    printf("eth=%s http=%s\n", cli->eth_.link_up() ? "up" : "down",
           cli->http_.listening() ? "listen" : "down");
    cli->relays_.print();
    return 0;
}

int UartCli::cmd_on(void *ctx, int argc, char **argv) {
    return set_ch(ctx, argc, argv, true);
}

int UartCli::cmd_off(void *ctx, int argc, char **argv) {
    return set_ch(ctx, argc, argv, false);
}

int UartCli::cmd_toggle(void *ctx, int argc, char **argv) {
    auto *cli = static_cast<UartCli *>(ctx);
    size_t ch = 0;
    if (!parse_ch(argc, argv, &ch) || !cli->relays_.set(ch, !cli->relays_.is_on(ch))) {
        return 1;
    }
    cli->relays_.print();
    return 0;
}

int UartCli::cmd_allon(void *ctx, int argc, char **argv) {
    (void)argc;
    (void)argv;
    auto *cli = static_cast<UartCli *>(ctx);
    if (!cli->relays_.all_on()) {
        return 1;
    }
    cli->relays_.print();
    return 0;
}

int UartCli::cmd_alloff(void *ctx, int argc, char **argv) {
    (void)argc;
    (void)argv;
    auto *cli = static_cast<UartCli *>(ctx);
    if (!cli->relays_.all_off()) {
        return 1;
    }
    cli->relays_.print();
    return 0;
}

int UartCli::cmd_chase(void *ctx, int argc, char **argv) {
    auto *cli = static_cast<UartCli *>(ctx);
    if (argc < 2) {
        printf("chase start|stop\n");
        return 1;
    }
    const bool start = strcmp(argv[1], "start") == 0;
    const bool stop = strcmp(argv[1], "stop") == 0;
    if (!start && !stop) {
        printf("chase start|stop\n");
        return 1;
    }
    if (start ? !cli->relays_.chase_start() : !cli->relays_.chase_stop()) {
        return 1;
    }
    cli->relays_.print();
    return 0;
}

int UartCli::cmd_ethreset(void *ctx, int argc, char **argv) {
    (void)argc;
    (void)argv;
    static_cast<UartCli *>(ctx)->eth_.request_recover("uart");
    return 0;
}

void UartCli::reg(const char *name, const char *help, const char *hint,
                  int (*fn)(void *, int, char **)) {
    esp_console_cmd_t cmd = {};
    cmd.command = name;
    cmd.help = help;
    cmd.hint = hint;
    cmd.func_w_context = fn;
    cmd.context = this;
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}

void UartCli::begin() {
    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    repl_config.prompt = "relay>";
    repl_config.max_cmdline_length = 64;
    esp_console_dev_usb_serial_jtag_config_t hw_config =
        ESP_CONSOLE_DEV_USB_SERIAL_JTAG_CONFIG_DEFAULT();
    esp_console_repl_t *repl = nullptr;
    ESP_ERROR_CHECK(esp_console_new_repl_usb_serial_jtag(&hw_config, &repl_config, &repl));
    ESP_ERROR_CHECK(esp_console_register_help_command());
    reg("status", "ETH/HTTP/relays/DI1", nullptr, cmd_status);
    reg("on", "on 1..8", "<ch>", cmd_on);
    reg("off", "off 1..8", "<ch>", cmd_off);
    reg("toggle", "toggle 1..8", "<ch>", cmd_toggle);
    reg("allon", "All on", nullptr, cmd_allon);
    reg("alloff", "All off", nullptr, cmd_alloff);
    reg("chase", "chase start|stop", "start|stop", cmd_chase);
    reg("ethreset", "Hardware reset W5500", nullptr, cmd_ethreset);
    ESP_ERROR_CHECK(esp_console_start_repl(repl));
}
