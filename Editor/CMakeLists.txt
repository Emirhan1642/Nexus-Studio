cmake_minimum_required(VERSION 3.24)

set(EDITOR_SOURCES
    Main.cpp
    UI/Backend/imgui_impl_bgfx.cpp
    UI/Backend/gizmo.cpp
    UI/ImGuiLayer.cpp
    UI/RedrawScheduler.cpp
    UI/NexusTheme.cpp
    UI/IconRegistry.cpp
    UI/TopBar.cpp
    UI/FileListBar.cpp
    UI/LeftToolbar.cpp
    UI/ViewportPanel.cpp
    UI/ExplorerPanel.cpp
    UI/PropertiesPanel.cpp
    UI/SelectionManager.cpp
    UI/AssetBrowserPanel.cpp
    UI/ConsolePanel.cpp
    UI/MaterialEditorPanel.cpp
    UI/AICopilotPanel.cpp
    UI/BottomBar.cpp
    UI/EditorLayout.cpp
    UI/ModelingOperatorPanel.cpp
    Modeling/ModelingContext.cpp
    Asset/MeshAssetExporter.cpp
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
    ${CMAKE_BINARY_DIR}/_deps/bgfx-src/bgfx/examples/common
    ${CMAKE_BINARY_DIR}/_deps/imgui-src
    ${CMAKE_BINARY_DIR}/_deps/imgui-src/backends
    ${CMAKE_BINARY_DIR}/_deps/imnodes-src
    ${CMAKE_BINARY_DIR}/_deps/bgfx-src/bimg/3rdparty/stb
    ${CMAKE_BINARY_DIR}/_deps/bgfx-src/bgfx/3rdparty/dear-imgui
    ${CMAKE_CURRENT_SOURCE_DIR}/../ThirdParty/httplib
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
    dear_imgui
    glfw
    bgfx
    bimg
    bx
)

if (WIN32)
    target_link_libraries(NexusStudioEditor PRIVATE ws2_32 crypt32)
endif()

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${EDITOR_SOURCES})
