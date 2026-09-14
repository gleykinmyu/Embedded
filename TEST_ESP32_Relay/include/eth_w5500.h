#pragma once

#include <stddef.h>

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
    bool link_up() const { return link_up_; }

private:
    void pulse_rst();
    void apply_static_ip();
    void send_garp();
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
