idf_component_register(
    SRCS
        "ui.cpp"
        "monitor_screen.cpp"
        "graph_screen.cpp"
    INCLUDE_DIRS
        "."
    REQUIRES
        lv
        lvgl
        dmx
        esp_timer
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_20)