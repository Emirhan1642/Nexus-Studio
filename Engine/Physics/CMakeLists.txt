cmake_minimum_required(VERSION 3.24)

set(PHYSICS_SOURCES
    PhysicsWorld.cpp
    ContactListenerImpl.cpp
)

set(PHYSICS_HEADERS
    PhysicsWorld.h
    PhysicsConversions.h
    ContactListenerImpl.h
    PhysicsBodyHandle.h
)

add_library(EnginePhysics OBJECT ${PHYSICS_SOURCES} ${PHYSICS_HEADERS})

target_include_directories(EnginePhysics PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

# Need to make sure JoltPhysics includes are found
# JoltPhysics CMake usually exposes its includes via target properties, but for OBJECT lib we might need to manually add if we can't link
target_link_libraries(EnginePhysics PUBLIC Jolt)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${PHYSICS_SOURCES} ${PHYSICS_HEADERS})
