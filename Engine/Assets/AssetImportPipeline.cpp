#include "AssetImportPipeline.h"
#include "AssetDependencyTracker.h"
#include "ThumbnailCache.h"
#include "../Core/DataModel/DataModel.h"
#include "../Core/DataModel/Part.h"
#include <filesystem>
#include <iostream>
#include <functional>

namespace fs = std::filesystem;

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
    // Simulate expensive background import
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    ImportResult result;
    result.success = true;
    
    // In a real system, we'd extract actual metadata (e.g. LODs, vertices) here
    // We just return updated metadata. For MVP, we don't modify it heavily here.
    return result;
}

void AssetImportPipeline::applyImportResult(AssetGuid guid, const ImportResult& result) {
    m_progress.completedAssets++;
    
    if (result.success) {
        // The metadata could be updated here if runImporterForFile filled it out
        // AssetDatabase::instance().updateMetadata(guid, result.metadata);
        
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
