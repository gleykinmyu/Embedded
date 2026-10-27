set(CCAM_DIR "${CMAKE_SOURCE_DIR}/../libs/ccam")
set(BIF_DIR "${CMAKE_SOURCE_DIR}/../libs/Interfaces")

# HTML страница UI → C++ raw-string include (PlatformIO EMBED_TXTFILES ломается с custom build_dir).
set(STATUS_HTML "${CMAKE_CURRENT_LIST_DIR}/www/status.html")
set(STATUS_HTML_INC "${CMAKE_CURRENT_BINARY_DIR}/status.html.inc")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${STATUS_HTML}")
file(READ "${STATUS_HTML}" _status_html_body)
file(WRITE "${STATUS_HTML_INC}" "R\"html(${_status_html_body})html\"\n")
unset(_status_html_body)

idf_component_register(
    SRCS
        "main_eth01.cpp"
        "eth_lan8720.cpp"
        "http_status.cpp"
        "cli_eth01.cpp"
        "rs422_uart.cpp"
        "ph350_ctrl.cpp"
        "ota_espota.cpp"
        "${CCAM_DIR}/src/transport.cpp"
        "${CCAM_DIR}/src/pt_device.cpp"
        "${CCAM_DIR}/src/camera_protocol.cpp"
        "${CCAM_DIR}/src/camera_device.cpp"
    INCLUDE_DIRS
        "."
        "../include"
        "${CCAM_DIR}/include"
        "${BIF_DIR}"
        "${CMAKE_CURRENT_BINARY_DIR}"
    REQUIRES
        driver esp_eth esp_netif esp_event nvs_flash console esp_http_server lwip esp_timer
        app_update
)
