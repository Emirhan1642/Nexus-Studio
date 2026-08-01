#pragma once
#include <string>
#include "../../Engine/Assets/AssetDatabase.h"

namespace Editor::UI {

class AssetBrowserPanel {
public:
    void initialize();
    void draw();

private:
    void drawAssetGrid();
    void drawSingleCard(
        const Engine::Assets::AssetGuid& guid,
        const char* label,
        const char* typeLbl,
        bool isActive);

    std::string m_currentFolder;
};

} // namespace Editor::UI
