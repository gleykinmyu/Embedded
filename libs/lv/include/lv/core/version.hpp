#pragma once

/**
 * @file version.hpp
 * @brief LVGL version detection for conditional API support
 */

#include <lvgl.h>

/// Check if the LVGL version is at least major.minor.patch
#ifndef LV_VERSION_AT_LEAST
#define LV_VERSION_AT_LEAST(major, minor, patch) \
    ((LVGL_VERSION_MAJOR > (major)) || \
     (LVGL_VERSION_MAJOR == (major) && LVGL_VERSION_MINOR > (minor)) || \
     (LVGL_VERSION_MAJOR == (major) && LVGL_VERSION_MINOR == (minor) && LVGL_VERSION_PATCH >= (patch)))
#endif
