cmake_minimum_required(VERSION 3.24)

project(EngineScripting)

file(GLOB_RECURSE SCRIPTING_SOURCES
    "LuauRuntime/*.cpp"
    "LuauRuntime/*.h"
    "*.cpp"
    "*.h"
)

# We might not have files yet, so this could fail on Windows if sources is empty.
# Let's add a dummy cpp or just create the files soon.
# Triggering CMake re-configure to catch ScriptWatchdog.cpp

add_library(EngineScripting STATIC ${SCRIPTING_SOURCES})

target_include_directories(EngineScripting PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(EngineScripting PUBLIC
    EngineCore
    Luau.VM
    Luau.Compiler
    Luau.Ast
)
