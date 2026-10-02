#pragma once

#include <stddef.h>
#include <stdint.h>

#include "esp_eth.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

class HttpUi;

class EthW5500 {
public:
    void begin(HttpUi *http);
    void request_recover(const char *why);
    void note_http_rx();
    void ip_text(char *buf, size_t len) const;
    bool set_ip(uint32_t addr);
    bool set_mask(uint32_t mask);
    bool set_gw(uint32_t gw);
    void print() const;
    bool link_up() const { return link_up_; }
    bool has_ip() const { return got_ip_; }

private:
    void pulse_rst();
    bool apply_static_ip();
    bool apply_now();
    void send_garp();
    void load();
    void save();
    void recover(const char *why);
    void datapath_tick();
    void recover_loop();
    void on_eth_event(int32_t event_id, void *event_data);
    void on_got_ip(const ip_event_got_ip_t *event);

    static void recover_task(void *arg);
    static void eth_event(void *arg, esp_event_base_t base, int32_t id, void *data);
    static void got_ip_event(void *arg, esp_event_base_t base, int32_t id, void *data);

    HttpUi *http_{};
    esp_netif_t *netif_{};
    esp_eth_handle_t handle_{};
    esp_eth_mac_t *mac_{};
    esp_eth_phy_t *phy_{};
    char ip_[16]{"0.0.0.0"};
    uint32_t cfg_ip_{};
    uint32_t cfg_mask_{};
    uint32_t cfg_gw_{};
    uint32_t cfg_dns_{};

    bool link_up_{};
    volatile bool got_ip_{};
    volatile bool recover_req_{};
    volatile bool recovering_{};
    volatile bool ignore_linkdown_{};
    volatile bool http_was_alive_{};
    volatile TickType_t http_rx_tick_{};
    TickType_t last_recover_tick_{};
    volatile uint8_t silent_rst_streak_{};
    const char *recover_why_{"link down"};
};
