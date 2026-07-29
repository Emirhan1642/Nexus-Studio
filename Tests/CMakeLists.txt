cmake_minimum_required(VERSION 3.24)

set(TEST_SOURCES
    ReflectionTests.cpp
    AnimationTests.cpp
    NetworkingTests.cpp
    PhysicsAndSerializationTests.cpp
    HumanoidTests.cpp
)

add_executable(NexusStudioTests ${TEST_SOURCES})

# Target properties
target_include_directories(NexusStudioTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)

# Link dependencies
target_link_libraries(NexusStudioTests PRIVATE
    EngineCore
    EnginePhysics
    Jolt
    EngineAssets
    EngineRenderer
    EngineScripting
    EngineNetworking
    EngineAnimation
    GTest::gtest_main
)

# Add to ctest
include(GoogleTest)
gtest_discover_tests(NexusStudioTests)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${TEST_SOURCES})
