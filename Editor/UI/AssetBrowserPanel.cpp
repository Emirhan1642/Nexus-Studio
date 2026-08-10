#include "AssetBrowserPanel.h"
#include "../../Engine/Assets/AssetDatabase.h"
#include "../../Engine/Assets/ThumbnailCache.h"
#include "../../Engine/Assets/AssetImportPipeline.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <filesystem>
#include <nlohmann/json.hpp>
#include "IconRegistry.h"
#include "NexusTheme.h"
#include "SharedTabBar.h"

namespace fs = std::filesystem;
namespace Editor::UI {

static ImU32 COL(const ImVec4& v) { return ImGui::ColorConvertFloat4ToU32(v); }
static ImU32 COLA(uint32_t hex, float a) { return IM_COL32((hex>>16)&0xFF,(hex>>8)&0xFF,hex&0xFF,(uint8_t)(a*255)); }

static int s_activeTab = 0; 

void AssetBrowserPanel::initialize() {
    m_currentFolder = Engine::Assets::AssetDatabase::instance().getProjectRoot() + "/Assets";
}

void AssetBrowserPanel::drawContents() {
    auto& T = NexusTheme::instance();
    
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float width = ImGui::GetWindowWidth();

    Engine::Assets::AssetImportPipeline::instance().update();
    Engine::Assets::ThumbnailCache::instance().processPendingRenders(2);

    if (s_activeTab != 0) {
        ImGui::BeginChild("##ABEmpty");
        ImGui::SetCursorPos(ImVec2(12, 12));
        ImGui::TextDisabled("Output log will appear here.");
        ImGui::EndChild();
        return;
    }

    // ─────────────────────────────────────────────────────────────────────────
    // ASSET FRAME (padding 5px, gap 10px)
    // ─────────────────────────────────────────────────────────────────────────
    ImGui::PushStyleColor(ImGuiCol_ChildBg, T.bgDeepest);
    ImGui::BeginChild("##AssetFrame", ImVec2(width, ImGui::GetContentRegionAvail().y), false, ImGuiWindowFlags_NoScrollbar);
    
    ImVec2 framePos = ImGui::GetCursorScreenPos();
    float frameH = ImGui::GetContentRegionAvail().y;
    
    // Padding: 5px
    float pad = 5.0f;
    float gap = 10.0f;
    
    // AssetList width: 226px
    float listW = 226.0f;
    
    // ── AssetList (Klasör Ağacı) ──
    ImGui::SetCursorScreenPos(ImVec2(framePos.x + pad, framePos.y + pad));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, NexusTheme::HexColorAlpha(0x050505, 0.0f)); // Transparent to use frame bg
    ImGui::BeginChild("##AssetList", ImVec2(listW, frameH - pad*2), false, 0);
    {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 2));

        // Project Root
        ImVec2 p = ImGui::GetCursorScreenPos();
        dl->AddRectFilled(p, {p.x+listW, p.y+22}, COL(T.panelHover), 4.0f);
        dl->AddRect(p, {p.x+listW, p.y+22}, COLA(0x242424,0.8f), 4.0f);
        ImTextureID foldTex = IconRegistry::instance().get("icon_folder");
        if (foldTex) dl->AddImage(foldTex,{p.x+6,p.y+3},{p.x+22,p.y+19});
        dl->AddText({p.x+28, p.y+4}, COL(T.textPrimary), "Project_Root");
        ImGui::Dummy(ImVec2(listW, 22));

        // Folder Tree helper lambda
        std::function<void(const fs::path&)> drawFolderTree = [&](const fs::path& currentPath) {
            for (const auto& entry : fs::directory_iterator(currentPath)) {
                if (!entry.is_directory()) continue;
                
                std::string folderName = entry.path().filename().string();
                bool isSelected = (m_currentFolder == entry.path().string());
                
                ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
                
                // Check if it has subdirectories
                bool hasSubdirs = false;
                for (const auto& sub : fs::directory_iterator(entry.path())) {
                    if (sub.is_directory()) { hasSubdirs = true; break; }
                }
                if (!hasSubdirs) flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
                if (isSelected) flags |= ImGuiTreeNodeFlags_Selected;

                bool isOpen = ImGui::TreeNodeEx(folderName.c_str(), flags, "");
                if (ImGui::IsItemClicked()) {
                    m_currentFolder = entry.path().string();
                }

                ImVec2 r = ImGui::GetItemRectMin();
                ImVec2 rMax = ImGui::GetItemRectMax();

                if (isSelected) {
                    dl->AddRectFilled(r, rMax, COLA(0x00d2ff,0.15f), 4.0f);
                    dl->AddRect(r, rMax, COLA(0x00d2ff,0.40f), 4.0f);
                }

                ImU32 fc = isSelected ? COL(T.accent) : COL(T.textMuted);
                if (foldTex) dl->AddImage(foldTex,{r.x+18,r.y+1},{r.x+34,r.y+17},{0,0},{1,1},fc);

                ImU32 tc = isSelected ? COL(T.textPrimary) : COL(T.textMuted);
                dl->AddText({r.x+38,r.y+2}, tc, folderName.c_str());

                if (isOpen && hasSubdirs) {
                    drawFolderTree(entry.path());
                    ImGui::TreePop();
                }
            }
        };

        ImGui::SetCursorPosX(12);
        ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing, 14.0f);
        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0,0,0,0));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, COL(T.panelHover));

        std::string assetsRoot = Engine::Assets::AssetDatabase::instance().getProjectRoot() + "/Assets";
        if (fs::exists(assetsRoot)) {
            drawFolderTree(assetsRoot);
        } else {
            ImGui::TextColored(T.textMuted, "No Assets folder found.");
            // Create dummy for testing if not exists
            try { fs::create_directories(assetsRoot); } catch(...) {}
        }

        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();

    // ── AssetGrid (2D Grid Önizleme) ──
    ImGui::SetCursorScreenPos(ImVec2(framePos.x + pad + listW + gap, framePos.y + pad));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, NexusTheme::HexColorAlpha(0x050505, 0.0f));
    ImGui::BeginChild("##AssetGrid", ImVec2(width - pad*2 - listW - gap, frameH - pad*2), false, 0);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 8));
    drawAssetGrid();
    ImGui::PopStyleVar();
    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::EndChild(); // End AssetFrame
    ImGui::PopStyleColor();
    
    // Bottom border of panel
    ImVec2 winP = ImGui::GetWindowPos();
    float winH = ImGui::GetWindowHeight();
    dl->AddLine(ImVec2(winP.x, winP.y + winH - 1.0f), ImVec2(winP.x + width, winP.y + winH - 1.0f), COL(T.border));
}

