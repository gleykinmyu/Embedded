cmake_minimum_required(VERSION 3.16)

# ui/ — app UI; shared Embedded/libs/lv — aptumfr/lv OOP wrapper
get_filename_component(_EMBEDDED_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(EXTRA_COMPONENT_DIRS
    "${CMAKE_CURRENT_LIST_DIR}/ui"
    "${_EMBEDDED_ROOT}/libs/lv"
    "${_EMBEDDED_ROOT}/libs/dmx"
)

# aptumfr/lv needs C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
add_compile_options("-Wno-attributes")
project(dmx_lvgl9)

# PlatformIO breaks PUBLIC `-include` from esp_lvgl_adapter / similar.
include("${_EMBEDDED_ROOT}/cmake/pio_strip_public_include.cmake")
embedded_pio_strip_public_include()
