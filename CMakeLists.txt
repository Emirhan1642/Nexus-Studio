cmake_minimum_required(VERSION 3.24)
project(NexusStudio LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Ensure all projects use the Dynamic CRT (/MD and /MDd) to prevent LNK4098 conflicts
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")

# Binaries go to the bin folder
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Use folders for IDEs like Visual Studio
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

add_subdirectory(ThirdParty)
add_subdirectory(Engine)
add_subdirectory(Editor)

# Tests
enable_testing()
add_subdirectory(Tests)
