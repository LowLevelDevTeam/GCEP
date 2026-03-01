add_executable(ScriptingRunner main.cpp)

target_include_directories(ScriptingRunner PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Engine/Core/Scripting
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Engine
)

target_link_libraries(ScriptingRunner PRIVATE GCEngineScripting)

