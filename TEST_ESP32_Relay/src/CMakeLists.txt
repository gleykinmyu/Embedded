idf_component_register(
    SRCS "main.cpp"
    INCLUDE_DIRS "." "../include"
    REQUIRES driver esp_eth esp_netif esp_event esp_http_server nvs_flash
    EMBED_TXTFILES "index.html"
)
