cmake_minimum_required(VERSION 3.24)

set(TEST_SOURCES
    ReflectionTests.cpp
    AnimationTests.cpp
    NetworkingTests.cpp
    PhysicsAndSerializationTests.cpp
    HumanoidTests.cpp
    DataModelTests.cpp
    MathTests.cpp
    AssetTests.cpp
    ScriptingTests.cpp
    EditorStateTests.cpp
    SelectionManagerTests.cpp
    TransformTests.cpp
    GeometryTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../Editor/UI/SelectionManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../Editor/Undo/UndoStack.cpp
)

add_executable(NexusStudioTests ${TEST_SOURCES})

# Target properties
target_include_directories(NexusStudioTests PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../Editor
    ${CMAKE_BINARY_DIR}/_deps/imgui-src
    ${CMAKE_BINARY_DIR}/_deps/imgui-src/backends
)

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
    dear_imgui
    GTest::gtest_main
)

# Add to ctest
include(GoogleTest)
gtest_discover_tests(NexusStudioTests)

source_group("Source Files" FILES ${TEST_SOURCES})
