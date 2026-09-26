#include "driver/gpio.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "led";

/* Встроенный светодиод DEVKIT V1. Активный уровень — высокий. */
static const gpio_num_t kLedPin = GPIO_NUM_2;
static const int kBlinkMs = 500;

static const char *chip_model_name(esp_chip_model_t model)
{
    switch (model) {
    case CHIP_ESP32:
        return "ESP32";
    case CHIP_ESP32S2:
        return "ESP32-S2";
    case CHIP_ESP32S3:
        return "ESP32-S3";
    case CHIP_ESP32C3:
        return "ESP32-C3";
    case CHIP_ESP32C2:
        return "ESP32-C2";
    case CHIP_ESP32C6:
        return "ESP32-C6";
    case CHIP_ESP32H2:
        return "ESP32-H2";
    default:
        return "unknown";
    }
}

static void log_chip(void)
{
    esp_chip_info_t chip;
    esp_chip_info(&chip);

    uint32_t flash_size = 0;
    esp_flash_get_size(NULL, &flash_size);

    uint8_t mac[6] = {0};
    esp_read_mac(mac, ESP_MAC_WIFI_STA);

    ESP_LOGI(TAG, "chip %s, cores %u, revision %u.%u",
             chip_model_name(chip.model),
             chip.cores,
             chip.revision / 100,
             chip.revision % 100);
    ESP_LOGI(TAG, "features wifi %s, bt %s, ble %s, psram %s",
             (chip.features & CHIP_FEATURE_WIFI_BGN) ? "yes" : "no",
             (chip.features & CHIP_FEATURE_BT) ? "yes" : "no",
             (chip.features & CHIP_FEATURE_BLE) ? "yes" : "no",
             (chip.features & CHIP_FEATURE_EMB_PSRAM) ? "yes" : "no");
    ESP_LOGI(TAG, "flash %lu bytes", (unsigned long)flash_size);
    ESP_LOGI(TAG, "mac %02x:%02x:%02x:%02x:%02x:%02x",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    ESP_LOGI(TAG, "idf %s, target %s", esp_get_idf_version(), CONFIG_IDF_TARGET);
}

static void led_init(void)
{
    gpio_config_t cfg = {
        .pin_bit_mask = 1ULL << kLedPin,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&cfg);
    gpio_set_level(kLedPin, 0);
}

void app_main(void)
{
    log_chip();
    led_init();

    int level = 0;
    while (1) {
        level = !level;
        gpio_set_level(kLedPin, level);
        ESP_LOGI(TAG, "GPIO%d %s", kLedPin, level ? "on" : "off");
        vTaskDelay(pdMS_TO_TICKS(kBlinkMs));
    }
}
