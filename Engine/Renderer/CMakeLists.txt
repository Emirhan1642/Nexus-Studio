cmake_minimum_required(VERSION 3.24)

set(RENDERER_SOURCES
    SceneGraph/RenderScene.cpp
    Renderer.cpp
)

set(RENDERER_HEADERS
    SceneGraph/RenderProxy.h
    SceneGraph/RenderScene.h
    Camera.h
    Renderer.h
)

add_library(EngineRenderer STATIC ${RENDERER_SOURCES} ${RENDERER_HEADERS})

target_include_directories(EngineRenderer PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)

target_link_libraries(EngineRenderer PUBLIC
    EngineCore
    bgfx
    bimg
    bx
)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${RENDERER_SOURCES} ${RENDERER_HEADERS})
