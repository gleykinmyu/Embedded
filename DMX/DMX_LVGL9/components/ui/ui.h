/**
 * @file ui.h
 * User-facing API for the LVGL Editor UI project.
 * Safe to edit — Editor creates this only if missing.
 */

#ifndef UI_H
#define UI_H

#ifdef __cplusplus
extern "C" {
#endif

#include "ui_gen.h"

/**
 * Initialize UI assets/subjects and create permanent screens.
 * @param asset_path Prefix for file-based fonts/images ("" if none).
 */
void ui_init(const char * asset_path);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*UI_H*/
