function(copy_textures TARGET)
    set(TEXTURE_SRC ${CMAKE_SOURCE_DIR}/Engine/Core/RHI/TestTextures)
    set(TEXTURE_DST ${CMAKE_BINARY_DIR}/TestTextures)

    add_custom_target(${TARGET} ALL DEPENDS
        COMMAND ${CMAKE_COMMAND} -E make_directory ${TEXTURE_DST}
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${TEXTURE_SRC} ${TEXTURE_DST}
    )
endfunction()