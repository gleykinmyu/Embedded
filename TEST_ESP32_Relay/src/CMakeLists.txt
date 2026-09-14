idf_component_register(
    SRCS "main.cpp" "relay_board.cpp" "eth_w5500.cpp" "http_ui.cpp" "uart_cli.cpp"
    INCLUDE_DIRS "." "../include"
    REQUIRES driver esp_eth esp_netif esp_event nvs_flash console esp_http_server lwip
    EMBED_TXTFILES "index.html"
)
