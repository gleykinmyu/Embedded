#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "board_pins.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "driver/spi_master.h"
#include "esp_eth.h"
#include "esp_eth_mac_spi.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "nvs_flash.h"

static const char *TAG = "relay";

static i2c_master_dev_handle_t tca_dev;
static esp_netif_t *eth_netif;
static SemaphoreHandle_t relay_mu;
static TaskHandle_t chase_task_handle;

static uint8_t relay_shadow = 0x00;
static size_t chase_ch = 0;
static bool chase_running = true;

static bool tca_write(uint8_t reg, uint8_t value) {
    uint8_t data[2] = {reg, value};
    return i2c_master_transmit(tca_dev, data, sizeof(data), 100) == ESP_OK;
}

static void relays_all_off_locked() {
    relay_shadow = 0x00;
    tca_write(TCA_REG_OUTPUT, relay_shadow);
}

static void relay_on_locked(size_t ch) {
    relay_shadow |= static_cast<uint8_t>(1u << PIN_RELAY[ch]);
    tca_write(TCA_REG_OUTPUT, relay_shadow);
}

static void relay_off_locked(size_t ch) {
    relay_shadow &= static_cast<uint8_t>(~(1u << PIN_RELAY[ch]));
    tca_write(TCA_REG_OUTPUT, relay_shadow);
}

static void relays_all_off() {
    xSemaphoreTake(relay_mu, portMAX_DELAY);
    relays_all_off_locked();
    xSemaphoreGive(relay_mu);
}

static bool relay_is_on(size_t ch) {
    return (relay_shadow & static_cast<uint8_t>(1u << PIN_RELAY[ch])) != 0;
}

static void fill_json(char *buf, size_t len) {
    char ip[16] = "0.0.0.0";
    if (eth_netif) {
        esp_netif_ip_info_t info;
        if (esp_netif_get_ip_info(eth_netif, &info) == ESP_OK) {
            snprintf(ip, sizeof(ip), IPSTR, IP2STR(&info.ip));
        }
    }

    xSemaphoreTake(relay_mu, portMAX_DELAY);
    int n = snprintf(buf, len, "{\"ip\":\"%s\",\"running\":%s,\"relays\":[",
                     ip, chase_running ? "true" : "false");
    for (size_t ch = 0; ch < RELAY_COUNT && n > 0 && static_cast<size_t>(n) < len; ++ch) {
        n += snprintf(buf + n, len - static_cast<size_t>(n), "%s%d", ch ? "," : "",
                      relay_is_on(ch) ? 1 : 0);
    }
    xSemaphoreGive(relay_mu);
    if (n > 0 && static_cast<size_t>(n) < len) {
        snprintf(buf + n, len - static_cast<size_t>(n), "]}");
    }
}

extern const char index_html_start[] asm("_binary_index_html_start");
extern const char index_html_end[] asm("_binary_index_html_end");

static esp_err_t send_json(httpd_req_t *req) {
    char buf[192];
    fill_json(buf, sizeof(buf));
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_send(req, buf, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t handle_index(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_send(req, index_html_start, index_html_end - index_html_start - 1);
}

static esp_err_t handle_relays(httpd_req_t *req) {
    return send_json(req);
}

static esp_err_t handle_chase_start(httpd_req_t *req) {
    xSemaphoreTake(relay_mu, portMAX_DELAY);
    chase_running = true;
    xSemaphoreGive(relay_mu);
    if (chase_task_handle) {
        xTaskNotifyGive(chase_task_handle);
    }
    return send_json(req);
}

static esp_err_t handle_chase_stop(httpd_req_t *req) {
    xSemaphoreTake(relay_mu, portMAX_DELAY);
    chase_running = false;
    relays_all_off_locked();
    xSemaphoreGive(relay_mu);
    return send_json(req);
}

static esp_err_t handle_relay_set(httpd_req_t *req) {
    char query[48];
    char ch_s[8];
    char on_s[8];
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) != ESP_OK ||
        httpd_query_key_value(query, "ch", ch_s, sizeof(ch_s)) != ESP_OK ||
        httpd_query_key_value(query, "on", on_s, sizeof(on_s)) != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "need ch,on");
        return ESP_FAIL;
    }

    const int ch = atoi(ch_s);
    const int on = atoi(on_s);
    if (ch < 0 || static_cast<size_t>(ch) >= RELAY_COUNT || (on != 0 && on != 1)) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "bad ch,on");
        return ESP_FAIL;
    }

    xSemaphoreTake(relay_mu, portMAX_DELAY);
    chase_running = false;
    if (on) {
        relay_on_locked(static_cast<size_t>(ch));
    } else {
        relay_off_locked(static_cast<size_t>(ch));
    }
    xSemaphoreGive(relay_mu);

    ESP_LOGI(TAG, "CH%u %s", static_cast<unsigned>(ch + 1), on ? "ON" : "OFF");
    return send_json(req);
}

