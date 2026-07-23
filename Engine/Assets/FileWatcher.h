#pragma once
#include <string>
#include <functional>
#include <efsw/efsw.hpp>

namespace Engine::Assets {

class FileWatcher : public efsw::FileWatchListener {
public:
    using ChangeCallback = std::function<void(const std::string& path, int type)>;

    FileWatcher();
    ~FileWatcher();

    void watch(const std::string& directory, ChangeCallback callback);

    void handleFileAction(efsw::WatchID watchid, const std::string& dir,
                          const std::string& filename, efsw::Action action,
                          const std::string& oldFilename) override;

private:
    efsw::FileWatcher* m_watcher;
    efsw::WatchID m_watchID = 0;
    ChangeCallback m_callback;
};

} // namespace Engine::Assets
