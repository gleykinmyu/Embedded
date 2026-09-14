#include "eth_w5500.h"

#include <stdio.h>
#include <string.h>

#include "board_pins.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_eth_mac.h"
#include "esp_eth_mac_spi.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif_net_stack.h"
#include "http_ui.h"
#include "lwip/etharp.h"
#include "lwip/netif.h"
#include "lwip/tcpip.h"

static const char *TAG = "eth";

#define ETH_TO_IP4(octets) ESP_IP4TOADDR(octets)

void EthW5500::pulse_rst() {
    gpio_set_level(static_cast<gpio_num_t>(PIN_ETH_RST), 0);
    vTaskDelay(pdMS_TO_TICKS(1));
    gpio_set_level(static_cast<gpio_num_t>(PIN_ETH_RST), 1);
    vTaskDelay(pdMS_TO_TICKS(10));
}

void EthW5500::apply_static_ip() {
    ESP_ERROR_CHECK(esp_netif_dhcpc_stop(netif_));
    esp_netif_ip_info_t ip_info = {};
    ip_info.ip.addr = ETH_TO_IP4(ETH_IP4_ADDR);
    ip_info.gw.addr = ETH_TO_IP4(ETH_GW4_ADDR);
    ip_info.netmask.addr = ETH_TO_IP4(ETH_MASK_ADDR);
    ESP_ERROR_CHECK(esp_netif_set_ip_info(netif_, &ip_info));

    esp_netif_dns_info_t dns = {};
    dns.ip.type = ESP_IPADDR_TYPE_V4;
    dns.ip.u_addr.ip4.addr = ETH_TO_IP4(ETH_DNS4_ADDR);
    ESP_ERROR_CHECK(esp_netif_set_dns_info(netif_, ESP_NETIF_DNS_MAIN, &dns));
}

void EthW5500::send_garp() {
    if (!netif_) {
        return;
    }
    auto *nif = static_cast<struct netif *>(esp_netif_get_netif_impl(netif_));
    if (!nif) {
        return;
    }
    LOCK_TCPIP_CORE();
    etharp_gratuitous(nif);
    UNLOCK_TCPIP_CORE();
}

void EthW5500::request_recover(const char *why) {
    recover_why_ = why;
    recover_req_ = true;
}

void EthW5500::note_http_rx() {
    http_rx_tick_ = xTaskGetTickCount();
    http_was_alive_ = true;
    silent_rst_streak_ = 0;
}

void EthW5500::ip_text(char *buf, size_t len) const {
    snprintf(buf, len, "%s", ip_);
}

void EthW5500::recover(const char *why) {
    if (!handle_ || !mac_ || !phy_) {
        return;
    }
    const TickType_t now = xTaskGetTickCount();
    if (last_recover_tick_ && (now - last_recover_tick_) < pdMS_TO_TICKS(ETH_RECOVER_DEBOUNCE_MS)) {
        return;
    }
    recovering_ = true;
    recover_req_ = false;
    ignore_linkdown_ = true;
    last_recover_tick_ = now;
    ESP_LOGW(TAG, "W5500 HW reset (%s)", why);

    link_up_ = false;
    got_ip_ = false;
    memcpy(ip_, "0.0.0.0", 8);
    if (http_) {
        http_->stop();
    }
    pulse_rst();

    (void)esp_eth_stop(handle_);
    (void)mac_->deinit(mac_);
    if (mac_->init(mac_) != ESP_OK) {
        ESP_LOGE(TAG, "W5500 MAC reinit failed");
        recovering_ = false;
        recover_req_ = true;
        return;
    }
    (void)phy_->init(phy_);
    uint8_t mac[6] = {};
    esp_read_mac(mac, ESP_MAC_ETH);
    (void)esp_eth_ioctl(handle_, ETH_CMD_S_MAC_ADDR, mac);
    if (esp_eth_start(handle_) != ESP_OK) {
        ESP_LOGE(TAG, "esp_eth_start after RST failed");
        recover_req_ = true;
    }
    recovering_ = false;
}

void EthW5500::datapath_tick() {
    if (!got_ip_ || !link_up_ || !http_ || !http_->listening() || recovering_ || recover_req_ ||
        !http_was_alive_) {
        return;
    }
    if (last_recover_tick_ &&
        (xTaskGetTickCount() - last_recover_tick_) < pdMS_TO_TICKS(ETH_RECOVER_COOLDOWN_MS)) {
        return;
    }
    if ((xTaskGetTickCount() - http_rx_tick_) < pdMS_TO_TICKS(ETH_HTTP_SILENT_MS)) {
        return;
    }
    if (silent_rst_streak_ >= 2) {
        http_was_alive_ = false;
        return;
    }
    silent_rst_streak_ = static_cast<uint8_t>(silent_rst_streak_ + 1);
    request_recover("http silent");
}