static void chase_task(void *arg) {
    (void)arg;
    while (true) {
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(RELAY_STEP_MS));

        xSemaphoreTake(relay_mu, portMAX_DELAY);
        const bool run = chase_running;
        const size_t ch = chase_ch;
        if (run) {
            chase_ch = (chase_ch + 1) % RELAY_COUNT;
            relays_all_off_locked();
            relay_on_locked(ch);
        }
        xSemaphoreGive(relay_mu);

        if (run) {
            ESP_LOGI(TAG, "CH%u ON (EXIO%u)", static_cast<unsigned>(ch + 1),
                     static_cast<unsigned>(PIN_RELAY[ch] + 1));
        }
    }
}

static void eth_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id,
                              void *event_data) {
    (void)arg;
    (void)event_base;
    switch (event_id) {
        case ETHERNET_EVENT_CONNECTED: {
            uint8_t mac[6] = {};
            esp_eth_handle_t handle = *static_cast<esp_eth_handle_t *>(event_data);
            esp_eth_ioctl(handle, ETH_CMD_G_MAC_ADDR, mac);
            ESP_LOGI(TAG, "ETH link up, MAC %02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1],
                     mac[2], mac[3], mac[4], mac[5]);
            break;
        }
        case ETHERNET_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "ETH link down");
            break;
        default:
            break;
    }
}

static void got_ip_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id,
                                 void *event_data) {
    (void)arg;
    (void)event_base;
    (void)event_id;
    const ip_event_got_ip_t *event = static_cast<ip_event_got_ip_t *>(event_data);
    ESP_LOGI(TAG, "HTTP http://" IPSTR, IP2STR(&event->ip_info.ip));
}

#define ETH_TO_IP4(octets) ESP_IP4TOADDR(octets)

static void apply_static_ip(esp_netif_t *netif) {
    ESP_ERROR_CHECK(esp_netif_dhcpc_stop(netif));
    esp_netif_ip_info_t ip_info = {};
    ip_info.ip.addr = ETH_TO_IP4(ETH_IP4_ADDR);
    ip_info.gw.addr = ETH_TO_IP4(ETH_GW4_ADDR);
    ip_info.netmask.addr = ETH_TO_IP4(ETH_MASK_ADDR);
    ESP_ERROR_CHECK(esp_netif_set_ip_info(netif, &ip_info));

    esp_netif_dns_info_t dns = {};
    dns.ip.type = ESP_IPADDR_TYPE_V4;
    dns.ip.u_addr.ip4.addr = ETH_TO_IP4(ETH_DNS4_ADDR);
    ESP_ERROR_CHECK(esp_netif_set_dns_info(netif, ESP_NETIF_DNS_MAIN, &dns));
}

