#pragma once
#include <string>
#include <vector>
#include <set>
#include <filesystem>
#include "../../Engine/Assets/AssetDatabase.h"

namespace Editor::UI {

class AssetBrowserPanel {
public:
    void initialize();
    void drawContents();

private:
    void drawTopBar(float width);
    void drawAssetFrame(float width, float height);
    void drawAssetList(float width, float height);
    void drawAssetGrid(float width, float height);
    void drawSingleCard(const Engine::Assets::AssetGuid& guid, const char* label, const char* typeLbl, bool isActive, float cardW = 92.0f, float cardH = 80.0f);

    std::string m_currentFolder;
    std::string m_selectedFolder;
    std::set<std::string> m_expandedFolders;
    
    char m_searchBuffer[128] = "";
    bool m_searchOpen = false;
};

} // namespace Editor::UI
