#include "cli_eth01.h"
#include "eth_lan8720.h"
#include "http_status.h"
#include "nvs_flash.h"
#include "ota_espota.h"
#include "ph350_ctrl.h"

#include "board_eth01.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/inet.h"
#include "nvs.h"

static const char *TAG_BOOT = "boot";

namespace {

bool factory_rst_held()
{
    gpio_config_t io = {};
    io.pin_bit_mask = 1ULL << PIN_FACTORY_RST;
    io.mode = GPIO_MODE_INPUT;
    io.pull_up_en = GPIO_PULLUP_ENABLE;
    io.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io.intr_type = GPIO_INTR_DISABLE;
    if (gpio_config(&io) != ESP_OK) {
        return false;
    }
    /* антидребезг: только если стабильно к земле */
    int low = 0;
    for (int i = 0; i < 10; ++i) {
        if (gpio_get_level(static_cast<gpio_num_t>(PIN_FACTORY_RST)) == 0) {
            ++low;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    return low >= 8;
}

void erase_nvs_ns(const char *ns)
{
    nvs_handle_t h{};
    if (nvs_open(ns, NVS_READWRITE, &h) != ESP_OK) {
        return;
    }
    (void)nvs_erase_all(h);
    (void)nvs_commit(h);
    nvs_close(h);
}

void write_factory_eth_nvs()
{
    nvs_handle_t h{};
    if (nvs_open("eth01", NVS_READWRITE, &h) != ESP_OK) {
        ESP_LOGW(TAG_BOOT, "eth01 nvs open fail");
        return;
    }
#define ETH_TO_IP4(octets) ESP_IP4TOADDR(octets)
    const uint32_t ip = ETH_TO_IP4(ETH01_IP4_ADDR);
    const uint32_t mask = ETH_TO_IP4(ETH01_MASK_ADDR);
    const uint32_t gw = ETH_TO_IP4(ETH01_GW4_ADDR);
    const uint32_t dns = ETH_TO_IP4(ETH01_DNS4_ADDR);
#undef ETH_TO_IP4
    (void)nvs_set_u32(h, "ip", ip);
    (void)nvs_set_u32(h, "mask", mask);
    (void)nvs_set_u32(h, "gw", gw);
    (void)nvs_set_u32(h, "dns", dns);
    (void)nvs_commit(h);
    nvs_close(h);
}

void maybe_factory_reset()
{
    if (!factory_rst_held()) {
        return;
    }
    ESP_LOGW(TAG_BOOT, "GPIO%d LOW → factory defaults (IP/mask/auth/models)", PIN_FACTORY_RST);
    erase_nvs_ns("eth01");
    erase_nvs_ns("auth");
    erase_nvs_ns("ph350");
    write_factory_eth_nvs();
    /* auth: пустой NVS → admin/admin при HttpAuth::begin */
    /* ph350: пустой NVS → Ph350 + E650, rate/invert по коду */
}

} // namespace

extern "C" void app_main(void) {
    esp_err_t nvs = nvs_flash_init();
    if (nvs == ESP_ERR_NVS_NO_FREE_PAGES || nvs == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        nvs = nvs_flash_init();
    }
    ESP_ERROR_CHECK(nvs);

    maybe_factory_reset();

    static Ph350Ctrl pt;
    static EthLan8720 eth;
    static HttpStatus http(eth, pt);
    static CliEth01 cli(eth, http, pt);
    static OtaEspota ota;

    ESP_ERROR_CHECK(pt.begin() ? ESP_OK : ESP_FAIL);
    eth.begin(&http);
    ota.begin();
    cli.begin();
}
