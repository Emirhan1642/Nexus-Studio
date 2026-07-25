cmake_minimum_required(VERSION 3.24)

find_package(Protobuf REQUIRED)

protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS Messages.proto)

set(SOURCES
    Transport/NetworkContext.cpp
    Transport/NetworkServer.cpp
    Transport/NetworkClient.cpp
    Replication/ReplicationManager.cpp
    Serialization/PacketSerializer.cpp
    Prediction/ClientPredictor.cpp
    Prediction/HumanoidPredictor.cpp
)

add_library(EngineNetworking OBJECT ${SOURCES} ${PROTO_SRCS} ${PROTO_HDRS})

target_include_directories(EngineNetworking PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}/.. 
    ${CMAKE_CURRENT_BINARY_DIR}
)

target_link_libraries(EngineNetworking PUBLIC
    GameNetworkingSockets
    EngineCore
    libprotobuf
)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SOURCES} Messages.proto)
source_group("Generated" FILES ${PROTO_SRCS} ${PROTO_HDRS})

# Ensure EngineCore waits for the proto headers to be generated
add_custom_target(GenerateNetworkingProto DEPENDS ${PROTO_HDRS})
if (TARGET EngineCore)
    add_dependencies(EngineCore GenerateNetworkingProto)
endif()
