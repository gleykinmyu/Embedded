#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "chase";

/* DevKitC-1: обычные GPIO, не strapping и не USB/UART. */
static const gpio_num_t kPins[] = {
    GPIO_NUM_4,
    GPIO_NUM_5,
    GPIO_NUM_6,
    GPIO_NUM_7,
    GPIO_NUM_15,
    GPIO_NUM_16,
    GPIO_NUM_17,
    GPIO_NUM_18,
};

static const int kPinCount = sizeof(kPins) / sizeof(kPins[0]);
static const int kStepMs = 150;

static void pins_init(void)
{
    uint64_t mask = 0;
    for (int i = 0; i < kPinCount; ++i) {
        mask |= 1ULL << kPins[i];
    }

    gpio_config_t cfg = {
        .pin_bit_mask = mask,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&cfg);

    for (int i = 0; i < kPinCount; ++i) {
        gpio_set_level(kPins[i], 0);
    }
}

void app_main(void)
{
    pins_init();

    int index = 0;
    while (1) {
        for (int i = 0; i < kPinCount; ++i) {
            gpio_set_level(kPins[i], i == index);
        }

        ESP_LOGI(TAG, "GPIO%d", kPins[index]);
        index = (index + 1) % kPinCount;
        vTaskDelay(pdMS_TO_TICKS(kStepMs));
    }
}
