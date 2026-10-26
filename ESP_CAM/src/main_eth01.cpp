#include "cli_eth01.h"
#include "eth_lan8720.h"
#include "http_status.h"
#include "nvs_flash.h"
#include "ph350_ctrl.h"

extern "C" void app_main(void) {
    esp_err_t nvs = nvs_flash_init();
    if (nvs == ESP_ERR_NVS_NO_FREE_PAGES || nvs == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        nvs = nvs_flash_init();
    }
    ESP_ERROR_CHECK(nvs);

    static Ph350Ctrl pt;
    static EthLan8720 eth;
    static HttpStatus http(eth, pt);
    static CliEth01 cli(eth, http, pt);

    ESP_ERROR_CHECK(pt.begin() ? ESP_OK : ESP_FAIL);
    eth.begin(&http);
    cli.begin();
}
