#include "eth_lan8720.h"

#include <stdio.h>
#include <string.h>

#include "board_eth01.h"
#include "driver/gpio.h"
#include "esp_eth_mac_esp.h"
#include "esp_eth_netif_glue.h"
#include "esp_eth_phy.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "http_status.h"
#include "nvs.h"

static const char *TAG = "eth";

#define ETH_TO_IP4(octets) ESP_IP4TOADDR(octets)

namespace {

constexpr char kNvsNs[] = "eth01";
constexpr char kKeyIp[] = "ip";
constexpr char kKeyMask[] = "mask";
constexpr char kKeyGw[] = "gw";
constexpr char kKeyDns[] = "dns";

void fmt_ip4(char *buf, size_t len, uint32_t addr) {
    esp_ip4_addr_t a{};
    a.addr = addr;
    snprintf(buf, len, IPSTR, IP2STR(&a));
}

} // namespace

void EthLan8720::phy_power(bool on) {
    gpio_set_level(static_cast<gpio_num_t>(PIN_ETH_PHY_POWER), on ? 1 : 0);
}

bool EthLan8720::apply_static_ip() {
    if (netif_ == nullptr) {
        return false;
    }
    (void)esp_netif_dhcpc_stop(netif_);
    esp_netif_ip_info_t ip_info = {};
    ip_info.ip.addr = cfg_ip_;
    ip_info.gw.addr = cfg_gw_;
    ip_info.netmask.addr = cfg_mask_;
    if (esp_netif_set_ip_info(netif_, &ip_info) != ESP_OK) {
        return false;
    }

    esp_netif_dns_info_t dns = {};
    dns.ip.type = ESP_IPADDR_TYPE_V4;
    dns.ip.u_addr.ip4.addr = cfg_dns_;
    (void)esp_netif_set_dns_info(netif_, ESP_NETIF_DNS_MAIN, &dns);
    fmt_ip4(ip_, sizeof(ip_), cfg_ip_);
    return true;
}

bool EthLan8720::apply_now() {
    if (http_) {
        http_->stop();
    }
    if (!apply_static_ip()) {
        return false;
    }
    if (link_up_) {
        got_ip_ = true;
    }
    if (http_ && link_up_) {
        http_->start();
    }
    return true;
}

void EthLan8720::load() {
    nvs_handle_t h{};
    if (nvs_open(kNvsNs, NVS_READONLY, &h) != ESP_OK) {
        return;
    }
    uint32_t v = 0;
    if (nvs_get_u32(h, kKeyIp, &v) == ESP_OK && v != 0 && v != 0xFFFFFFFFu) {
        cfg_ip_ = v;
    }
    if (nvs_get_u32(h, kKeyMask, &v) == ESP_OK && v != 0) {
        cfg_mask_ = v;
    }
    if (nvs_get_u32(h, kKeyGw, &v) == ESP_OK) {
        cfg_gw_ = v;
    }
    if (nvs_get_u32(h, kKeyDns, &v) == ESP_OK) {
        cfg_dns_ = v;
    }
    nvs_close(h);
}

void EthLan8720::save() {
    nvs_handle_t h{};
    if (nvs_open(kNvsNs, NVS_READWRITE, &h) != ESP_OK) {
        return;
    }
    (void)nvs_set_u32(h, kKeyIp, cfg_ip_);
    (void)nvs_set_u32(h, kKeyMask, cfg_mask_);
    (void)nvs_set_u32(h, kKeyGw, cfg_gw_);
    (void)nvs_set_u32(h, kKeyDns, cfg_dns_);
    (void)nvs_commit(h);
    nvs_close(h);
}

bool EthLan8720::set_ip(uint32_t addr) {
    if (addr == 0 || addr == 0xFFFFFFFFu) {
        return false;
    }
    cfg_ip_ = addr;
    if (!apply_now()) {
        return false;
    }
    save();
    return true;
}

bool EthLan8720::set_mask(uint32_t mask) {
    if (mask == 0) {
        return false;
    }
    cfg_mask_ = mask;
    if (!apply_now()) {
        return false;
    }
    save();
    return true;
}

bool EthLan8720::set_gw(uint32_t gw) {
    cfg_gw_ = gw;
    cfg_dns_ = gw;
    if (!apply_now()) {
        return false;
    }
    save();
    return true;
}

void EthLan8720::print() const {
    char ip[16] = {};
    char mask[16] = {};
    char gw[16] = {};
    fmt_ip4(ip, sizeof(ip), cfg_ip_);
    fmt_ip4(mask, sizeof(mask), cfg_mask_);
    fmt_ip4(gw, sizeof(gw), cfg_gw_);
    printf("ip=%s mask=%s gw=%s live=%s\n", ip, mask, gw, ip_);
}

void EthLan8720::ip_text(char *buf, size_t len) const {
    snprintf(buf, len, "%s", ip_);
}

void EthLan8720::mask_text(char *buf, size_t len) const {
    fmt_ip4(buf, len, cfg_mask_);
}

void EthLan8720::gw_text(char *buf, size_t len) const {
    fmt_ip4(buf, len, cfg_gw_);
}

void EthLan8720::mac_text(char *buf, size_t len) const {
    snprintf(buf, len, "%02X:%02X:%02X:%02X:%02X:%02X", mac_[0], mac_[1], mac_[2], mac_[3], mac_[4],
             mac_[5]);
}

