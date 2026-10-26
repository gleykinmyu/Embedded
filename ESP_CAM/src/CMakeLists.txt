set(CCAM_DIR "${CMAKE_SOURCE_DIR}/../libs/ccam")
set(BIF_DIR "${CMAKE_SOURCE_DIR}/../libs/Interfaces")

idf_component_register(
    SRCS
        "main_eth01.cpp"
        "eth_lan8720.cpp"
        "http_status.cpp"
        "cli_eth01.cpp"
        "rs422_uart.cpp"
        "ph350_ctrl.cpp"
        "${CCAM_DIR}/src/rs485_transport.cpp"
        "${CCAM_DIR}/src/pt_device.cpp"
        "${CCAM_DIR}/src/camera_protocol.cpp"
        "${CCAM_DIR}/src/camera_device.cpp"
    INCLUDE_DIRS
        "."
        "../include"
        "${CCAM_DIR}/include"
        "${BIF_DIR}"
    REQUIRES
        driver esp_eth esp_netif esp_event nvs_flash console esp_http_server lwip esp_timer
)
