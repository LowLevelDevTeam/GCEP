function(add_slang_shader_target TARGET)
    set(SHADERS_DIR ${CMAKE_BINARY_DIR}/Shaders)
    file(MAKE_DIRECTORY ${SHADERS_DIR})

    file(GLOB_RECURSE SHADER_SOURCES
            CONFIGURE_DEPENDS
            "${CMAKE_SOURCE_DIR}/Engine/*.slang"
    )

    find_program(SLANGC_EXECUTABLE slangc REQUIRED)

    add_custom_command(
            OUTPUT ${SHADERS_DIR}/triangle.spv
            COMMAND ${SLANGC_EXECUTABLE}
            ${SHADER_SOURCES}
            -target spirv
            -profile spirv_1_4
            -emit-spirv-directly
            -fvk-use-entrypoint-name
            -entry vertMain
            -entry fragMain
            -o ${SHADERS_DIR}/triangle.spv
            DEPENDS ${SHADER_SOURCES}
            COMMENT "Compiling Slang Shaders"
            VERBATIM
    )

    add_custom_target(${TARGET} DEPENDS ${SHADERS_DIR}/triangle.spv)
endfunction()
