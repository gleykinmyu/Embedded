#pragma once

class EthLan8720;
class HttpStatus;

class CliEth01 {
public:
    CliEth01(EthLan8720 &eth, HttpStatus &http);
    void begin();

private:
    static int cmd_status(void *ctx, int argc, char **argv);
    static int cmd_ip(void *ctx, int argc, char **argv);
    static int cmd_ethreset(void *ctx, int argc, char **argv);
    static int cmd_ping(void *ctx, int argc, char **argv);

    void reg(const char *name, const char *help, const char *hint, int (*fn)(void *, int, char **));

    EthLan8720 &eth_;
    HttpStatus &http_;
};
