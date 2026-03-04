include_directories(${gtest_SOURCE_DIR}/include ${gtest_SOURCE_DIR})

add_executable(runUnitTests PhysicsTest.cpp)

target_link_libraries(runUnitTests gtest gtest_main)
target_link_libraries(runUnitTests GCEngineLib)