file(GLOB_RECURSE GCENGINE_SCRIPTS_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
)

file(GLOB_RECURSE GCENGINE_CORE_SOURCES CONFIGURE_DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/../Engine/Core/Maths/*.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../Engine/Core/PhysicsWrapper/Layers/*.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../Engine/Core/PhysicsWrapper/physics_shape.cpp
)

add_library(GCEngineScripts SHARED
    ${GCENGINE_SCRIPTS_SOURCES}
    ${GCENGINE_CORE_SOURCES}
)

target_link_libraries(GCEngineScripts PRIVATE Jolt ktx)

target_compile_definitions(GCEngineScripts PRIVATE GCE_SCRIPT_BUILD)

target_include_directories(GCEngineScripts PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../Engine/Core
    ${CMAKE_CURRENT_SOURCE_DIR}/../Engine/Core/Maths
    ${CMAKE_CURRENT_SOURCE_DIR}/../Engine/Core/Scripting
    ${CMAKE_CURRENT_SOURCE_DIR}/../Externals/glm
    ${CMAKE_CURRENT_SOURCE_DIR}/../Externals/JoltPhysics
    ${CMAKE_CURRENT_SOURCE_DIR}/../Externals/KTX-Software/lib/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../Externals/boost_pfr/include
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
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:GCEngineScripts> $<TARGET_FILE_DIR:GCEngine>/Scripts)