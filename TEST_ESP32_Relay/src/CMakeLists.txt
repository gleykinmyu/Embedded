if(EXISTS "${CMAKE_SOURCE_DIR}/board_select.cmake")
    include("${CMAKE_SOURCE_DIR}/board_select.cmake")
endif()

include("${CMAKE_SOURCE_DIR}/../libs/cmake/pio_idf_embed.cmake")

if(APP_BOARD STREQUAL "esp32_eth01")
    idf_component_register(
        SRCS "main_eth01.cpp" "eth_lan8720.cpp" "http_status.cpp" "cli_eth01.cpp"
        INCLUDE_DIRS "." "../include"
        REQUIRES driver esp_eth esp_netif esp_event nvs_flash console esp_http_server lwip
    )
else()
    pio_idf_embed_txt(INDEX_HTML_ASM "${CMAKE_CURRENT_LIST_DIR}/index.html")
    idf_component_register(
        SRCS "main.cpp" "relay_board.cpp" "eth_w5500.cpp" "http_ui.cpp" "http_ota.cpp" "uart_cli.cpp"
             "lwip_udp.cpp" "artnet_relays.cpp"
             "${INDEX_HTML_ASM}"
        INCLUDE_DIRS "." "../include" "../../libs/Interfaces" "../../libs/dmx" "../../libs/dmx/artnet"
        REQUIRES driver esp_eth esp_netif esp_event nvs_flash console esp_http_server lwip app_update
    )
    if(NOT CMAKE_BUILD_EARLY_EXPANSION)
        pio_idf_embed_track(${COMPONENT_LIB} "${INDEX_HTML_ASM}" "${CMAKE_CURRENT_LIST_DIR}/index.html"
            TARGET_NAME relay_index_html_embed
        )
    endif()
endif()
