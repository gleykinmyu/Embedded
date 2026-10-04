#pragma once

#include <cstdint>

class EthLan8720;
class HttpStatus;
class Ph350Ctrl;

class CliEth01 {
public:
    CliEth01(EthLan8720 &eth, HttpStatus &http, Ph350Ctrl &pt);
    void begin();

private:
    static int cmd_status(void *ctx, int argc, char **argv);
    static int cmd_ip(void *ctx, int argc, char **argv);
    static int cmd_ethreset(void *ctx, int argc, char **argv);
    static int cmd_ping(void *ctx, int argc, char **argv);
    static int cmd_pan(void *ctx, int argc, char **argv);
    static int cmd_tilt(void *ctx, int argc, char **argv);
    static int cmd_stop(void *ctx, int argc, char **argv);
    static int cmd_power(void *ctx, int argc, char **argv);
    static int cmd_home(void *ctx, int argc, char **argv);
    static int cmd_speed(void *ctx, int argc, char **argv);
    static int cmd_preset(void *ctx, int argc, char **argv);
    static int cmd_pos(void *ctx, int argc, char **argv);
    static int cmd_raw(void *ctx, int argc, char **argv);

    void reg(const char *name, const char *help, const char *hint, int (*fn)(void *, int, char **));
    static uint8_t rate_arg(Ph350Ctrl &pt, int argc, char **argv, int idx);

    EthLan8720 &eth_;
    HttpStatus &http_;
    Ph350Ctrl &pt_;
};
