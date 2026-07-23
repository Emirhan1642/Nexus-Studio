cmake_minimum_required(VERSION 3.24)

set(SOURCES
    Transport/NetworkContext.cpp
    Transport/NetworkServer.cpp
    Transport/NetworkClient.cpp
    Replication/ReplicationManager.cpp
    Prediction/ClientPredictor.cpp
)

add_library(EngineNetworking OBJECT ${SOURCES})

target_include_directories(EngineNetworking PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)

target_link_libraries(EngineNetworking PUBLIC
    GameNetworkingSockets
    EngineCore
)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SOURCES})
