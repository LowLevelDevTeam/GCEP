function(add_slang_shader_target TARGET)
    set(SHADERS_DIR ${CMAKE_BINARY_DIR}/Assets/Shaders)
    file(MAKE_DIRECTORY ${SHADERS_DIR})

    file(GLOB_RECURSE SLANG_FILES
         CONFIGURE_DEPENDS
         "${CMAKE_SOURCE_DIR}/Assets/*.slang"
    )

    find_program(SLANGC_EXECUTABLE slangc REQUIRED)

    set(GENERATED_SPVS)
    foreach(SRC IN LISTS SLANG_FILES)
        get_filename_component(FNAME ${SRC} NAME_WE)
        set(OUT ${SHADERS_DIR}/${FNAME}.spv)

        string(FIND "${FNAME}" "Compute" COMPUTE_POS)
        if(COMPUTE_POS EQUAL 0)
            add_custom_command(
                OUTPUT ${OUT}
                COMMAND ${SLANGC_EXECUTABLE} ${SRC}
                        -target spirv
                        -profile spirv_1_4
                        -emit-spirv-directly
                        -fvk-use-entrypoint-name
                        -entry compMain
                        -o ${OUT}
                DEPENDS ${SRC}
                COMMENT "Compiling ${SRC} -> ${OUT}"
                VERBATIM
            )
        else()
            add_custom_command(
                OUTPUT ${OUT}
                COMMAND ${SLANGC_EXECUTABLE} ${SRC}
                        -target spirv
                        -profile spirv_1_4
                        -emit-spirv-directly
                        -fvk-use-entrypoint-name
                        -entry vertMain
                        -entry fragMain
                        -o ${OUT}
                DEPENDS ${SRC}
                COMMENT "Compiling ${SRC} -> ${OUT}"
                VERBATIM
            )
        endif()

        list(APPEND GENERATED_SPVS ${OUT})
    endforeach()

    add_custom_target(${TARGET} DEPENDS ${GENERATED_SPVS})
endfunction()