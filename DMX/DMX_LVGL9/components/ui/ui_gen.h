/**
 * @file ui_gen.h
 */

#ifndef UI_GEN_H
#define UI_GEN_H

#ifndef UI_SUBJECT_STRING_LENGTH
#define UI_SUBJECT_STRING_LENGTH 256
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/

#ifdef LV_LVGL_H_INCLUDE_SIMPLE
    #include "lvgl.h"
#else
    #include "lvgl/lvgl.h"
#endif

/*********************
 *      DEFINES
 *********************/

#define BG_DARK lv_color_hex(0x1a1a2e)

#define TEXT_PRIMARY lv_color_hex(0xeaeaea)

#define TEXT_MUTED lv_color_hex(0xa0a0b0)

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 * GLOBAL VARIABLES
 **********************/

/*-------------------
 * Permanent screens
 *------------------*/

extern lv_obj_t * main_screen;

/*----------------
 * Global styles
 *----------------*/

extern lv_style_t style_screen;

/*----------------
 * Fonts
 *----------------*/

/*----------------
 * Images
 *----------------*/

/*----------------
 * Subjects
 *----------------*/

extern lv_subject_t tap_count;

/**********************
 * GLOBAL PROTOTYPES
 **********************/

/*----------------
 * Event Callbacks
 *----------------*/

/**
 * Initialize the component library
 */

void ui_init_gen(const char * asset_path);

/**********************
 *      MACROS
 **********************/

/**********************
 *   POST INCLUDES
 **********************/

/*Include all the widget and components of this library*/
#include "screens/main_screen_gen.h"

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*UI_GEN_H*/