set(INTERFACES_DIR "${CMAKE_CURRENT_LIST_DIR}/../../libs/Interfaces")
set(BOARD_ESP32_DIR "${CMAKE_CURRENT_LIST_DIR}/../board/esp32")
set(EMBEDDED_CLI_DIR "${CMAKE_CURRENT_LIST_DIR}/../third_party/embedded-cli")

idf_component_register(
    SRCS "main.cpp" "${BOARD_ESP32_DIR}/instantiate.cpp" "${BOARD_ESP32_DIR}/stdio.cpp"
        "${EMBEDDED_CLI_DIR}/embedded_cli.c"
    INCLUDE_DIRS "." "${INTERFACES_DIR}" "${BOARD_ESP32_DIR}" "${EMBEDDED_CLI_DIR}"
    REQUIRES driver esp_hw_support esp_system freertos log spi_flash
)

target_link_libraries(${COMPONENT_LIB} INTERFACE "-Wl,--wrap=_write_r")
