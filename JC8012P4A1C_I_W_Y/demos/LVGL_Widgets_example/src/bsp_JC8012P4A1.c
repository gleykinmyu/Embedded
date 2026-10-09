/*******************************************************************************
Created by profi-max (Oleg Linnik) 2026
https://profimaxblog.ru
https://github.com/profi-max

*******************************************************************************/
#include <Arduino.h>
#include "lvgl.h"
#include "esp_timer.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_mipi_dsi.h"
#include "esp_lcd_panel_io.h"
#include "esp_ldo_regulator.h"
#include "esp_log.h"

#include "driver/i2c.h"
#include "driver/ledc.h"

#include "lcd/esp_lcd_jd9365.h"
#include "touch/esp_lcd_touch.h"
#include "touch/esp_lcd_gsl3680.h"

#include "bsp_JC8012P4A1.h"
#include "lvgl_port_v9.h"


static const char *TAG = "lvgl_init";

#define MIPI_DPI_PX_FORMAT (LCD_COLOR_PIXEL_FORMAT_RGB565)
#define LCD_BIT_PER_PIXEL (16)

//“VDD_MIPI_DPHY” must be 2.5V，it can be from internal or external LDO 
#define MIPI_DSI_PHY_PWR_LDO_CHAN 3 // LDO_VO3 connected to VDD_MIPI_DPHY
#define MIPI_DSI_PHY_PWR_LDO_VOLTAGE_MV 2500

#define LCD_BK_LIGHT_LEDC_CH         LEDC_CHANNEL_6

//==========================================================================================================================
esp_err_t bsp_lcd_brightness_init(void)
{
    // Setup LEDC peripheral for PWM backlight control
    const ledc_channel_config_t LCD_backlight_channel = {
        .gpio_num = LCD_LED,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LCD_BK_LIGHT_LEDC_CH,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = 1,
        .duty = 0,
        .hpoint = 0
    };
    const ledc_timer_config_t LCD_backlight_timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_10_BIT,
        .timer_num = 1,
        .freq_hz = 20000,
        .clk_cfg = LEDC_AUTO_CLK
    };

    ESP_ERROR_CHECK(ledc_timer_config(&LCD_backlight_timer));
    ESP_ERROR_CHECK(ledc_channel_config(&LCD_backlight_channel));
    return ESP_OK;
}

//==========================================================================================================================
esp_err_t bsp_lcd_brightness_set(uint8_t brightness_percent)
{
    if (brightness_percent > 100) {
        brightness_percent = 100;
    }

    ESP_LOGI(TAG, "Setting LCD backlight: %d%%", brightness_percent);
    uint32_t duty_cycle = (1023 * brightness_percent) / 100; // LEDC resolution set to 10bits, thus: 100% = 1023
    ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, LCD_BK_LIGHT_LEDC_CH, duty_cycle));
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, LCD_BK_LIGHT_LEDC_CH));
    return ESP_OK;
}
//==========================================================================================================================
void bsp_lvgl_init(void)
{
    esp_lcd_panel_handle_t panel_handle = NULL;
    esp_lcd_touch_handle_t tp_handle = NULL;
   
    bsp_lcd_brightness_init();  //  Attach backlight
    bsp_lcd_brightness_set(0);   // Turn backlight off

    // Power on MIPI DSI PHY
    esp_ldo_channel_handle_t ldo_mipi_phy = NULL;
#ifdef MIPI_DSI_PHY_PWR_LDO_CHAN
    esp_ldo_channel_config_t ldo_mipi_phy_config = {
        .chan_id = MIPI_DSI_PHY_PWR_LDO_CHAN,
        .voltage_mv = MIPI_DSI_PHY_PWR_LDO_VOLTAGE_MV,
    };
    ESP_ERROR_CHECK(esp_ldo_acquire_channel(&ldo_mipi_phy_config, &ldo_mipi_phy));
    ESP_LOGI(TAG, "MIPI DSI PHY Powered on");
#endif

    // Create MIPI DSI bus, then initialize DSI PHY
    esp_lcd_dsi_bus_handle_t mipi_dsi_bus;
    esp_lcd_dsi_bus_config_t bus_config = JD9365_PANEL_BUS_DSI_2CH_CONFIG();
    ESP_ERROR_CHECK(esp_lcd_new_dsi_bus(&bus_config, &mipi_dsi_bus));

    ESP_LOGI(TAG, "Install MIPI DSI LCD control panel");
    // We use DBI interface to control LCD 
    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_dbi_io_config_t dbi_config = JD9365_PANEL_IO_DBI_CONFIG();
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_dbi(mipi_dsi_bus, &dbi_config, &io_handle));

    // Create panel  JD9365
    esp_lcd_dpi_panel_config_t dpi_config = JD9365_800_1280_PANEL_60HZ_DPI_CONFIG(MIPI_DPI_PX_FORMAT);

    jd9365_vendor_config_t vendor_config = {
    .mipi_config = {
            .dsi_bus = mipi_dsi_bus,
            .dpi_config = &dpi_config,
            .lane_num = 2,      //   MMMMMMYYYYYY
        },
    };
    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = LCD_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = LCD_BIT_PER_PIXEL,
        .vendor_config = &vendor_config,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_jd9365(io_handle, &panel_config, &panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
//    ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel_handle,true,true));
 
    // Init gsl3680 touch
       i2c_config_t i2c_conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_SDA,
        .scl_io_num = I2C_SCL,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
    };
    i2c_conf.master.clk_speed = 400000; // 400kHz

    ESP_ERROR_CHECK(i2c_param_config(I2C_NUM_0, &i2c_conf));
    ESP_ERROR_CHECK(i2c_driver_install(I2C_NUM_0, i2c_conf.mode, 0, 0, 0));

    ESP_LOGI(TAG, "Initialize touch IO (I2C)");
    esp_lcd_panel_io_i2c_config_t tp_io_config = ESP_LCD_TOUCH_IO_I2C_GSL3680_CONFIG();
    esp_lcd_panel_io_handle_t tp_io_handle = NULL;
    esp_lcd_new_panel_io_i2c((esp_lcd_i2c_bus_handle_t)I2C_NUM_0, &tp_io_config, &tp_io_handle);

    esp_lcd_touch_config_t tp_cfg = {
        .x_max = LVGL_PORT_H_RES,
        .y_max = LVGL_PORT_V_RES,
        .rst_gpio_num = TP_RST,
        .int_gpio_num = TP_INT,
        .levels = {
            .reset = 0,
            .interrupt = 0,
        },
        .flags = {
            .swap_xy = 0,
            .mirror_x = 0,
            .mirror_y = 1,
        },
    };

    ESP_LOGI(TAG, "Initialize touch controller gsl3680");
    ESP_ERROR_CHECK(esp_lcd_touch_new_i2c_gsl3680(tp_io_handle, &tp_cfg, &tp_handle));

    lvgl_port_interface_t interface = (dpi_config.flags.use_dma2d) ? LVGL_PORT_INTERFACE_MIPI_DSI_DMA : LVGL_PORT_INTERFACE_MIPI_DSI_NO_DMA;
    lvgl_port_init(panel_handle, tp_handle, interface);
    
    bsp_lcd_brightness_set(100); // Turn backlight on
}
//==========================================================================================================================



//==========================================================================================================================
//==========================================================================================================================
//==========================================================================================================================
//==========================================================================================================================
//==========================================================================================================================
//==========================================================================================================================
//==========================================================================================================================
//==========================================================================================================================
//==========================================================================================================================
