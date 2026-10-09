/*******************************************************************************
Created by profi-max (Oleg Linnik) 2026
https://profimaxblog.ru
https://github.com/profi-max

*******************************************************************************/
#pragma once

#include <stdint.h>

#include "esp_err.h"
#include "esp_lcd_types.h"
#include "touch/esp_lcd_touch.h"
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

//#define LCD_H_RES 1280
//#define LCD_V_RES 800

#define LCD_RST GPIO_NUM_27
#define LCD_LED GPIO_NUM_23

#define TP_RST GPIO_NUM_22
#define TP_INT GPIO_NUM_21

#define I2C_SDA GPIO_NUM_7
#define I2C_SCL GPIO_NUM_8

#define SD_CLK GPIO_NUM_43
#define SD_CMD GPIO_NUM_44
#define SD_DATA0 GPIO_NUM_39
#define SD_DATA1 GPIO_NUM_40
#define SD_DATA2 GPIO_NUM_41
#define SD_DATA3 GPIO_NUM_42

#define CODEC_I2S0_MCLK GPIO_NUM_13 // is I2S_MCLK
#define CODEC_I2S0_SCLK GPIO_NUM_12 // is I2S_BCLK
#define CODEC_I2S0_SDOUT GPIO_NUM_11
#define CODEC_I2S0_LRCK GPIO_NUM_10 // is I2S_LRCK
#define CODEC_I2S0_DSDIN GPIO_NUM_9  // is I2S_DOUT

void bsp_lvgl_init(void);

esp_err_t bsp_lcd_brightness_set(uint8_t brightness_percent);


#ifdef __cplusplus
}
#endif