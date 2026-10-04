#pragma once

#include <stddef.h>
#include <stdint.h>

#include "esp_eth.h"
#include "esp_event.h"
#include "esp_netif.h"

class HttpStatus;

class EthLan8720 {
public:
    void begin(HttpStatus *http);
    void reset();
    void ip_text(char *buf, size_t len) const;
    void mask_text(char *buf, size_t len) const;
    void gw_text(char *buf, size_t len) const;
    void mac_text(char *buf, size_t len) const;
    bool set_ip(uint32_t addr);
    bool set_mask(uint32_t mask);
    bool set_gw(uint32_t gw);
    void print() const;
    bool link_up() const { return link_up_; }
    bool has_ip() const { return got_ip_; }

private:
    bool apply_static_ip();
    bool apply_now();
    void load();
    void save();
    void phy_power(bool on);
    void on_eth_event(int32_t event_id, void *event_data);
    void on_got_ip(const ip_event_got_ip_t *event);

    static void eth_event(void *arg, esp_event_base_t base, int32_t id, void *data);
    static void got_ip_event(void *arg, esp_event_base_t base, int32_t id, void *data);

    HttpStatus *http_{};
    esp_netif_t *netif_{};
    esp_eth_handle_t handle_{};
    char ip_[16]{"0.0.0.0"};
    uint32_t cfg_ip_{};
    uint32_t cfg_mask_{};
    uint32_t cfg_gw_{};
    uint32_t cfg_dns_{};
    uint8_t mac_[6]{};
    volatile bool link_up_{};
    volatile bool got_ip_{};
};
