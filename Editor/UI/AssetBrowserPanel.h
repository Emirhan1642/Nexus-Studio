#pragma once
#include <string>

namespace Editor::UI {

class AssetBrowserPanel {
public:
    static AssetBrowserPanel& instance() { static AssetBrowserPanel p; return p; }

    void initialize();
    void draw();

private:
    void drawAssetGrid();
    
    std::string m_currentFolder;
};

} // namespace Editor::UI
