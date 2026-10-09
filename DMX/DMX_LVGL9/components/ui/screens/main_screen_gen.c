/**
 * @file main_screen_gen.c
 * @brief Template source file for LVGL objects
 */

/*********************
 *      INCLUDES
 *********************/

#include "main_screen_gen.h"
#include "ui.h"

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/***********************
 *  STATIC VARIABLES
 **********************/

/***********************
 *  STATIC PROTOTYPES
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

lv_obj_t * main_screen_create(void)
{
    LV_TRACE_OBJ_CREATE("begin");

    static lv_style_t style_main;

    static bool style_inited = false;

    if (!style_inited) {
        lv_style_init(&style_main);
        lv_style_set_bg_color(&style_main, lv_color_hex(0x1a1a2e));

        style_inited = true;
    }

    if (main_screen == NULL) main_screen = lv_obj_create(NULL);
    lv_obj_t * lv_obj_0 = main_screen;
    lv_obj_set_name_static(lv_obj_0, "main_screen_#");

    lv_obj_add_style(lv_obj_0, &style_main, 0);
    lv_obj_t * lv_label_0 = lv_label_create(lv_obj_0);
    lv_label_set_text(lv_label_0, "DMX LVGL9");
    lv_obj_set_align(lv_label_0, LV_ALIGN_TOP_MID);
    lv_obj_set_y(lv_label_0, 40);
    lv_obj_set_style_text_color(lv_label_0, lv_color_hex(0xeaeaea), 0);
    
    lv_obj_t * lv_label_1 = lv_label_create(lv_obj_0);
    lv_label_bind_text(lv_label_1, &tap_count, "Count: %d");
    lv_obj_set_align(lv_label_1, LV_ALIGN_CENTER);
    lv_obj_set_y(lv_label_1, -20);
    lv_obj_set_style_text_color(lv_label_1, lv_color_hex(0xa0a0b0), 0);
    
    lv_obj_t * lv_button_0 = lv_button_create(lv_obj_0);
    lv_obj_set_align(lv_button_0, LV_ALIGN_CENTER);
    lv_obj_set_y(lv_button_0, 60);
    lv_obj_set_width(lv_button_0, 200);
    lv_obj_set_height(lv_button_0, 60);
    lv_obj_add_subject_increment_event(lv_button_0, &tap_count, LV_EVENT_CLICKED, 1);
    lv_obj_t * lv_label_2 = lv_label_create(lv_button_0);
    lv_label_set_text(lv_label_2, "Tap me");

    LV_TRACE_OBJ_CREATE("finished");

    return lv_obj_0;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

