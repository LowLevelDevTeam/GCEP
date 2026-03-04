add_library(GCEngineScripts SHARED SampleScript.cpp)

target_compile_definitions(GCEngineScripts PRIVATE GCE_SCRIPT_BUILD)

target_include_directories(GCEngineScripts PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../Engine/Core
    ${CMAKE_CURRENT_SOURCE_DIR}/../Engine/Core/Scripting
    ${CMAKE_CURRENT_SOURCE_DIR}/../Externals/glm
    ${CMAKE_CURRENT_SOURCE_DIR}/../Externals/JoltPhysics
    ${Vulkan_INCLUDE_DIRS}
)

set_target_properties(GCEngineScripts PROPERTIES
    PREFIX ""
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/Scripts
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/Scripts
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/Scripts
)

add_custom_command(TARGET GCEngineScripts POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:GCEngine>/Scripts
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:GCEngineScripts> $<TARGET_FILE_DIR:GCEngine>/Scripts
)
