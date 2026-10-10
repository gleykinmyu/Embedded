# Header-only C++20 wrapper: https://github.com/aptumfr/lv (AGPL-3.0)
# Vendored headers only — demos/examples stripped.

idf_component_register(
    INCLUDE_DIRS "include"
    REQUIRES lvgl
)

target_compile_features(${COMPONENT_LIB} INTERFACE cxx_std_20)
