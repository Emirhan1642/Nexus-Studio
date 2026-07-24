cmake_minimum_required(VERSION 3.24)

set(ANIMATION_SOURCES
    Skeleton.cpp
    AnimationClip.cpp
    AnimationPlayer.cpp
)

set(ANIMATION_HEADERS
    Skeleton.h
    AnimationClip.h
    AnimationPlayer.h
    IK/TwoBoneIK.h
)

add_library(EngineAnimation OBJECT ${ANIMATION_SOURCES} ${ANIMATION_HEADERS})

target_include_directories(EngineAnimation PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/..)

target_link_libraries(EngineAnimation PUBLIC EngineCore EngineRenderer bgfx)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${ANIMATION_SOURCES} ${ANIMATION_HEADERS})
