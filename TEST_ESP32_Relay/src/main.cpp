#include "eth_w5500.h"
#include "http_ui.h"
#include "nvs_flash.h"
#include "relay_board.h"
#include "uart_cli.h"

extern "C" void app_main(void) {
    esp_err_t nvs = nvs_flash_init();
    if (nvs == ESP_ERR_NVS_NO_FREE_PAGES || nvs == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        nvs = nvs_flash_init();
    }
    ESP_ERROR_CHECK(nvs);

    static RelayBoard relays;
    static EthW5500 eth;
    static HttpUi http(relays, eth);
    static UartCli cli(relays, eth, http);

    relays.begin();
    eth.begin(&http);
    cli.begin();
}
