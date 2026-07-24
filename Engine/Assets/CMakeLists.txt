cmake_minimum_required(VERSION 3.24)

set(ASSETS_SOURCES
    AssetDatabase.cpp
    AssetImportPipeline.cpp
    FileWatcher.cpp
    ThumbnailRenderer.cpp
    Importers/SkeletalMeshImporter.cpp
)

set(ASSETS_HEADERS
    AssetDatabase.h
    AssetImportPipeline.h
    FileWatcher.h
    ThumbnailRenderer.h
    ThumbnailCache.h
    AssetDependencyTracker.h
    Importers/SkeletalMeshImporter.h
)

add_library(EngineAssets OBJECT ${ASSETS_SOURCES} ${ASSETS_HEADERS})

target_include_directories(EngineAssets PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(EngineAssets PUBLIC nlohmann_json::nlohmann_json efsw EngineCore EngineRenderer EngineAnimation bgfx assimp)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${ASSETS_SOURCES} ${ASSETS_HEADERS})
