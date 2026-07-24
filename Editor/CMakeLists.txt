cmake_minimum_required(VERSION 3.24)

set(EDITOR_SOURCES
    Main.cpp
    UI/ImGuiLayer.cpp
    UI/ViewportPanel.cpp
    UI/ExplorerPanel.cpp
    UI/PropertiesPanel.cpp
    UI/SelectionManager.cpp
    UI/AssetBrowserPanel.cpp
    UI/MaterialEditorPanel.cpp
    Undo/UndoStack.cpp
)

set(EDITOR_EXTERNAL_SOURCES
    ${CMAKE_BINARY_DIR}/_deps/imnodes-src/imnodes.cpp
)

add_executable(NexusStudioEditor ${EDITOR_SOURCES} ${EDITOR_EXTERNAL_SOURCES})

# Target properties
target_include_directories(NexusStudioEditor PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_BINARY_DIR}/_deps/bgfx-src/examples/common
    ${CMAKE_BINARY_DIR}/_deps/bgfx-src/bgfx/3rdparty/dear-imgui
    ${CMAKE_BINARY_DIR}/_deps/imnodes-src
)

# Link dependencies
target_link_libraries(NexusStudioEditor PRIVATE
    EngineCore
    EnginePhysics
    Jolt
    EngineRenderer
    EngineScripting
    EngineAssets
    EngineNetworking
    EngineAnimation
    example-common
    glfw
    bgfx
    bimg
    bx
)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${EDITOR_SOURCES})
