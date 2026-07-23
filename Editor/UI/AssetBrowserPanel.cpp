#include "AssetBrowserPanel.h"
#include "../../Engine/Assets/AssetDatabase.h"
#include "../../Engine/Assets/ThumbnailCache.h"
#include "../../Engine/Assets/AssetImportPipeline.h"
#include <imgui.h>
#include <filesystem>

namespace fs = std::filesystem;

namespace Editor::UI {

void AssetBrowserPanel::initialize() {
    m_currentFolder = Engine::Assets::AssetDatabase::instance().getProjectRoot() + "/Assets";
}

void AssetBrowserPanel::draw() {
    ImGui::Begin("Asset Browser");

    // Process background imports or pending renders
    Engine::Assets::AssetImportPipeline::instance().update();
    Engine::Assets::ThumbnailCache::instance().processPendingRenders(2);

    ImGui::BeginChild("AssetGrid");
    drawAssetGrid();
    ImGui::EndChild();

    ImGui::End();
}

void AssetBrowserPanel::drawAssetGrid() {
    float cardSize = 96.0f;
    float padding = 16.0f;
    float cellSize = cardSize + padding;

    float panelWidth = ImGui::GetContentRegionAvail().x;
    int columnCount = (int)(panelWidth / cellSize);
    if (columnCount < 1) columnCount = 1;

    ImGui::Columns(columnCount, nullptr, false);

    // List all assets for MVP
    auto assets = Engine::Assets::AssetDatabase::instance().getAllAssets();
    
    for (auto& guid : assets) {
        bgfx::TextureHandle thumb = Engine::Assets::ThumbnailCache::instance().get(guid);
        
        ImGui::PushID(guid.toString().c_str());
        
        // Thumbnail
        ImGui::ImageButton("##thumb", (ImTextureID)(uintptr_t)thumb.idx, ImVec2(cardSize, cardSize));
        
        // Drag source
        if (ImGui::BeginDragDropSource()) {
            Engine::Assets::AssetGuid dragGuid = guid;
            ImGui::SetDragDropPayload("ASSET_GUID", &dragGuid, sizeof(Engine::Assets::AssetGuid));
            ImGui::Text("%s", Engine::Assets::AssetDatabase::instance().find(guid)->relativePath.c_str());
            ImGui::EndDragDropSource();
        }

        // File name text
        const auto* meta = Engine::Assets::AssetDatabase::instance().find(guid);
        if (meta) {
            std::string filename = fs::path(meta->relativePath).filename().string();
            // Truncate if too long
            if (filename.length() > 15) filename = filename.substr(0, 12) + "...";
            
            float textWidth = ImGui::CalcTextSize(filename.c_str()).x;
            float textIndent = (cardSize - textWidth) * 0.5f;
            if (textIndent > 0) ImGui::SetCursorPosX(ImGui::GetCursorPosX() + textIndent);
            
            ImGui::Text("%s", filename.c_str());
        }

        ImGui::PopID();
        ImGui::NextColumn();
    }
    
    ImGui::Columns(1);
}

} // namespace Editor::UI