void AssetBrowserPanel::drawAssetGrid() {
    auto& T  = NexusTheme::instance();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    float cardW = 92.0f, cardH = 80.0f;
    float panelW = ImGui::GetContentRegionAvail().x;
    int cols = std::max(1, (int)(panelW / (cardW + 8.0f)));

    if (m_currentFolder.starts_with("guid:")) {
        Engine::Assets::AssetGuid parentGuid = Engine::Assets::AssetGuid::fromString(m_currentFolder.substr(5));
        const auto* parentMeta = Engine::Assets::AssetDatabase::instance().find(parentGuid);

        ImGui::SetCursorPosX(8);
        ImGui::PushStyleColor(ImGuiCol_Button, COL(T.panelHover));
        if (ImGui::Button("< Back")) {
            m_currentFolder = Engine::Assets::AssetDatabase::instance().getProjectRoot() + "/Assets";
        }
        ImGui::PopStyleColor();

        if (parentMeta) {
            nlohmann::json settings;
            try { settings = nlohmann::json::parse(parentMeta->importSettings); }
            catch (...) {}
            if (settings.contains("subAssets")) {
                int colIndex = 0;
                for (auto& [name, guidStr] : settings["subAssets"].items()) {
                    Engine::Assets::AssetGuid sg = Engine::Assets::AssetGuid::fromString(guidStr.get<std::string>());
                    ImGui::PushID(sg.toString().c_str());
                    drawSingleCard(sg, name.c_str(), "Mesh", false, cardW, cardH);
                    ImGui::PopID();
                    
                    colIndex++;
                    if (colIndex < cols) ImGui::SameLine(0, 8.0f);
                    else colIndex = 0;
                }
                if (colIndex != 0) ImGui::NewLine();
            }
        }
        return;
    }

    auto assets = Engine::Assets::AssetDatabase::instance().getAllAssets();
    
    int colIndex = 0;
    for (auto& guid : assets) {
        const auto* meta = Engine::Assets::AssetDatabase::instance().find(guid);
        if (meta && meta->importerType == "Virtual") continue;

        // Check if asset is inside m_currentFolder
        fs::path assetAbsPath = Engine::Assets::AssetDatabase::instance().getAbsolutePath(meta ? meta->relativePath : "");
        if (meta && assetAbsPath.parent_path().string() != m_currentFolder) continue;

        std::string fname = meta ? fs::path(meta->relativePath).filename().string() : "Unknown";
        std::string typeStr = meta ? meta->importerType : "?";

        ImGui::PushID(guid.toString().c_str());
        drawSingleCard(guid, fname.c_str(), typeStr.c_str(), false, cardW, cardH);
        ImGui::PopID();
        
        colIndex++;
        if (colIndex < cols) ImGui::SameLine(0, 8.0f);
        else colIndex = 0;
    }
    if (colIndex != 0) ImGui::NewLine();
}

