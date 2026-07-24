cmake_minimum_required(VERSION 3.24)

set(CORE_SOURCES
    Reflection/TypeRegistry.cpp
    Reflection/EnumRegistry.cpp
    DataModel/Instance.cpp
    DataModel/InstanceRegistry.cpp
    DataModel/Part.cpp
    DataModel/Constraint.cpp
    DataModel/WeldConstraint.cpp
    DataModel/HingeConstraint.cpp
    DataModel/SpringConstraint.cpp
    DataModel/Humanoid.cpp
    DataModel/HumanoidStateMachine.cpp
    DataModel/RemoteEvent.cpp
    DataModel/AnimationTrack.cpp
    DataModel/DataModelSerializer.cpp
    Math/Matrix4.cpp
    Math/Quaternion.cpp
)

set(CORE_HEADERS
    Reflection/TypeRegistry.h
    Reflection/ClassBuilder.h
    Reflection/EnumRegistry.h
    DataModel/Instance.h
    DataModel/InstanceRegistry.h
    DataModel/DataModel.h
    DataModel/Part.h
    DataModel/Constraint.h
    DataModel/WeldConstraint.h
    DataModel/HingeConstraint.h
    DataModel/SpringConstraint.h
    DataModel/RemoteEvent.h
    DataModel/AnimationTrack.h
    DataModel/DataModelSerializer.h
    Math/Vector3.h
    Math/Matrix4.h
    Math/Quaternion.h
)

add_library(EngineCore OBJECT ${CORE_SOURCES} ${CORE_HEADERS})

# Include directories so that other modules can include "Reflection/TypeRegistry.h"
target_include_directories(EngineCore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(EngineCore PUBLIC EnginePhysics Jolt bgfx GameNetworkingSockets nlohmann_json::nlohmann_json)

# For setting folders in IDE
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${CORE_SOURCES} ${CORE_HEADERS})
