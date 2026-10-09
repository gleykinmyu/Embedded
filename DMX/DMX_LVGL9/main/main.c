/*
 * LVGL 9 UI on Waveshare ESP32-S3-Touch-LCD-7 (ESP-IDF / PlatformIO).
 * Display/touch port from Waveshare 09_lvgl_v9_demo.
 * UI is built with LVGL Editor XML in components/ui.
 */

#include <assert.h>

#include "esp_log.h"
#include "esp_lv_adapter.h"
#include "lvgl.h"
#include "ui.h"
#include "waveshare_rgb_lcd_port.h"

static const char *TAG = "dmx_lvgl9";

void app_main(void)
{
    const esp_lv_adapter_rotation_t rotation = ESP_LV_ADAPTER_ROTATE_0;
    const esp_lv_adapter_tear_avoid_mode_t tear_mode = ESP_LV_ADAPTER_TEAR_AVOID_MODE_DEFAULT_RGB;

    esp_lcd_panel_handle_t panel_handle = NULL;
    esp_lcd_touch_handle_t touch_handle = NULL;

    ESP_ERROR_CHECK(waveshare_esp32_s3_rgb_lcd_init(
        tear_mode,
        rotation,
        &panel_handle,
        &touch_handle));
    ESP_ERROR_CHECK(waveshare_rgb_lcd_backlight_on());

    esp_lv_adapter_config_t adapter_config = ESP_LV_ADAPTER_DEFAULT_CONFIG();
    adapter_config.task_stack_size = 12 * 1024;
    adapter_config.stack_in_psram = true;
    ESP_ERROR_CHECK(esp_lv_adapter_init(&adapter_config));

    esp_lv_adapter_display_config_t disp_config = ESP_LV_ADAPTER_DISPLAY_RGB_DEFAULT_CONFIG(
        panel_handle,
        NULL,
        EXAMPLE_LCD_H_RES,
        EXAMPLE_LCD_V_RES,
        rotation);
    disp_config.profile.use_psram = true;

    lv_display_t *disp = esp_lv_adapter_register_display(&disp_config);
    assert(disp != NULL);

    if (touch_handle != NULL) {
        esp_lv_adapter_touch_config_t touch_config = ESP_LV_ADAPTER_TOUCH_DEFAULT_CONFIG(disp, touch_handle);
        lv_indev_t *touch = esp_lv_adapter_register_touch(&touch_config);
        assert(touch != NULL);
    }

    ESP_ERROR_CHECK(esp_lv_adapter_start());

    ESP_LOGI(TAG, "Loading LVGL Editor UI");
    if (esp_lv_adapter_lock(-1) == ESP_OK) {
        ui_init("");
        lv_screen_load(main_screen);
        esp_lv_adapter_unlock();
    }
}
