/**
 * @file ui.h
 * @brief C API для main.c → C++ UI (aptumfr/lv).
 *
 * Вызывать под esp_lv_adapter_lock / unlock.
 */

#ifndef UI_H
#define UI_H

#ifdef __cplusplus
extern "C" {
#endif

/** Собрать монитор, загрузить экран, запустить demo-feed. */
void ui_init(void);

#ifdef __cplusplus
}
#endif

#endif /* UI_H */
