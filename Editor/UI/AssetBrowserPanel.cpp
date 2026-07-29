#include "AssetBrowserPanel.h"
#include "../../Engine/Assets/AssetDatabase.h"
#include "../../Engine/Assets/ThumbnailCache.h"
#include "../../Engine/Assets/AssetImportPipeline.h"
#include <imgui.h>
#include <filesystem>
#include <nlohmann/json.hpp>

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
    
    // Progress Overlay
    auto& progress = Engine::Assets::AssetImportPipeline::instance().getProgress();
    if (progress.completedAssets < progress.totalAssets) {
        // Calculate progress at the bottom right corner
        ImGui::SetNextWindowPos(ImVec2(ImGui::GetWindowPos().x + ImGui::GetWindowSize().x - 320, ImGui::GetWindowPos().y + ImGui::GetWindowSize().y - 80), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(300, 60), ImGuiCond_Always);
        if (ImGui::Begin("Importing Assets", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoBackground)) {
            float fraction = (float)progress.completedAssets / (float)progress.totalAssets;
            ImGui::ProgressBar(fraction, ImVec2(280, 20), "Importing...");
            ImGui::Text("%d / %d completed", progress.completedAssets.load(), progress.totalAssets.load());
            ImGui::End();
        }
    }

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

    // If we are inside an FBX
    if (m_currentFolder.starts_with("guid:")) {
        Engine::Assets::AssetGuid parentGuid = Engine::Assets::AssetGuid::fromString(m_currentFolder.substr(5));
        const auto* parentMeta = Engine::Assets::AssetDatabase::instance().find(parentGuid);
        
        if (ImGui::Button("< Back")) {
            m_currentFolder = Engine::Assets::AssetDatabase::instance().getProjectRoot() + "/Assets";
        }
        
        if (parentMeta) {
            nlohmann::json settings;
            try { settings = nlohmann::json::parse(parentMeta->importSettings); } catch (...) {}
            if (settings.contains("subAssets")) {
                for (auto& [name, guidStr] : settings["subAssets"].items()) {
                    Engine::Assets::AssetGuid subGuid = Engine::Assets::AssetGuid::fromString(guidStr.get<std::string>());
                    
                    ImGui::PushID(subGuid.toString().c_str());
                    
                    // Simple button as thumbnail placeholder
                    ImGui::Button(name.c_str(), ImVec2(cardSize, cardSize));
                    
                    if (ImGui::BeginDragDropSource()) {
                        Engine::Assets::AssetGuid dragGuid = subGuid;
                        ImGui::SetDragDropPayload("ASSET_GUID", &dragGuid, sizeof(Engine::Assets::AssetGuid));
                        ImGui::Text("%s", name.c_str());
                        ImGui::EndDragDropSource();
                    }
                    
                    ImGui::Text("%s", name.c_str());
                    
                    ImGui::PopID();
                    ImGui::NextColumn();
                }
            }
        }
        ImGui::Columns(1);
        return;
    }

    // List all assets for MVP
    auto assets = Engine::Assets::AssetDatabase::instance().getAllAssets();
    
    for (auto& guid : assets) {
        const auto* meta = Engine::Assets::AssetDatabase::instance().find(guid);
        if (meta && meta->importerType == "Virtual") continue; // Hide sub-assets from root view
        
        bgfx::TextureHandle thumb = Engine::Assets::ThumbnailCache::instance().get(guid);
        
        ImGui::PushID(guid.toString().c_str());
        
        // Thumbnail
        ImGui::ImageButton("##thumb", (ImTextureID)(uintptr_t)thumb.idx, ImVec2(cardSize, cardSize));
        
        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
            if (meta && meta->importerType == "Mesh") {
                m_currentFolder = "guid:" + guid.toString();
            }
        }
        
        // Drag source
        if (ImGui::BeginDragDropSource()) {
            Engine::Assets::AssetGuid dragGuid = guid;
            ImGui::SetDragDropPayload("ASSET_GUID", &dragGuid, sizeof(Engine::Assets::AssetGuid));
            ImGui::Text("%s", meta ? meta->relativePath.c_str() : "Unknown");
            ImGui::EndDragDropSource();
        }

        // File name text
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
