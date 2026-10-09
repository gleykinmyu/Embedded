# Dual-mode CMake:
# - ESP-IDF / PlatformIO: register as component "ui"
# - LVGL Editor preview: build as a static library (Editor may overwrite this file)
#
# If the Editor regenerates this file, restore the ESP_PLATFORM branch from git.

if(ESP_PLATFORM)
  include(${CMAKE_CURRENT_LIST_DIR}/user_config.cmake)
  include(${CMAKE_CURRENT_LIST_DIR}/file_list_gen.cmake)

  idf_component_register(
      SRCS ${LV_EDITOR_PROJECT_SOURCES}
      INCLUDE_DIRS "." "screens"
      REQUIRES lvgl
  )

  target_compile_definitions(${COMPONENT_LIB} PUBLIC LV_LVGL_H_INCLUDE_SIMPLE)
else()
  if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    cmake_minimum_required(VERSION 3.10)
    project(LVGLProject)
  endif()

  if(DEFINED LVGL_COMPONENT_LIB_NAME)
    set(LIB_NAME ${LVGL_COMPONENT_LIB_NAME})
  else()
    set(LIB_NAME lib-ui)
  endif()

  include(${CMAKE_CURRENT_LIST_DIR}/user_config.cmake)
  set(LV_EDITOR_USER_SOURCES ${LV_EDITOR_PROJECT_SOURCES})
  set(LV_EDITOR_PROJECT_SOURCES)
  include(${CMAKE_CURRENT_LIST_DIR}/file_list_gen.cmake)

  add_library(${LIB_NAME} ${LV_EDITOR_PROJECT_SOURCES})
  if(LV_EDITOR_USER_SOURCES)
    add_library(${LIB_NAME}_user OBJECT ${LV_EDITOR_USER_SOURCES})
    target_link_libraries(${LIB_NAME} PUBLIC $<TARGET_OBJECTS:${LIB_NAME}_user>)
  endif()
  if(TARGET lvgl)
    target_link_libraries(${LIB_NAME} PUBLIC lvgl)
  endif()
  target_include_directories(${LIB_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
endif()
