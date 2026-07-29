#include "AssetImportPipeline.h"
#include "AssetDependencyTracker.h"
#include "ThumbnailCache.h"
#include "../Core/DataModel/DataModel.h"
#include "../Core/DataModel/Part.h"
#include <filesystem>
#include <iostream>
#include <functional>
#include <nlohmann/json.hpp>
#include "Importers/SkeletalMeshImporter.h"

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace Engine::Assets {

void AssetImportPipeline::initialize() {
    std::string assetsDir = AssetDatabase::instance().getProjectRoot() + "/Assets";
    m_fileWatcher.watch(assetsDir, [this](const std::string& path, int type) {
        onFileChanged(path, type);
    });
}

void AssetImportPipeline::shutdown() {
    // any cleanup logic
}

void AssetImportPipeline::update() {
    std::function<void()> task;
    while (true) {
        {
            std::lock_guard<std::mutex> lock(m_mainThreadQueueMutex);
            if (m_mainThreadQueue.empty()) break;
            task = m_mainThreadQueue.front();
            m_mainThreadQueue.pop();
        }
        task();
    }
}

void AssetImportPipeline::onFileChanged(const std::string& path, int type) {
    if (type == 2 /* Deleted */) {
        // Handle deletion
        return;
    }

    if (path.ends_with(".meta")) return;

    std::string relativePath = AssetDatabase::instance().getRelativePath(path);
    AssetGuid guid = AssetDatabase::instance().getOrCreateGuid(relativePath);
    AssetMetadata* meta = AssetDatabase::instance().findMutable(guid);

    // simple check to avoid loops if file hash is unchanged
    // actual hash check could go here if we were computing MD5s, but we use last write time
    
    reimportAsset(guid, path);
}

void AssetImportPipeline::reimportAsset(AssetGuid guid, const std::string& absolutePath) {
    m_progress.totalAssets++;
    
    // Spawn a detached thread for MVP async work
    std::thread([this, guid, absolutePath]() {
        ImportResult result = runImporterForFile(absolutePath);
        
        std::lock_guard<std::mutex> lock(m_mainThreadQueueMutex);
        m_mainThreadQueue.push([this, guid, result]() {
            applyImportResult(guid, result);
        });
    }).detach();
}

ImportResult AssetImportPipeline::runImporterForFile(const std::string& absolutePath) {
    ImportResult result;
    result.success = false;
    
    std::string ext = fs::path(absolutePath).extension().string();
    if (ext == ".fbx" || ext == ".obj") {
        auto importedMesh = SkeletalMeshImporter::importFBX(absolutePath);
        if (!importedMesh.vertices.empty()) {
            result.success = true;
            
            // We can't safely modify AssetDatabase directly from this worker thread in a real engine without locks,
            // but for this MVP, the result application happens on the main thread anyway.
            // Wait, we need to pass the mesh data back to the main thread via ImportResult!
            
            // We will stash the shared_ptr inside ImportResult's metadata or a new field.
            // Actually, we can just use std::any for MVP.
            result.metadata.importSettings = "mesh_data"; // flag
            result.metadata.importerType = "Mesh";
            
            auto ptr = std::make_shared<ImportedSkeletalMesh>(std::move(importedMesh));
            result.importedMesh = ptr;
        }
    } else if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".tga") {
        result.success = true;
    } else {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        result.success = true;
    }

    return result;
}

void AssetImportPipeline::applyImportResult(AssetGuid guid, const ImportResult& result) {
    m_progress.completedAssets++;
    
    if (result.success) {
        if (result.importedMesh) {
            AssetDatabase::instance().setSkeletalMesh(guid, result.importedMesh);
            
            // Generate virtual sub-assets for animations and mesh
            AssetMetadata* meta = AssetDatabase::instance().findMutable(guid);
            if (meta) {
                json settings;
                try {
                    settings = json::parse(meta->importSettings.empty() ? "{}" : meta->importSettings);
                } catch (...) {}
                
                json subAssets = json::object();
                
                // Register a "Mesh" sub-asset
                AssetGuid meshGuid = AssetDatabase::instance().registerVirtualAsset(meta->relativePath, "Mesh", "SkeletalMesh");
                subAssets["Mesh"] = meshGuid.toString();
                
                // Register AnimationClips
                for (auto& clip : result.importedMesh->clips) {
                    if (clip.name.empty()) continue;
                    AssetGuid clipGuid = AssetDatabase::instance().registerVirtualAsset(meta->relativePath, clip.name, "AnimationClip");
                    subAssets[clip.name] = clipGuid.toString();
                }
                
                settings["subAssets"] = subAssets;
                meta->importSettings = settings.dump();
                AssetDatabase::instance().updateMetadata(guid, *meta);
            }
        }
        
        // Invalidate thumbnail
        ThumbnailCache::instance().invalidate(guid);
        
        notifyDependentInstances(guid);
    }
}

void AssetImportPipeline::notifyDependentInstances(AssetGuid guid) {
    std::function<void(std::shared_ptr<Instance>)> traverse = [&](std::shared_ptr<Instance> node) {
        if (!node) return;
        if (auto part = std::dynamic_pointer_cast<Part>(node)) {
            if (part->meshAssetGuid == guid) {
                part->setMeshFromAsset(guid); // re-trigger render/physics update
            }
        }
        for (const auto& child : node->getChildren()) {
            traverse(child);
        }
    };
    traverse(DataModel::instance());
}

} // namespace Engine::Assets
