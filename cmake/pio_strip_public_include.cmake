# PlatformIO (SCons) mishandles CMake PUBLIC `-include header.h`:
# the flag is split/reordered → gcc treats the .h as a second source.
# Call once after project() in any ESP-IDF + PlatformIO app.
#
# Usage:
#   include(<path>/cmake/pio_strip_public_include.cmake)
#   embedded_pio_strip_public_include()
#
# Scans all BUILD_COMPONENTS; no per-component list needed.
# Safe under pure idf.py (no-op effect if flags were already fine).

function(embedded_pio_strip_public_include)
    idf_build_get_property(_build_components BUILD_COMPONENTS)
    foreach(_component IN LISTS _build_components)
        idf_component_get_property(_lib ${_component} COMPONENT_LIB)
        if(NOT _lib OR NOT TARGET ${_lib})
            continue()
        endif()
        foreach(_prop IN ITEMS INTERFACE_COMPILE_OPTIONS COMPILE_OPTIONS)
            get_target_property(_opts ${_lib} ${_prop})
            if(NOT _opts OR _opts STREQUAL "_opts-NOTFOUND")
                continue()
            endif()
            set(_filtered "")
            set(_skip_next FALSE)
            set(_changed FALSE)
            foreach(_o IN LISTS _opts)
                if(_skip_next)
                    set(_skip_next FALSE)
                    set(_changed TRUE)
                    continue()
                endif()
                # CMake usually stores: -include ; /path/to/header.h
                if(_o STREQUAL "-include")
                    set(_skip_next TRUE)
                    set(_changed TRUE)
                    continue()
                endif()
                list(APPEND _filtered "${_o}")
            endforeach()
            if(_changed)
                set_property(TARGET ${_lib} PROPERTY ${_prop} "${_filtered}")
            endif()
        endforeach()
    endforeach()
endfunction()
