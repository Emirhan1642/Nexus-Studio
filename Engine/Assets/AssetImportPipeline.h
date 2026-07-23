#pragma once
#include "AssetDatabase.h"
#include "FileWatcher.h"
#include <string>
#include <thread>
#include <mutex>
#include <queue>
#include <atomic>
#include <functional>

namespace Engine::Assets {

struct ImportProgress {
    std::atomic<int> totalAssets{0};
    std::atomic<int> completedAssets{0};
};

struct ImportResult {
    AssetMetadata metadata;
    bool success = false;
};

class AssetImportPipeline {
public:
    static AssetImportPipeline& instance() { static AssetImportPipeline p; return p; }
    
    void initialize();
    void shutdown();
    void update(); // call from main thread every frame
    
    void onFileChanged(const std::string& path, int type); // type: 0=Added, 1=Modified, 2=Deleted
    void reimportAsset(AssetGuid guid, const std::string& absolutePath);

    ImportProgress& getProgress() { return m_progress; }

private:
    void applyImportResult(AssetGuid guid, const ImportResult& result);
    ImportResult runImporterForFile(const std::string& absolutePath);
    void notifyDependentInstances(AssetGuid guid);

    ImportProgress m_progress;

    std::queue<std::function<void()>> m_mainThreadQueue;
    std::mutex m_mainThreadQueueMutex;
    FileWatcher m_fileWatcher;
};

} // namespace Engine::Assets