void EthLan8720::reset() {
    if (!handle_) {
        return;
    }
    ESP_LOGW(TAG, "PHY power cycle");
    link_up_ = false;
    got_ip_ = false;
    snprintf(ip_, sizeof(ip_), "0.0.0.0");
    if (http_) {
        http_->stop();
    }
    (void)esp_eth_stop(handle_);
    phy_power(false);
    vTaskDelay(pdMS_TO_TICKS(100));
    phy_power(true);
    vTaskDelay(pdMS_TO_TICKS(200));
    if (esp_eth_start(handle_) != ESP_OK) {
        ESP_LOGE(TAG, "esp_eth_start after reset failed");
    }
}

void EthLan8720::on_eth_event(int32_t event_id, void *event_data) {
    switch (event_id) {
        case ETHERNET_EVENT_CONNECTED: {
            link_up_ = true;
            auto handle = *static_cast<esp_eth_handle_t *>(event_data);
            (void)esp_eth_ioctl(handle, ETH_CMD_G_MAC_ADDR, mac_);
            ESP_LOGI(TAG, "ETH link up, MAC %02X:%02X:%02X:%02X:%02X:%02X", mac_[0], mac_[1], mac_[2],
                     mac_[3], mac_[4], mac_[5]);
            break;
        }
        case ETHERNET_EVENT_DISCONNECTED:
            link_up_ = false;
            got_ip_ = false;
            snprintf(ip_, sizeof(ip_), "0.0.0.0");
            if (http_) {
                http_->stop();
            }
            ESP_LOGI(TAG, "ETH link down");
            break;
        default:
            break;
    }
}

void EthLan8720::on_got_ip(const ip_event_got_ip_t *event) {
    got_ip_ = true;
    snprintf(ip_, sizeof(ip_), IPSTR, IP2STR(&event->ip_info.ip));
    ESP_LOGI(TAG, "IP %s", ip_);
    if (http_) {
        http_->start();
    }
}

void EthLan8720::eth_event(void *arg, esp_event_base_t, int32_t id, void *data) {
    static_cast<EthLan8720 *>(arg)->on_eth_event(id, data);
}

void EthLan8720::got_ip_event(void *arg, esp_event_base_t, int32_t, void *data) {
    static_cast<EthLan8720 *>(arg)->on_got_ip(static_cast<ip_event_got_ip_t *>(data));
}

void EthLan8720::begin(HttpStatus *http) {
    http_ = http;
    cfg_ip_ = ETH_TO_IP4(ETH01_IP4_ADDR);
    cfg_mask_ = ETH_TO_IP4(ETH01_MASK_ADDR);
    cfg_gw_ = ETH_TO_IP4(ETH01_GW4_ADDR);
    cfg_dns_ = ETH_TO_IP4(ETH01_DNS4_ADDR);
    load();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    gpio_config_t pwr = {};
    pwr.pin_bit_mask = 1ULL << PIN_ETH_PHY_POWER;
    pwr.mode = GPIO_MODE_OUTPUT;
    ESP_ERROR_CHECK(gpio_config(&pwr));
    phy_power(false);
    vTaskDelay(pdMS_TO_TICKS(100));
    phy_power(true);
    vTaskDelay(pdMS_TO_TICKS(200));

    esp_netif_config_t netif_cfg = ESP_NETIF_DEFAULT_ETH();
    netif_ = esp_netif_new(&netif_cfg);
    ESP_ERROR_CHECK(esp_netif_set_hostname(netif_, "esp32-eth01"));
    if (!apply_static_ip()) {
        ESP_LOGE(TAG, "static IP failed");
        return;
    }

    eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
    eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();
    phy_config.phy_addr = ETH_PHY_ADDR;
    phy_config.reset_gpio_num = -1;

    eth_esp32_emac_config_t emac_config = ETH_ESP32_EMAC_DEFAULT_CONFIG();
    emac_config.smi_gpio.mdc_num = PIN_ETH_MDC;
    emac_config.smi_gpio.mdio_num = PIN_ETH_MDIO;
    emac_config.clock_config.rmii.clock_mode = EMAC_CLK_EXT_IN;
    emac_config.clock_config.rmii.clock_gpio = EMAC_CLK_IN_GPIO;

    esp_eth_mac_t *mac = esp_eth_mac_new_esp32(&emac_config, &mac_config);
    esp_eth_phy_t *phy = esp_eth_phy_new_lan87xx(&phy_config);
    if (!mac || !phy) {
        ESP_LOGE(TAG, "LAN8720 MAC/PHY create failed");
        return;
    }

    esp_eth_config_t config = ETH_DEFAULT_CONFIG(mac, phy);
    ESP_ERROR_CHECK(esp_eth_driver_install(&config, &handle_));

    ESP_ERROR_CHECK(esp_read_mac(mac_, ESP_MAC_ETH));
    ESP_ERROR_CHECK(esp_eth_ioctl(handle_, ETH_CMD_S_MAC_ADDR, mac_));
    ESP_ERROR_CHECK(esp_netif_attach(netif_, esp_eth_new_netif_glue(handle_)));
    ESP_ERROR_CHECK(esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, &eth_event, this));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, &got_ip_event, this));
    ESP_ERROR_CHECK(esp_eth_start(handle_));
}
