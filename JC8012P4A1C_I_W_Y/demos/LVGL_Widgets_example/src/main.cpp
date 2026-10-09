/*******************************************************************************
Created by profi-max (Oleg Linnik) 2026
https://profimaxblog.ru
https://github.com/profi-max

*******************************************************************************/
#pragma GCC push_options
#pragma GCC optimize("O3")

#include <Arduino.h>
#include "lvgl.h"
#include "bsp_JC8012P4A1.h"
#include "lvgl_port_v9.h"

#include "demos/lv_demos.h"

#define TAG  "main"  

void setup()
{
    Serial.begin(115200);
    ESP_LOGI(TAG, "MY ESP32P4 MIPI DSI LVGL");

    psramInit();
    ESP_LOGI(TAG,"Memory available in PSRAM : %d\n", ESP.getFreePsram());


    bsp_lvgl_init();

    /* Lock the mutex due to the LVGL APIs are not thread-safe */
    lvgl_port_lock(-1);

    // * Or try out a demo. Don't forget to enable the demos in lv_conf.h. E.g. LV_USE_DEMO_WIDGETS
     lv_demo_widgets();
    // lv_demo_music();        
    // lv_demo_stress();      
    //lv_demo_benchmark();    

    /* Release the mutex */
    lvgl_port_unlock();

}

void loop()
{   
  vTaskSuspend(NULL); // // let arduino task sleep (suspend itself)
}