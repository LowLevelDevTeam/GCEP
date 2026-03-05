function(copy_textures TARGET)
    set(TEXTURE_SRC ${CMAKE_SOURCE_DIR}/Assets)
    set(TEXTURE_DST ${CMAKE_BINARY_DIR}/Assets)

    add_custom_target(${TARGET} ALL DEPENDS
        COMMAND ${CMAKE_COMMAND} -E make_directory ${TEXTURE_DST}
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${TEXTURE_SRC} ${TEXTURE_DST}
    )
endfunction()