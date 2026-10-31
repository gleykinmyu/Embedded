# PIO-safe embed: same symbols as IDF EMBED_TXTFILES / EMBED_FILES, but .S
# lives in the component binary dir so PlatformIO SCons does not double the path.
#
# Usage (before idf_component_register):
#   include("${CMAKE_SOURCE_DIR}/../libs/cmake/pio_idf_embed.cmake")
#   pio_idf_embed_txt(MY_ASM "${CMAKE_CURRENT_LIST_DIR}/index.html")
#   idf_component_register(SRCS ... "${MY_ASM}" ...)
#
# After early expansion (optional incremental rebuild):
#   pio_idf_embed_track(${COMPONENT_LIB} "${MY_ASM}" "${CMAKE_CURRENT_LIST_DIR}/index.html")

if(NOT DEFINED ENV{IDF_PATH})
    message(FATAL_ERROR "pio_idf_embed: IDF_PATH is not set")
endif()

set(PIO_IDF_EMBED_ASM_CMAKE
    "$ENV{IDF_PATH}/tools/cmake/scripts/data_file_embed_asm.cmake")

function(pio_idf_embed_generate out_var input file_type)
    get_filename_component(_pio_idf_name "${input}" NAME)
    set(_pio_idf_asm "${CMAKE_CURRENT_BINARY_DIR}/${_pio_idf_name}.S")
    if(NOT EXISTS "${input}")
        message(FATAL_ERROR "pio_idf_embed: missing input ${input}")
    endif()
    execute_process(
        COMMAND "${CMAKE_COMMAND}"
                -D "DATA_FILE=${input}"
                -D "SOURCE_FILE=${_pio_idf_asm}"
                -D "FILE_TYPE=${file_type}"
                -P "${PIO_IDF_EMBED_ASM_CMAKE}"
        RESULT_VARIABLE _pio_idf_rc
    )
    if(NOT _pio_idf_rc EQUAL 0)
        message(FATAL_ERROR "pio_idf_embed: embed failed for ${input} (${_pio_idf_rc})")
    endif()
    set(${out_var} "${_pio_idf_asm}" PARENT_SCOPE)
endfunction()

function(pio_idf_embed_txt out_var input)
    pio_idf_embed_generate(_pio_idf_tmp "${input}" "TEXT")
    set(${out_var} "${_pio_idf_tmp}" PARENT_SCOPE)
endfunction()

function(pio_idf_embed_bin out_var input)
    pio_idf_embed_generate(_pio_idf_tmp "${input}" "BINARY")
    set(${out_var} "${_pio_idf_tmp}" PARENT_SCOPE)
endfunction()

# Rebuild .S when input (or extra DEPENDS) changes; attach to component lib.
function(pio_idf_embed_track component_lib asm_file input_file)
    cmake_parse_arguments(ARG "" "TARGET_NAME;FILE_TYPE;COMMENT" "DEPENDS" ${ARGN})
    if(NOT ARG_FILE_TYPE)
        set(ARG_FILE_TYPE "TEXT")
    endif()
    if(NOT ARG_TARGET_NAME)
        get_filename_component(_pio_idf_t "${asm_file}" NAME)
        string(MAKE_C_IDENTIFIER "pio_idf_embed_${_pio_idf_t}" ARG_TARGET_NAME)
    endif()
    if(NOT ARG_COMMENT)
        set(ARG_COMMENT "PIO-safe IDF embed ${input_file}")
    endif()

    add_custom_command(
        OUTPUT "${asm_file}"
        COMMAND "${CMAKE_COMMAND}"
                -D "DATA_FILE=${input_file}"
                -D "SOURCE_FILE=${asm_file}"
                -D "FILE_TYPE=${ARG_FILE_TYPE}"
                -P "${PIO_IDF_EMBED_ASM_CMAKE}"
        DEPENDS "${input_file}" ${ARG_DEPENDS} "${PIO_IDF_EMBED_ASM_CMAKE}"
        COMMENT "${ARG_COMMENT}"
        VERBATIM
    )
    add_custom_target(${ARG_TARGET_NAME} DEPENDS "${asm_file}")
    add_dependencies(${component_lib} ${ARG_TARGET_NAME})
    set_source_files_properties("${asm_file}" PROPERTIES GENERATED TRUE)
endfunction()