void AssetBrowserPanel::drawSingleCard(const Engine::Assets::AssetGuid& guid, const char* label, const char* typeLbl, bool isActive, float cardW, float cardH) {
    auto& T  = NexusTheme::instance();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    ImVec2 p = ImGui::GetCursorScreenPos();
    ImVec2 pMax= {p.x + cardW, p.y + cardH};

    ImGui::InvisibleButton(label, ImVec2(cardW, cardH));
    bool hov = ImGui::IsItemHovered();

    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
        const auto* meta = Engine::Assets::AssetDatabase::instance().find(guid);
        if (meta && meta->importerType == "Mesh")
            m_currentFolder = "guid:" + guid.toString();
    }

    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
        Engine::Assets::AssetGuid dragGuid = guid;
        ImGui::SetDragDropPayload("ASSET_GUID", &dragGuid, sizeof(Engine::Assets::AssetGuid));
        ImGui::Text("%s", label);
        ImGui::EndDragDropSource();
    }

    ImU32 bgCol     = COL(T.bgPanel);
    ImU32 borderCol = isActive ? COLA(0x00d2ff,1.0f) : (hov ? COLA(0x00d2ff,0.7f) : COL(T.border));
    float borderW   = isActive ? 2.0f : 1.0f;

    dl->AddRectFilled(p, pMax, bgCol, 8.0f);
    dl->AddRect(p, pMax, borderCol, 8.0f, 0, borderW);

    if (isActive) dl->AddRectFilled({p.x-2,p.y-2},{pMax.x+2,pMax.y+2}, COLA(0x00d2ff,0.08f), 10.0f);

    float thumbS = 32.0f;
    float thumbX = p.x + (cardW - thumbS) * 0.5f;
    float thumbY = p.y + 8.0f;

    bgfx::TextureHandle thumb = Engine::Assets::ThumbnailCache::instance().get(guid);

    if (thumb.idx != bgfx::kInvalidHandle) {
        dl->AddImageRounded((ImTextureID)(uintptr_t)thumb.idx, {thumbX, thumbY}, {thumbX+thumbS, thumbY+thumbS}, {0,0},{1,1}, IM_COL32_WHITE, thumbS * 0.4f);
    } else {
        ImU32 typeColor = COLA(0x00d2ff, 0.2f);
        const char* iconKey = "icon_mesh";
        if (std::string(typeLbl).find("Script") != std::string::npos || std::string(label).ends_with(".luau")) { typeColor = COLA(0x14B8A6, 0.2f); iconKey = "icon_script"; }
        else if (std::string(typeLbl).find("Material") != std::string::npos || std::string(label).ends_with(".mat")) { typeColor = COLA(0xF59E0B, 0.2f); iconKey = "icon_material"; }
        else if (std::string(label).ends_with(".png") || std::string(label).ends_with(".jpg")) { typeColor = COLA(0x8B5CF6, 0.2f); iconKey = "icon_particle"; }

        dl->AddRectFilled({thumbX,thumbY},{thumbX+thumbS,thumbY+thumbS}, typeColor, thumbS*0.4f);
        ImTextureID icn = IconRegistry::instance().get(iconKey);
        if (icn) {
            float is = thumbS * 0.55f;
            float ix = thumbX + (thumbS-is)*0.5f;
            float iy = thumbY + (thumbS-is)*0.5f;
            dl->AddImage(icn,{ix,iy},{ix+is,iy+is},{0,0},{1,1},COL(T.textMuted));
        }
    }

    std::string name = label;
    if (name.size() > 14) name = name.substr(0,11) + "...";
    float nw = ImGui::CalcTextSize(name.c_str()).x;
    dl->AddText({p.x+(cardW-nw)*0.5f, p.y+cardH-34}, COL(T.textPrimary), name.c_str());

    float tw = ImGui::CalcTextSize(typeLbl).x;
    ImU32 typeTextCol = isActive ? COL(T.accent) : COL(T.textMuted);
    dl->AddText({p.x+(cardW-tw)*0.5f, p.y+cardH-20}, typeTextCol, typeLbl);

    if (isActive) {
        const char* badge = "Active";
        float bw = ImGui::CalcTextSize(badge).x + 8;
        dl->AddRectFilled({p.x+4,p.y+4},{p.x+4+bw,p.y+16}, COLA(0x00d2ff,0.20f), 4.0f);
        dl->AddText({p.x+8,p.y+4}, COL(T.accent), badge);
    }

    ImGui::Dummy(ImVec2(0,4));
}
}
