#include "FileWatcher.h"
#include <filesystem>

namespace Engine::Assets {

FileWatcher::FileWatcher() {
    m_watcher = new efsw::FileWatcher();
}

FileWatcher::~FileWatcher() {
    if (m_watcher) {
        if (m_watchID > 0) {
            m_watcher->removeWatch(m_watchID);
        }
        delete m_watcher;
    }
}

void FileWatcher::watch(const std::string& directory, ChangeCallback callback) {
    m_callback = callback;
    m_watchID = m_watcher->addWatch(directory, this, true);
    m_watcher->watch();
}

void FileWatcher::handleFileAction(efsw::WatchID watchid, const std::string& dir,
                                   const std::string& filename, efsw::Action action,
                                   const std::string& oldFilename) {
    if (!m_callback) return;

    std::string fullPath = (std::filesystem::path(dir) / filename).string();
    int type = -1;
    
    switch (action) {
        case efsw::Actions::Add: type = 0; break;
        case efsw::Actions::Delete: type = 2; break;
        case efsw::Actions::Modified: type = 1; break;
        case efsw::Actions::Moved: type = 0; break;
        default: break;
    }
    
    if (type != -1) {
        m_callback(fullPath, type);
    }
}

} // namespace Engine::Assets