void EthW5500::recover_loop() {
    while (true) {
        datapath_tick();
        if (recover_req_ && !recovering_) {
            recover(recover_why_);
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

void EthW5500::recover_task(void *arg) {
    static_cast<EthW5500 *>(arg)->recover_loop();
}

void EthW5500::on_eth_event(int32_t event_id, void *event_data) {
    switch (event_id) {
        case ETHERNET_EVENT_CONNECTED: {
            link_up_ = true;
            ignore_linkdown_ = false;
            uint8_t mac[6] = {};
            auto handle = *static_cast<esp_eth_handle_t *>(event_data);
            esp_eth_ioctl(handle, ETH_CMD_G_MAC_ADDR, mac);
            ESP_LOGI(TAG, "ETH link up, MAC %02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2],
                     mac[3], mac[4], mac[5]);
            break;
        }
        case ETHERNET_EVENT_DISCONNECTED:
            link_up_ = false;
            got_ip_ = false;
            memcpy(ip_, "0.0.0.0", 8);
            if (!recovering_ && !ignore_linkdown_) {
                request_recover("link down");
            }
            ESP_LOGI(TAG, "ETH link down");
            break;
        default:
            break;
    }
}

void EthW5500::on_got_ip(const ip_event_got_ip_t *event) {
    got_ip_ = true;
    snprintf(ip_, sizeof(ip_), IPSTR, IP2STR(&event->ip_info.ip));
    ESP_LOGI(TAG, "IP %s", ip_);
    send_garp();
    if (http_) {
        http_->start();
    }
}

void EthW5500::eth_event(void *arg, esp_event_base_t, int32_t id, void *data) {
    static_cast<EthW5500 *>(arg)->on_eth_event(id, data);
}

void EthW5500::got_ip_event(void *arg, esp_event_base_t, int32_t, void *data) {
    static_cast<EthW5500 *>(arg)->on_got_ip(static_cast<ip_event_got_ip_t *>(data));
}

void EthW5500::begin(HttpUi *http) {
    http_ = http;
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(gpio_install_isr_service(0));

    esp_netif_config_t netif_cfg = {};
    netif_cfg.base = ESP_NETIF_BASE_DEFAULT_ETH;
    netif_cfg.stack = ESP_NETIF_NETSTACK_DEFAULT_ETH;
    netif_ = esp_netif_new(&netif_cfg);
    ESP_ERROR_CHECK(esp_netif_set_hostname(netif_, "esp32-relay"));
    apply_static_ip();

    spi_bus_config_t buscfg = {};
    buscfg.mosi_io_num = PIN_ETH_MOSI;
    buscfg.miso_io_num = PIN_ETH_MISO;
    buscfg.sclk_io_num = PIN_ETH_SCLK;
    buscfg.quadwp_io_num = -1;
    buscfg.quadhd_io_num = -1;
    ESP_ERROR_CHECK(spi_bus_initialize(SPI3_HOST, &buscfg, SPI_DMA_CH_AUTO));

    spi_device_interface_config_t spi_devcfg = {};
    spi_devcfg.mode = 0;
    spi_devcfg.clock_speed_hz = 10 * 1000 * 1000;
    spi_devcfg.spics_io_num = PIN_ETH_CS;
    spi_devcfg.queue_size = 20;

    gpio_config_t rst_cfg = {};
    rst_cfg.pin_bit_mask = 1ULL << PIN_ETH_RST;
    rst_cfg.mode = GPIO_MODE_OUTPUT;
    gpio_config(&rst_cfg);
    gpio_set_level(static_cast<gpio_num_t>(PIN_ETH_RST), 1);
    pulse_rst();

    eth_w5500_config_t w5500_config = {};
    w5500_config.int_gpio_num = PIN_ETH_INT;
    w5500_config.spi_host_id = SPI3_HOST;
    w5500_config.spi_devcfg = &spi_devcfg;

    eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
    mac_config.flags = ETH_MAC_FLAG_PIN_TO_CORE;
    eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();
    phy_config.phy_addr = 1;
    phy_config.reset_gpio_num = -1;

    mac_ = esp_eth_mac_new_w5500(&w5500_config, &mac_config);
    phy_ = esp_eth_phy_new_w5500(&phy_config);
    if (!mac_ || !phy_) {
        ESP_LOGE(TAG, "W5500 MAC/PHY create failed");
        return;
    }

    esp_eth_config_t config = ETH_DEFAULT_CONFIG(mac_, phy_);
    config.check_link_period_ms = 100;
    ESP_ERROR_CHECK(esp_eth_driver_install(&config, &handle_));

    uint8_t mac_addr[6] = {};
    ESP_ERROR_CHECK(esp_read_mac(mac_addr, ESP_MAC_ETH));
    ESP_ERROR_CHECK(esp_eth_ioctl(handle_, ETH_CMD_S_MAC_ADDR, mac_addr));
    ESP_ERROR_CHECK(esp_netif_attach(netif_, esp_eth_new_netif_glue(handle_)));
    ESP_ERROR_CHECK(esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, &eth_event, this));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, &got_ip_event, this));
    ESP_ERROR_CHECK(esp_eth_start(handle_));

    xTaskCreate(recover_task, "ethrst", 4096, this, 6, nullptr);
}
