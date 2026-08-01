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
    void drawSingleCard(const Engine::Assets::AssetGuid& guid, const char* label, const char* typeLbl, bool isActive, float cardW = 80.0f, float cardH = 80.0f);

    std::string m_currentFolder;
};

} // namespace Editor::UI