static void eth_begin() {
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(gpio_install_isr_service(0));

    esp_netif_config_t netif_cfg = {};
    netif_cfg.base = ESP_NETIF_BASE_DEFAULT_ETH;
    netif_cfg.stack = ESP_NETIF_NETSTACK_DEFAULT_ETH;
    eth_netif = esp_netif_new(&netif_cfg);
    ESP_ERROR_CHECK(esp_netif_set_hostname(eth_netif, "esp32-relay"));
    apply_static_ip(eth_netif);

    spi_bus_config_t buscfg = {};
    buscfg.mosi_io_num = PIN_ETH_MOSI;
    buscfg.miso_io_num = PIN_ETH_MISO;
    buscfg.sclk_io_num = PIN_ETH_SCLK;
    buscfg.quadwp_io_num = -1;
    buscfg.quadhd_io_num = -1;
    ESP_ERROR_CHECK(spi_bus_initialize(SPI3_HOST, &buscfg, SPI_DMA_CH_AUTO));

    spi_device_interface_config_t spi_devcfg = {};
    spi_devcfg.mode = 0;
    spi_devcfg.clock_speed_hz = 20 * 1000 * 1000;
    spi_devcfg.spics_io_num = PIN_ETH_CS;
    spi_devcfg.queue_size = 20;

    eth_w5500_config_t w5500_config = {};
    w5500_config.int_gpio_num = PIN_ETH_INT;
    w5500_config.spi_host_id = SPI3_HOST;
    w5500_config.spi_devcfg = &spi_devcfg;

    eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
    eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();
    phy_config.phy_addr = 1;
    phy_config.reset_gpio_num = -1;

    esp_eth_mac_t *mac = esp_eth_mac_new_w5500(&w5500_config, &mac_config);
    esp_eth_phy_t *phy = esp_eth_phy_new_w5500(&phy_config);
    if (!mac || !phy) {
        ESP_LOGE(TAG, "W5500 MAC/PHY create failed");
        return;
    }

    esp_eth_config_t config = ETH_DEFAULT_CONFIG(mac, phy);
    esp_eth_handle_t eth_handle = nullptr;
    ESP_ERROR_CHECK(esp_eth_driver_install(&config, &eth_handle));

    uint8_t eth_mac[6] = {};
    ESP_ERROR_CHECK(esp_read_mac(eth_mac, ESP_MAC_ETH));
    ESP_ERROR_CHECK(esp_eth_ioctl(eth_handle, ETH_CMD_S_MAC_ADDR, eth_mac));

    ESP_ERROR_CHECK(esp_netif_attach(eth_netif, esp_eth_new_netif_glue(eth_handle)));

    ESP_ERROR_CHECK(esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, &eth_event_handler, nullptr));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, &got_ip_event_handler, nullptr));
    ESP_ERROR_CHECK(esp_eth_start(eth_handle));
}

static void http_begin() {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.lru_purge_enable = true;
    httpd_handle_t server = nullptr;
    ESP_ERROR_CHECK(httpd_start(&server, &config));

    httpd_uri_t route = {};
    route.method = HTTP_GET;

    route.uri = "/";
    route.handler = handle_index;
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &route));
    route.uri = "/api/relays";
    route.handler = handle_relays;
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &route));
    route.uri = "/api/chase/start";
    route.handler = handle_chase_start;
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &route));
    route.uri = "/api/chase/stop";
    route.handler = handle_chase_stop;
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &route));
    route.uri = "/api/relay";
    route.handler = handle_relay_set;
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &route));
}

static void i2c_begin() {
    i2c_master_bus_config_t bus_cfg = {};
    bus_cfg.i2c_port = I2C_NUM_0;
    bus_cfg.sda_io_num = static_cast<gpio_num_t>(PIN_I2C_SDA);
    bus_cfg.scl_io_num = static_cast<gpio_num_t>(PIN_I2C_SCL);
    bus_cfg.clk_source = I2C_CLK_SRC_DEFAULT;
    bus_cfg.glitch_ignore_cnt = 7;
    bus_cfg.flags.enable_internal_pullup = true;

    i2c_master_bus_handle_t bus = nullptr;
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_cfg, &bus));

    if (i2c_master_probe(bus, TCA9554_ADDR, 100) != ESP_OK) {
        ESP_LOGE(TAG, "TCA9554 not found at 0x%02X", TCA9554_ADDR);
    }

    i2c_device_config_t dev_cfg = {};
    dev_cfg.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    dev_cfg.device_address = TCA9554_ADDR;
    dev_cfg.scl_speed_hz = 100000;
    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus, &dev_cfg, &tca_dev));

    tca_write(TCA_REG_CONFIG, 0x00);
    relays_all_off_locked();
}

extern "C" void app_main(void) {
    esp_err_t nvs = nvs_flash_init();
    if (nvs == ESP_ERR_NVS_NO_FREE_PAGES || nvs == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        nvs = nvs_flash_init();
    }
    ESP_ERROR_CHECK(nvs);

    relay_mu = xSemaphoreCreateMutex();

    gpio_config_t buzzer = {};
    buzzer.pin_bit_mask = 1ULL << PIN_BUZZER;
    buzzer.mode = GPIO_MODE_OUTPUT;
    gpio_config(&buzzer);
    gpio_set_level(static_cast<gpio_num_t>(PIN_BUZZER), 0);

    i2c_begin();
    eth_begin();
    http_begin();

    xTaskCreate(chase_task, "chase", 4096, nullptr, 5, &chase_task_handle);

    ESP_LOGI(TAG, "ESP32-S3-POE-ETH-8DI-8RO relay chase + HTTP (ESP-IDF)");
    ESP_LOGI(TAG, "Interval: %u ms, HIGH = ON", static_cast<unsigned>(RELAY_STEP_MS));
}
