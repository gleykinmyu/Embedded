#pragma once

#include <stddef.h>

class EthW5500;
class HttpUi;
class RelayBoard;

class UartCli {
public:
    UartCli(RelayBoard &relays, EthW5500 &eth, HttpUi &http);
    void begin();

private:
    static int cmd_status(void *ctx, int argc, char **argv);
    static int cmd_on(void *ctx, int argc, char **argv);
    static int cmd_off(void *ctx, int argc, char **argv);
    static int cmd_toggle(void *ctx, int argc, char **argv);
    static int cmd_allon(void *ctx, int argc, char **argv);
    static int cmd_alloff(void *ctx, int argc, char **argv);
    static int cmd_chase(void *ctx, int argc, char **argv);
    static int cmd_ethreset(void *ctx, int argc, char **argv);

    static bool parse_ch(int argc, char **argv, size_t *ch);
    static int set_ch(void *ctx, int argc, char **argv, bool on);
    void reg(const char *name, const char *help, const char *hint,
             int (*fn)(void *, int, char **));

    RelayBoard &relays_;
    EthW5500 &eth_;
    HttpUi &http_;
};
