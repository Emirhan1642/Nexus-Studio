cmake_minimum_required(VERSION 3.24)

include(FetchContent)

# 1. GLFW (Window management)
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG        3.4
)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glfw)

# 2. BGFX (Graphics API abstraction)
# Note: bgfx normally uses GENie, but bkaradzic maintains a cmake wrapper.
FetchContent_Declare(
    bgfx
    GIT_REPOSITORY https://github.com/bkaradzic/bgfx.cmake.git
    GIT_TAG        master
)
set(BGFX_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BGFX_BUILD_TOOLS ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(bgfx)

# 3. GoogleTest (for Phase 1 testing)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG        v1.14.0
)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# 4. Luau (Scripting)
FetchContent_Declare(
    luau
    GIT_REPOSITORY https://github.com/luau-lang/luau.git
    GIT_TAG        master
)
set(LUAU_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(LUAU_BUILD_CLI OFF CACHE BOOL "" FORCE)
set(LUAU_BUILD_WEB OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(luau)

# 5. Jolt Physics
FetchContent_Declare(
    JoltPhysics
    GIT_REPOSITORY https://github.com/jrouwe/JoltPhysics.git
    GIT_TAG        v5.0.0 # using a stable version
    SOURCE_SUBDIR  Build
)
set(USE_SSE4_2 ON CACHE BOOL "" FORCE) # typically good for modern PC
set(ENABLE_TESTS OFF CACHE BOOL "" FORCE)
set(ENABLE_SAMPLES OFF CACHE BOOL "" FORCE)
set(USE_STATIC_MSVC_RUNTIME_LIBRARY OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(JoltPhysics)

# JoltPhysics appends /fp:fast to CMAKE_CXX_FLAGS which leaks into subsequent targets.
# /fp:fast breaks stb_truetype.h in ImGui on MSVC, causing missing letters (W, F, k, etc).
# We must remove it here so it doesn't affect ImGui.
string(REPLACE "/fp:fast" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")

# 6. nlohmann_json
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
)
FetchContent_MakeAvailable(nlohmann_json)

# 7. efsw (File System Watcher)
FetchContent_Declare(
    efsw
    GIT_REPOSITORY https://github.com/SpartanJ/efsw.git
    GIT_TAG        master
)
# efsw uses BUILD_STATIC_LIBS, set it to ensure static link
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(BUILD_STATIC_LIBS ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(efsw)

# 8. Protobuf (Required for GNS)
FetchContent_Declare(
    protobuf
    GIT_REPOSITORY https://github.com/protocolbuffers/protobuf.git
    GIT_TAG        v21.12 # Version 21.12 doesn't require abseil
    SOURCE_SUBDIR  cmake
)
set(protobuf_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(protobuf_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(protobuf_BUILD_PROTOC_BINARIES ON CACHE BOOL "" FORCE)
set(protobuf_WITH_ZLIB OFF CACHE BOOL "" FORCE)
set(protobuf_MSVC_STATIC_RUNTIME OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(protobuf)

# Provide variables for FindProtobuf to succeed without actually searching
set(Protobuf_INCLUDE_DIR "${protobuf_SOURCE_DIR}/src" CACHE PATH "" FORCE)
set(Protobuf_LIBRARY "libprotobuf" CACHE FILEPATH "" FORCE)
set(Protobuf_PROTOC_EXECUTABLE "$<TARGET_FILE:protoc>" CACHE FILEPATH "" FORCE)
set(Protobuf_FOUND TRUE CACHE BOOL "" FORCE)
set(PROTOBUF_FOUND TRUE CACHE BOOL "" FORCE)
set(Protobuf_USE_STATIC_LIBS ON CACHE BOOL "" FORCE)
# GNS uses Protobuf::libprotobuf
if(NOT TARGET Protobuf::libprotobuf AND TARGET libprotobuf)
    add_library(Protobuf::libprotobuf ALIAS libprotobuf)
endif()

# 9. GameNetworkingSockets (Reliable UDP Networking + Encryption)
FetchContent_Declare(
    GameNetworkingSockets
    GIT_REPOSITORY https://github.com/ValveSoftware/GameNetworkingSockets.git
    GIT_TAG        master
)
# GNS has its own examples and tests, we disable them to speed up build
set(STEAMNETWORKINGSOCKETS_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(STEAMNETWORKINGSOCKETS_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(USE_CRYPTO "BCrypt" CACHE STRING "" FORCE)
FetchContent_MakeAvailable(GameNetworkingSockets)

# 10. Assimp (Asset Importer for Skeletal Meshes and FBX)
FetchContent_Declare(
    assimp
    GIT_REPOSITORY https://github.com/assimp/assimp.git
    GIT_TAG        v5.3.1
)
set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ASSIMP_TOOLS OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ZLIB ON CACHE BOOL "" FORCE) # We might need internal zlib for FBX
set(ASSIMP_INSTALL OFF CACHE BOOL "" FORCE)
set(ASSIMP_INJECT_DEBUG_POSTFIX OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(assimp)

# 11. Meshoptimizer (LOD Generation)
FetchContent_Declare(
    meshoptimizer
    GIT_REPOSITORY https://github.com/zeux/meshoptimizer.git
    GIT_TAG        v0.21
)
set(MESHOPT_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(meshoptimizer)

# 12. ImNodes (Node-Based Material Editor UI)
FetchContent_Declare(
    imnodes
    GIT_REPOSITORY https://github.com/Nelarius/imnodes.git
    GIT_TAG        master
)
FetchContent_GetProperties(imnodes)
if(NOT imnodes_POPULATED)
    FetchContent_Populate(imnodes)
endif()

# 13. Dear ImGui (Docking Branch)
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        docking
)
FetchContent_MakeAvailable(imgui)

add_library(dear_imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
)
target_include_directories(dear_imgui PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)
target_link_libraries(dear_imgui PUBLIC glfw)

if(MSVC)
    # stb_truetype.h (kullanilan imgui_draw.cpp icinde) MSVC /O2 (Release)
    # optimizasyonuyla bozuluyor: glyph koordinatlari yanlis hesaplaniyor,
    # W/F/K/O gibi harfler kayboliyor. Debug (/Od) duzgun calisir.
    # Cozum: Release'de /O2 yerine /O1 kullanmak (guvenli optimizasyon).
    target_compile_options(dear_imgui PRIVATE
        /fp:precise
        $<$<CONFIG:Release>:/O1>
        $<$<CONFIG:Release>:/Ob1>
    )
endif()
