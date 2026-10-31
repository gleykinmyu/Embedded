set(CCAM_DIR "${CMAKE_SOURCE_DIR}/../libs/ccam")
set(BIF_DIR "${CMAKE_SOURCE_DIR}/../libs/Interfaces")

set(WWW_DIR "${CMAKE_CURRENT_LIST_DIR}/www")
set(STATUS_HTML_EMBED "${CMAKE_CURRENT_LIST_DIR}/generated/status.html")
set(BUILD_WWW_PY "${CMAKE_CURRENT_LIST_DIR}/../tools/build_www.py")

include("${CMAKE_SOURCE_DIR}/../libs/cmake/pio_idf_embed.cmake")

find_program(ESP_CAM_PYTHON NAMES python python3 py)
if(NOT ESP_CAM_PYTHON)
    message(FATAL_ERROR "python required for www embed")
endif()

execute_process(
    COMMAND "${ESP_CAM_PYTHON}" "${BUILD_WWW_PY}" "--out" "${STATUS_HTML_EMBED}"
    WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/.."
    RESULT_VARIABLE _www_rc
)
if(NOT _www_rc EQUAL 0)
    message(FATAL_ERROR "build_www.py failed (${_www_rc})")
endif()

pio_idf_embed_txt(STATUS_HTML_ASM "${STATUS_HTML_EMBED}")

idf_component_register(
    SRCS
        "main_eth01.cpp"
        "eth_lan8720.cpp"
        "http_status.cpp"
        "cli_eth01.cpp"
        "rs422_uart.cpp"
        "ph350_ctrl.cpp"
        "ota_espota.cpp"
        "${STATUS_HTML_ASM}"
        "${CCAM_DIR}/src/transport.cpp"
        "${CCAM_DIR}/src/pt_device.cpp"
        "${CCAM_DIR}/src/integrated_ptz.cpp"
        "${CCAM_DIR}/src/camera_protocol.cpp"
        "${CCAM_DIR}/src/camera_device.cpp"
        "${CCAM_DIR}/src/device_caps.cpp"
        "${CCAM_DIR}/src/pt_link_health.cpp"
        "${CCAM_DIR}/src/cam_param_ranges.cpp"
        "${CCAM_DIR}/src/osd_params.cpp"
    INCLUDE_DIRS
        "."
        "../include"
        "${CCAM_DIR}/include"
        "${BIF_DIR}"
    REQUIRES
        driver esp_eth esp_netif esp_event nvs_flash console esp_http_server lwip esp_timer
        app_update
)

if(CMAKE_BUILD_EARLY_EXPANSION)
    return()
endif()

file(GLOB_RECURSE WWW_SRCS
    "${WWW_DIR}/index.html"
    "${WWW_DIR}/css/*.css"
    "${WWW_DIR}/js/*.js"
    "${WWW_DIR}/panels/*.html"
    "${WWW_DIR}/vendor/iptv-ui/kit/css/*.css"
    "${WWW_DIR}/vendor/iptv-ui/kit/js/*.js"
    "${WWW_DIR}/vendor/iptv-ui/kit/html/*.html"
)

add_custom_command(
    OUTPUT "${STATUS_HTML_EMBED}"
    COMMAND "${ESP_CAM_PYTHON}" "${BUILD_WWW_PY}" "--out" "${STATUS_HTML_EMBED}"
    DEPENDS ${WWW_SRCS} "${BUILD_WWW_PY}"
    WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/.."
    COMMENT "Assemble www → generated/status.html"
    VERBATIM
)
add_custom_target(esp_cam_www_html DEPENDS "${STATUS_HTML_EMBED}")

pio_idf_embed_track(${COMPONENT_LIB} "${STATUS_HTML_ASM}" "${STATUS_HTML_EMBED}"
    TARGET_NAME esp_cam_www_embed
    DEPENDS ${WWW_SRCS} "${BUILD_WWW_PY}"
    COMMENT "Embed generated/status.html"
)
add_dependencies(esp_cam_www_embed esp_cam_www_html)
