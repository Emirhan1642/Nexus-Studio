#include "AssetBrowserPanel.h"
#include "../../Engine/Assets/AssetDatabase.h"
#include "../../Engine/Assets/ThumbnailCache.h"
#include "../../Engine/Assets/AssetImportPipeline.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <filesystem>
#include <nlohmann/json.hpp>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include "IconRegistry.h"
#include "NexusTheme.h"
#include "EditorLayout.h"

namespace fs = std::filesystem;
namespace Editor::UI {

static ImU32 COL(const ImVec4& v) { return ImGui::ColorConvertFloat4ToU32(v); }
static ImU32 COLA(uint32_t hex, float a) {
    return IM_COL32((hex >> 16) & 0xFF, (hex >> 8) & 0xFF, hex & 0xFF, (uint8_t)(a * 255));
}

static std::string normalizePath(const std::string& p) {
    if (p.empty()) return "";
    std::string s = fs::path(p).generic_string();
    while (s.size() > 1 && s.back() == '/') s.pop_back();
    return s;
}

void AssetBrowserPanel::initialize() {
    std::string root = Engine::Assets::AssetDatabase::instance().getProjectRoot();
    m_currentFolder = normalizePath(root + "/Assets");
    m_selectedFolder = m_currentFolder;
    m_expandedFolders.insert(m_currentFolder);
}

void AssetBrowserPanel::drawContents() {
    auto& T = NexusTheme::instance();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float width = ImGui::GetWindowWidth();
    float height = ImGui::GetWindowHeight();

    Engine::Assets::AssetImportPipeline::instance().update();
    Engine::Assets::ThumbnailCache::instance().processPendingRenders(2);

    ImGuiWindow* window = ImGui::GetCurrentWindow();
    ImGuiDockNode* node = window->DockNode;
    ImGuiWindow* vpWindow = ImGui::FindWindowByName("Viewport");
    bool dockedWithViewport = (vpWindow && node && vpWindow->DockNode == node);

    if (!dockedWithViewport) {
        // ─────────────────────────────────────────────────────────────────────────
        // 1. FIGMA TOPBAR (h = 30px, bg: #0E0E0E, border: #242424)
        // ─────────────────────────────────────────────────────────────────────────
        drawTopBar(width);

        // ─────────────────────────────────────────────────────────────────────────
        // 2. MAIN BODY (AssetFrame)
        // ─────────────────────────────────────────────────────────────────────────
        float bodyH = ImGui::GetContentRegionAvail().y;
        if (bodyH < 10.0f) bodyH = height - 30.0f;

        ImGui::SetCursorPos(ImVec2(0, 30.0f));
        drawAssetFrame(width, bodyH);
    } else {
        float bodyH = ImGui::GetContentRegionAvail().y;
        drawAssetFrame(width, bodyH);
    }

    // Panel Outer Border Lines
    ImVec2 winP = ImGui::GetWindowPos();
    dl->AddLine(ImVec2(winP.x, winP.y + height - 1.0f), ImVec2(winP.x + width, winP.y + height - 1.0f), COL(T.border));
    dl->AddLine(ImVec2(winP.x + width - 1.0f, winP.y), ImVec2(winP.x + width - 1.0f, winP.y + height), COL(T.border));
}

void AssetBrowserPanel::drawTopBar(float width) {
    auto& T = NexusTheme::instance();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 basePos = ImGui::GetWindowPos();
    float H = 30.0f;

    // TopBar Background & Border
    dl->AddRectFilled(basePos, ImVec2(basePos.x + width, basePos.y + H), COL(T.bgPanel));
    dl->AddLine(ImVec2(basePos.x, basePos.y + H - 1.0f), ImVec2(basePos.x + width, basePos.y + H - 1.0f), COL(T.border));

    ImGuiContext& g = *GImGui;
    ImGuiWindow* window = g.CurrentWindow;
    ImGuiDockNode* node = window->DockNode;

    // ─────────────────────────────────────────────────────────────────────────
    // LEFT SIDE: TABS (Deterministically sorted: Asset Manager -> Console -> Material Editor)
    // ─────────────────────────────────────────────────────────────────────────
    float curX = basePos.x + 10.0f;
    float iconSize = 18.0f;

    ImGuiIO& io = ImGui::GetIO();
    bool pushedFont = false;
    if (io.Fonts->Fonts.Size > 1) {
        ImGui::PushFont(io.Fonts->Fonts[1]);
        pushedFont = true;
    }

    std::vector<ImGuiWindow*> displayWindows;
    if (node && node->Windows.Size > 0) {
        for (int i = 0; i < node->Windows.Size; i++) {
            displayWindows.push_back(node->Windows[i]);
        }
    } else {
        displayWindows.push_back(window);
    }

    auto getTabPriority = [](const std::string& name) -> int {
        if (name.find("Asset") != std::string::npos) return 0;
        if (name.find("Console") != std::string::npos) return 1;
        if (name.find("Material") != std::string::npos) return 2;
        if (name.find("Explorer") != std::string::npos) return 3;
        if (name.find("Properties") != std::string::npos) return 4;
        return 10;
    };

    std::sort(displayWindows.begin(), displayWindows.end(), [&](ImGuiWindow* a, ImGuiWindow* b) {
        int pa = getTabPriority(a->Name);
        int pb = getTabPriority(b->Name);
        if (pa != pb) return pa < pb;
        return std::string(a->Name) < std::string(b->Name);
    });

    for (size_t k = 0; k < displayWindows.size(); k++) {
        ImGuiWindow* tabWin = displayWindows[k];
        bool isActive = (tabWin == window);

        const char* sLabel = tabWin->Name;
        const char* sIcon = "icon_folder_bold";
        if (strstr(sLabel, "Asset Browser")) {
            sLabel = "Asset Manager";
            sIcon = "icon_folder_bold";
        } else if (strstr(sLabel, "Console")) {
            sIcon = "icon_script_bold";
        } else if (strstr(sLabel, "Material")) {
            sIcon = "icon_node_editor_bold";
        } else if (strstr(sLabel, "Explorer")) {
            sIcon = "icon_explorer_bold";
        }

        ImVec2 textSz = ImGui::CalcTextSize(sLabel);
        float tabW = 20.0f + 6.0f + textSz.x + 10.0f;
        ImVec2 tMin = ImVec2(curX, basePos.y);
        ImVec2 tMax = ImVec2(curX + tabW, basePos.y + H);

        window->IDStack.push_back(tabWin->ID);
        ImGuiID tabBtnId = window->GetID(sLabel);
        ImGui::SetCursorScreenPos(tMin);
        ImGui::InvisibleButton(sLabel, ImVec2(tabW, H));
        window->IDStack.pop_back();

        if (ImGui::IsItemClicked()) {
            ImGui::FocusWindow(tabWin);
            if (g.ActiveId == tabBtnId) {
                g.ActiveIdWindow = tabWin;
            }
            if (node) {
                node->SelectedTabId = tabWin->TabId;
                if (node->TabBar) {
                    node->TabBar->NextSelectedTabId = tabWin->TabId;
                    node->TabBar->SelectedTabId = tabWin->TabId;
                }
                node->VisibleWindow = tabWin;
            }
        }

        if (ImGui::IsItemActive() && ImGui::IsMouseDragging(0)) {
            if (tabWin->DockNode) {
                ImGui::DockContextQueueUndockWindow(GImGui, tabWin);
            }

            ImGui::StartMouseMovingWindow(tabWin);
            g.MovingWindow = tabWin;

            float offsetX = g.IO.MouseClickedPos[0].x - tabWin->Pos.x;
            g.ActiveIdClickOffset = ImVec2(offsetX, 0.0f);
            break;
        }

        bool hov = ImGui::IsItemHovered();
        if (hov && !isActive) {
            dl->AddRectFilled(tMin, tMax, COLA(0xFFFFFF, 0.04f), 4.0f);
        }

        ImTextureID iconTex = IconRegistry::instance().get(sIcon);
        float iy = tMin.y + (H - iconSize) * 0.5f;
        ImU32 colIcon = isActive ? COL(T.textPrimary) : (hov ? COL(T.textPrimary) : COL(T.textMuted));
        if (iconTex) {
            dl->AddImage(iconTex, ImVec2(curX, iy), ImVec2(curX + iconSize, iy + iconSize), ImVec2(0,0), ImVec2(1,1), colIcon);
        }

        ImU32 colText = isActive ? COL(T.textPrimary) : (hov ? COL(T.textPrimary) : COL(T.textMuted));
        dl->AddText(ImVec2(curX + iconSize + 6.0f, tMin.y + (H - textSz.y) * 0.5f), colText, sLabel);

        curX += tabW + 6.0f;

        if (k < displayWindows.size() - 1) {
            dl->AddRectFilled(ImVec2(curX, basePos.y + 5.0f), ImVec2(curX + 3.0f, basePos.y + 25.0f), COLA(0xFFFFFF, 0.12f), 2.0f);
            curX += 3.0f + 6.0f;
        }
    }

    if (pushedFont) ImGui::PopFont();

    // ─────────────────────────────────────────────────────────────────────────
    // RIGHT SIDE: ADAPTIVE / RESPONSIVE ACTION BUTTONS & PROJECT INFO
    // ─────────────────────────────────────────────────────────────────────────
    float tabsEndX = curX + 15.0f; // End of tabs with safety margin
    float rightX = basePos.x + width - 10.0f;
    float availRightW = rightX - tabsEndX;

    // 1. Project Name & Cloud Status Icon (Only if plenty of space: availRightW >= 340px)
    if (availRightW >= 340.0f) {
        std::string projName = "Nexus/Project";
        std::string root = Engine::Assets::AssetDatabase::instance().getProjectRoot();
        if (!root.empty()) {
            std::error_code ec;
            fs::path rp(root);
            if (fs::exists(rp, ec)) {
                projName = "Nexus/" + rp.filename().string();
            }
        }

        float cloudIconSz = 18.0f;
        ImVec2 projTextSz = ImGui::CalcTextSize(projName.c_str());
        float projGroupW = projTextSz.x + 8.0f + cloudIconSz;

        rightX -= projGroupW;
        dl->AddText(ImVec2(rightX, basePos.y + (H - projTextSz.y) * 0.5f), COL(T.textPrimary), projName.c_str());

        ImTextureID cloudTex = IconRegistry::instance().get("icon_svr_bold");
        if (!cloudTex) cloudTex = IconRegistry::instance().get("icon_folder_bold");
        if (cloudTex) {
            float iy = basePos.y + (H - cloudIconSz) * 0.5f;
            dl->AddImage(cloudTex, ImVec2(rightX + projTextSz.x + 8.0f, iy), ImVec2(rightX + projTextSz.x + 8.0f + cloudIconSz, iy + cloudIconSz), ImVec2(0,0), ImVec2(1,1), COL(T.textPrimary));
        }

        // 2px Capsule Vertical Divider
        rightX -= 12.0f;
        dl->AddRectFilled(ImVec2(rightX, basePos.y + 5.0f), ImVec2(rightX + 2.0f, basePos.y + 25.0f), COLA(0xFFFFFF, 0.12f), 1.0f);
        rightX -= 12.0f;
    }

    // 2. Search Box (Full input box if space >= 170px, else compact Search Icon Button)
    float remainingForSearchAndAdd = rightX - tabsEndX;
    if (remainingForSearchAndAdd >= 170.0f) {
        float searchInputW = std::min(130.0f, remainingForSearchAndAdd - 36.0f);
        rightX -= searchInputW;
        ImGui::SetCursorScreenPos(ImVec2(rightX, basePos.y + 4.0f));
        ImGui::PushItemWidth(searchInputW);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6, 3));
        ImGui::PushStyleColor(ImGuiCol_FrameBg, COL(T.bgDeep));
        ImGui::PushStyleColor(ImGuiCol_Border, COL(T.border));
        ImGui::InputTextWithHint("##AssetSearch", "Search assets...", m_searchBuffer, sizeof(m_searchBuffer));
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);
        ImGui::PopItemWidth();
    } else if (remainingForSearchAndAdd >= 55.0f) {
        // Compact search icon button (Click opens mini search popup)
        rightX -= 22.0f;
        ImGui::SetCursorScreenPos(ImVec2(rightX, basePos.y + 5.0f));
        ImTextureID searchTex = IconRegistry::instance().get("icon_search_bold");
        if (!searchTex) searchTex = IconRegistry::instance().get("icon_search");
        if (ImGui::InvisibleButton("##SearchBtn", ImVec2(20, 20))) {
            ImGui::OpenPopup("##AssetSearchPopup");
        }
        bool searchHov = ImGui::IsItemHovered();
        if (searchTex) {
            dl->AddImage(searchTex, ImVec2(rightX, basePos.y + 5.0f), ImVec2(rightX + 20.0f, basePos.y + 25.0f), ImVec2(0,0), ImVec2(1,1), (searchHov || m_searchBuffer[0] != '\0') ? COL(T.accent) : COL(T.textMuted));
        }
        if (searchHov) ImGui::SetTooltip("Search Assets");

        ImGui::SetNextWindowPos(ImVec2(rightX - 140.0f, basePos.y + H + 2.0f));
        if (ImGui::BeginPopup("##AssetSearchPopup")) {
            ImGui::PushItemWidth(160.0f);
            if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
            ImGui::InputTextWithHint("##PopupSearch", "Search assets...", m_searchBuffer, sizeof(m_searchBuffer));
            ImGui::PopItemWidth();
            ImGui::EndPopup();
        }
    }

    // 3. Add Button (Only if it safely fits without touching tabs)
    if (rightX - 28.0f >= tabsEndX) {
        rightX -= 28.0f;
        ImGui::SetCursorScreenPos(ImVec2(rightX, basePos.y + 5.0f));
        ImTextureID addTex = IconRegistry::instance().get("icon_add");
        if (!addTex) addTex = IconRegistry::instance().get("icon_add_bold");
        if (ImGui::InvisibleButton("##AddBtn", ImVec2(20, 20))) {
            // Quick add dialog or import trigger
        }
        bool addHov = ImGui::IsItemHovered();
        if (addTex) {
            dl->AddImage(addTex, ImVec2(rightX, basePos.y + 5.0f), ImVec2(rightX + 20.0f, basePos.y + 25.0f), ImVec2(0,0), ImVec2(1,1), addHov ? COL(T.accent) : COL(T.textMuted));
        }
        if (addHov) ImGui::SetTooltip("Add Asset / Create New");
    }
}

void AssetBrowserPanel::drawAssetFrame(float width, float height) {
    auto& T = NexusTheme::instance();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    float pad = 5.0f;
    float listW = 226.0f;
    float divW = 3.0f;
    float gap = 8.0f;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, T.bgDeepest);
    ImGui::BeginChild("##AssetFrameContainer", ImVec2(width, height), false, ImGuiWindowFlags_NoScrollbar);

    ImVec2 fPos = ImGui::GetCursorScreenPos();

    // ── Left: AssetList (226px) ──
    ImGui::SetCursorScreenPos(ImVec2(fPos.x + pad, fPos.y + pad));
    drawAssetList(listW, height - pad * 2.0f);

    // ── Center: 3px Capsule Vertical Divider (Rectangle 14 in Figma) ──
    float divX = fPos.x + pad + listW + gap * 0.5f - 1.5f;
    dl->AddRectFilled(ImVec2(divX, fPos.y + pad + 2.0f), ImVec2(divX + divW, fPos.y + height - pad - 2.0f), COLA(0xFFFFFF, 0.10f), 2.0f);

    // ── Right: AssetGrid / Detail View ──
    float gridX = fPos.x + pad + listW + gap + divW;
    float gridW = width - (gridX - fPos.x) - pad;
    ImGui::SetCursorScreenPos(ImVec2(gridX, fPos.y + pad));
    drawAssetGrid(gridW, height - pad * 2.0f);

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

void AssetBrowserPanel::drawAssetList(float width, float height) {
    auto& T = NexusTheme::instance();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    ImGui::PushStyleColor(ImGuiCol_ChildBg, NexusTheme::HexColorAlpha(0x000000, 0.0f));
    ImGui::BeginChild("##AssetListScroll", ImVec2(width, height), false, 0);

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0.0f));

    std::string rootAssets = normalizePath(Engine::Assets::AssetDatabase::instance().getProjectRoot() + "/Assets");
    std::error_code ec;
    if (!fs::exists(rootAssets, ec)) {
        try { fs::create_directories(rootAssets, ec); } catch (...) {}
    }

    // Helper for category badge color & type icon
    auto getItemVisuals = [](const std::string& name, bool isDir) -> std::pair<ImU32, const char*> {
        if (isDir) {
            std::string lower = name;
            std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
            if (lower.find("model") != std::string::npos) return {COLA(0xA7FF71, 1.0f), "icon_folder_bold"}; // Lime
            if (lower.find("mat") != std::string::npos)   return {COLA(0xFD71FF, 1.0f), "icon_folder_bold"}; // Magenta
            if (lower.find("script") != std::string::npos)return {COLA(0x66FF99, 1.0f), "icon_folder_bold"}; // Green
            if (lower.find("sound") != std::string::npos) return {COLA(0x9CA3AF, 1.0f), "icon_folder_bold"};
            return {COLA(0xA7FF71, 1.0f), "icon_folder_bold"};
        }
        if (name.ends_with(".luau") || name.ends_with(".lua") || name.ends_with(".cpp")) {
            return {COLA(0x66FF99, 1.0f), "icon_script_bold"};
        }
        if (name.ends_with(".mat") || name.ends_with(".material")) {
            return {COLA(0xFD71FF, 1.0f), "icon_material_bold"};
        }
        if (name.ends_with(".png") || name.ends_with(".jpg") || name.ends_with(".tga") || name.ends_with(".fx")) {
            return {COLA(0xFFDD6C, 1.0f), "icon_fx_bold"};
        }
        if (name.ends_with(".fbx") || name.ends_with(".obj") || name.ends_with(".gltf") || name.ends_with(".mesh")) {
            return {COLA(0x82D9FF, 1.0f), "icon_mesh_bold"};
        }
        return {COLA(0x9CA3AF, 1.0f), "icon_box"};
    };

    // Recursive tree renderer with complete exception safety & modern highlight
    std::function<void(const fs::path&, int)> renderTreeItem = [&](const fs::path& currentPath, int depth) {
        std::error_code dirEc;
        if (!fs::exists(currentPath, dirEc) || !fs::is_directory(currentPath, dirEc)) return;

        try {
            for (const auto& entry : fs::directory_iterator(currentPath, dirEc)) {
                if (dirEc) break;

                std::error_code isDirEc;
                bool isDir = entry.is_directory(isDirEc);
                std::string name = entry.path().filename().string();
                std::string fullPath = normalizePath(entry.path().string());

                // Skip hidden, meta files, and editor-internal folders from root
                if (name.starts_with(".") || name.ends_with(".meta")) continue;
                if (currentPath == rootAssets) {
                    if (name == "Fonts" || name == "Icons" || name == "Shaders" || name == "Internal") continue;
                }

                float rowH = 20.0f;
                ImVec2 rowPos = ImGui::GetCursorScreenPos();
                ImVec2 rowMax = ImVec2(rowPos.x + width, rowPos.y + rowH);

                bool isSelected = (normalizePath(m_selectedFolder) == fullPath);
                bool isExpanded = (m_expandedFolders.find(fullPath) != m_expandedFolders.end());

                // Interaction button
                std::string btnId = "##TreeItem_" + fullPath;
                ImGui::InvisibleButton(btnId.c_str(), ImVec2(width, rowH));
                bool isHovered = ImGui::IsItemHovered();

                if (isHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                    m_selectedFolder = fullPath;
                    if (isDir) {
                        m_currentFolder = fullPath;
                    } else {
                        std::error_code pEc;
                        auto parentP = entry.path().parent_path();
                        if (fs::exists(parentP, pEc) && fs::is_directory(parentP, pEc)) {
                            m_currentFolder = normalizePath(parentP.string());
                        }
                    }
                }

                // Explicit double click guard ONLY when this row is hovered
                if (isDir && isHovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                    if (isExpanded) m_expandedFolders.erase(fullPath);
                    else m_expandedFolders.insert(fullPath);
                }

                // Modern Highlight Styling
                if (isSelected) {
                    dl->AddRectFilled(rowPos, rowMax, COLA(0xFFFFFF, 0.08f), 4.0f);
                    dl->AddRectFilled(ImVec2(rowPos.x + 2.0f, rowPos.y + 3.0f), ImVec2(rowPos.x + 5.0f, rowPos.y + rowH - 3.0f), COL(T.accent), 2.0f);
                } else if (isHovered) {
                    dl->AddRectFilled(rowPos, rowMax, COLA(0xFFFFFF, 0.04f), 4.0f);
                }

                // Draw Indent Hierarchy Guide Lines (1px rgba(255,255,255,0.20))
                float indentStep = 14.0f;
                for (int d = 0; d < depth; ++d) {
                    float lineX = rowPos.x + 8.0f + d * indentStep;
                    dl->AddLine(ImVec2(lineX, rowPos.y), ImVec2(lineX, rowPos.y + rowH), COLA(0xFFFFFF, 0.20f), 1.0f);
                }

                float contentX = rowPos.x + 8.0f + depth * indentStep;

                // Chevron Icon (If Directory)
                if (isDir) {
                    ImTextureID chevTex = IconRegistry::instance().get(isExpanded ? "icon_chevron_down_bold" : "icon_chevron_right_bold");
                    if (chevTex) {
                        dl->AddImage(chevTex, ImVec2(contentX, rowPos.y + 3.0f), ImVec2(contentX + 14.0f, rowPos.y + 17.0f), ImVec2(0,0), ImVec2(1,1), COL(T.textPrimary));
                    }
                    if (isHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && ImGui::GetMousePos().x < contentX + 16.0f) {
                        if (isExpanded) m_expandedFolders.erase(fullPath);
                        else m_expandedFolders.insert(fullPath);
                    }
                    contentX += 16.0f;
                } else {
                    contentX += 6.0f;
                }

                // Category Color Badge Pill (3px x 10px rounded 2px)
                auto [badgeCol, iconKey] = getItemVisuals(name, isDir);
                dl->AddRectFilled(ImVec2(contentX, rowPos.y + 5.0f), ImVec2(contentX + 3.0f, rowPos.y + 15.0f), badgeCol, 2.0f);
                contentX += 7.0f;

                // Type Icon (16x16)
                ImTextureID typeIcon = IconRegistry::instance().get(iconKey);
                if (typeIcon) {
                    dl->AddImage(typeIcon, ImVec2(contentX, rowPos.y + 2.0f), ImVec2(contentX + 16.0f, rowPos.y + 18.0f), ImVec2(0,0), ImVec2(1,1), isDir ? badgeCol : COL(T.textPrimary));
                }
                contentX += 20.0f;

                // Text Label (10px Inter)
                ImU32 textCol = isSelected ? COL(T.textPrimary) : (isHovered ? COL(T.textPrimary) : COL(T.textSecondary));
                dl->AddText(ImVec2(contentX, rowPos.y + 3.0f), textCol, name.c_str());

                // Expand Subtree
                if (isDir && isExpanded) {
                    renderTreeItem(entry.path(), depth + 1);
                }
            }
        } catch (...) {}
    };

    // Root Workspace Folder Node ("Assets")
    {
        ImVec2 rPos = ImGui::GetCursorScreenPos();
        ImVec2 rMax = ImVec2(rPos.x + width, rPos.y + 22.0f);

        bool isRootSelected = (normalizePath(m_currentFolder) == rootAssets);
        bool isRootExpanded = (m_expandedFolders.find(rootAssets) != m_expandedFolders.end());

        ImGui::InvisibleButton("##RootWorkspace", ImVec2(width, 22.0f));
        bool isRootHovered = ImGui::IsItemHovered();

        if (isRootHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            m_selectedFolder = rootAssets;
            m_currentFolder = rootAssets;
            if (ImGui::GetMousePos().x < rPos.x + 22.0f) {
                if (isRootExpanded) m_expandedFolders.erase(rootAssets);
                else m_expandedFolders.insert(rootAssets);
            }
        }
        if (isRootHovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
            if (isRootExpanded) m_expandedFolders.erase(rootAssets);
            else m_expandedFolders.insert(rootAssets);
        }

        if (isRootSelected) {
            dl->AddRectFilled(rPos, rMax, COLA(0xFFFFFF, 0.08f), 4.0f);
            dl->AddRectFilled(ImVec2(rPos.x + 2.0f, rPos.y + 3.0f), ImVec2(rPos.x + 5.0f, rPos.y + 19.0f), COL(T.accent), 2.0f);
        } else if (isRootHovered) {
            dl->AddRectFilled(rPos, rMax, COLA(0xFFFFFF, 0.04f), 4.0f);
        }

        ImTextureID chevTex = IconRegistry::instance().get(isRootExpanded ? "icon_chevron_down_bold" : "icon_chevron_right_bold");
        if (chevTex) dl->AddImage(chevTex, ImVec2(rPos.x + 6.0f, rPos.y + 4.0f), ImVec2(rPos.x + 20.0f, rPos.y + 18.0f), ImVec2(0,0), ImVec2(1,1), COL(T.textPrimary));

        ImTextureID foldTex = IconRegistry::instance().get("icon_folder_bold");
        if (foldTex) dl->AddImage(foldTex, ImVec2(rPos.x + 24.0f, rPos.y + 3.0f), ImVec2(rPos.x + 40.0f, rPos.y + 19.0f), ImVec2(0,0), ImVec2(1,1), COL(T.textPrimary));

        dl->AddText(ImVec2(rPos.x + 44.0f, rPos.y + 4.0f), COL(T.textPrimary), "Assets");

        if (isRootExpanded) {
            renderTreeItem(rootAssets, 1);
        }
    }

    ImGui::PopStyleVar();
    ImGui::EndChild();
    ImGui::PopStyleColor();
}

void AssetBrowserPanel::drawAssetGrid(float width, float height) {
    auto& T = NexusTheme::instance();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    ImGui::PushStyleColor(ImGuiCol_ChildBg, NexusTheme::HexColorAlpha(0x000000, 0.0f));
    ImGui::BeginChild("##AssetGridPane", ImVec2(width, height), false, 0);

    // ─────────────────────────────────────────────────────────────────────────
    // Asset Cards Grid (Clean layout starting at the top)
    // ─────────────────────────────────────────────────────────────────────────
    float cardW = 92.0f;
    float cardH = 80.0f;
    float spacing = 8.0f;
    int cols = std::max(1, (int)((width - 12.0f) / (cardW + spacing)));

    // Right-Click Context Menu for current folder
    if (ImGui::BeginPopupContextWindow("##AssetGridContextMenu", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems)) {
        if (ImGui::MenuItem("Create Script (.luau)")) {
            std::string newScriptPath = m_currentFolder + "/NewScript.luau";
        }
        if (ImGui::MenuItem("Create Material (.mat)")) {
            std::string newMatPath = m_currentFolder + "/NewMaterial.mat";
        }
        if (ImGui::MenuItem("Create New Folder")) {
            std::string newFolderPath = m_currentFolder + "/NewFolder";
            std::error_code mkEc;
            try { fs::create_directories(newFolderPath, mkEc); } catch (...) {}
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Show in File Explorer")) {
            #if defined(_WIN32)
            std::string cmd = "explorer \"" + m_currentFolder + "\"";
            system(cmd.c_str());
            #endif
        }
        ImGui::EndPopup();
    }

    std::string rootAssets = normalizePath(Engine::Assets::AssetDatabase::instance().getProjectRoot() + "/Assets");
    auto assets = Engine::Assets::AssetDatabase::instance().getAllAssets();
    int colIndex = 0;
    int totalDrawn = 0;
    std::string searchLower = m_searchBuffer;
    std::transform(searchLower.begin(), searchLower.end(), searchLower.begin(), ::tolower);

    // 1. Draw subfolders first in the grid (excluding internal folders from root)
    std::error_code fldEc;
    if (fs::exists(m_currentFolder, fldEc) && fs::is_directory(m_currentFolder, fldEc)) {
        try {
            for (const auto& entry : fs::directory_iterator(m_currentFolder, fldEc)) {
                if (fldEc) break;
                if (!entry.is_directory(fldEc)) continue;

                std::string folderName = entry.path().filename().string();
                if (folderName.starts_with(".")) continue;
                if (normalizePath(m_currentFolder) == rootAssets) {
                    if (folderName == "Fonts" || folderName == "Icons" || folderName == "Shaders" || folderName == "Internal") continue;
                }

                if (!searchLower.empty()) {
                    std::string fnLower = folderName;
                    std::transform(fnLower.begin(), fnLower.end(), fnLower.begin(), ::tolower);
                    if (fnLower.find(searchLower) == std::string::npos) continue;
                }

                ImGui::PushID(folderName.c_str());
                ImVec2 cp = ImGui::GetCursorScreenPos();
                ImVec2 cpMax = ImVec2(cp.x + cardW, cp.y + cardH);

                ImGui::InvisibleButton("##FolderCard", ImVec2(cardW, cardH));
                bool hov = ImGui::IsItemHovered();
                if (hov && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                    m_currentFolder = normalizePath(entry.path().string());
                }

                dl->AddRectFilled(cp, cpMax, COL(T.bgPanel), 8.0f);
                dl->AddRect(cp, cpMax, hov ? COL(T.accent) : COL(T.border), 8.0f);

                float iconSz = 32.0f;
                float iconX = cp.x + (cardW - iconSz) * 0.5f;
                float iconY = cp.y + 8.0f;
                ImTextureID fTex = IconRegistry::instance().get("icon_folder_bold");
                if (fTex) dl->AddImage(fTex, ImVec2(iconX, iconY), ImVec2(iconX + iconSz, iconY + iconSz), ImVec2(0,0), ImVec2(1,1), COL(T.accentLime));

                std::string dName = folderName;
                if (dName.size() > 12) dName = dName.substr(0, 9) + "...";
                float nw = ImGui::CalcTextSize(dName.c_str()).x;
                dl->AddText(ImVec2(cp.x + (cardW - nw) * 0.5f, cp.y + cardH - 32.0f), COL(T.textPrimary), dName.c_str());

                float tw = ImGui::CalcTextSize("Folder").x;
                dl->AddText(ImVec2(cp.x + (cardW - tw) * 0.5f, cp.y + cardH - 18.0f), COL(T.textMuted), "Folder");

                ImGui::PopID();

                totalDrawn++;
                colIndex++;
                if (colIndex < cols) ImGui::SameLine(0, spacing);
                else { colIndex = 0; ImGui::NewLine(); }
            }
        } catch (...) {}
    }

    // 2. Draw asset files
    std::string curFolderNorm = normalizePath(m_currentFolder);
    for (auto& guid : assets) {
        const auto* meta = Engine::Assets::AssetDatabase::instance().find(guid);
        if (meta && meta->importerType == "Virtual") continue;

        fs::path assetAbsPath = Engine::Assets::AssetDatabase::instance().getAbsolutePath(meta ? meta->relativePath : "");
        std::string assetParent = normalizePath(assetAbsPath.parent_path().string());
        if (meta && assetParent != curFolderNorm && !m_currentFolder.starts_with("guid:")) {
            continue;
        }

        std::string fname = meta ? fs::path(meta->relativePath).filename().string() : "Unknown";
        std::string typeStr = meta ? meta->importerType : "Asset";

        if (!searchLower.empty()) {
            std::string fnLower = fname;
            std::transform(fnLower.begin(), fnLower.end(), fnLower.begin(), ::tolower);
            if (fnLower.find(searchLower) == std::string::npos) continue;
        }

        ImGui::PushID(guid.toString().c_str());
        drawSingleCard(guid, fname.c_str(), typeStr.c_str(), false, cardW, cardH);
        ImGui::PopID();

        totalDrawn++;
        colIndex++;
        if (colIndex < cols) ImGui::SameLine(0, spacing);
        else { colIndex = 0; ImGui::NewLine(); }
    }

    if (colIndex != 0) ImGui::NewLine();

    // 3. Polished Empty State when no items are found
    if (totalDrawn == 0) {
        float emptyY = 30.0f;
        ImGui::SetCursorPos(ImVec2(0, emptyY));

        const char* title = searchLower.empty() ? "This folder is empty" : "No matching assets found";
        const char* subtitle = searchLower.empty() ? "Right click to create assets or drag files here" : "Try searching for a different keyword";

        float tw = ImGui::CalcTextSize(title).x;
        float sw = ImGui::CalcTextSize(subtitle).x;

        ImGui::SetCursorPosX((width - tw) * 0.5f);
        ImGui::TextColored(T.textSecondary, "%s", title);

        ImGui::SetCursorPosX((width - sw) * 0.5f);
        ImGui::TextColored(T.textMuted, "%s", subtitle);
    }

    ImGui::Dummy(ImVec2(0, 8.0f));

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

void AssetBrowserPanel::drawSingleCard(const Engine::Assets::AssetGuid& guid, const char* label, const char* typeLbl, bool isActive, float cardW, float cardH) {
    auto& T = NexusTheme::instance();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    ImVec2 p = ImGui::GetCursorScreenPos();
    ImVec2 pMax = ImVec2(p.x + cardW, p.y + cardH);

    ImGui::InvisibleButton("##cardBtn", ImVec2(cardW, cardH));
    bool hov = ImGui::IsItemHovered();

    // Drag and Drop payload
    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
        Engine::Assets::AssetGuid dragGuid = guid;
        ImGui::SetDragDropPayload("ASSET_GUID", &dragGuid, sizeof(Engine::Assets::AssetGuid));
        ImGui::Text("%s", label);
        ImGui::EndDragDropSource();
    }

    // Card Background and Border
    ImU32 bgCol = COL(T.bgPanel);
    ImU32 borderCol = isActive ? COL(T.accent) : (hov ? COL(T.accent) : COL(T.border));
    float borderW = isActive ? 2.0f : 1.0f;

    dl->AddRectFilled(p, pMax, bgCol, 8.0f);
    dl->AddRect(p, pMax, borderCol, 8.0f, 0, borderW);

    if (isActive) {
        dl->AddRectFilled(ImVec2(p.x - 2, p.y - 2), ImVec2(pMax.x + 2, pMax.y + 2), COLA(0x82D9FF, 0.08f), 10.0f);
    }

    // Thumbnail Preview (32x32)
    float thumbS = 32.0f;
    float thumbX = p.x + (cardW - thumbS) * 0.5f;
    float thumbY = p.y + 8.0f;

    bgfx::TextureHandle thumb = Engine::Assets::ThumbnailCache::instance().get(guid);

    if (thumb.idx != bgfx::kInvalidHandle) {
        dl->AddImageRounded((ImTextureID)(uintptr_t)thumb.idx, ImVec2(thumbX, thumbY), ImVec2(thumbX + thumbS, thumbY + thumbS), ImVec2(0,0), ImVec2(1,1), IM_COL32_WHITE, 6.0f);
    } else {
        ImU32 typeColor = COLA(0x82D9FF, 0.2f);
        const char* iconKey = "icon_mesh_bold";
        std::string sLabel = label;
        std::string sType = typeLbl;

        if (sType.find("Script") != std::string::npos || sLabel.ends_with(".luau") || sLabel.ends_with(".lua")) {
            typeColor = COLA(0x66FF99, 0.2f); iconKey = "icon_script_bold";
        } else if (sType.find("Material") != std::string::npos || sLabel.ends_with(".mat")) {
            typeColor = COLA(0xFD71FF, 0.2f); iconKey = "icon_material_bold";
        } else if (sLabel.ends_with(".png") || sLabel.ends_with(".jpg")) {
            typeColor = COLA(0xFFDD6C, 0.2f); iconKey = "icon_fx_bold";
        }

        dl->AddRectFilled(ImVec2(thumbX, thumbY), ImVec2(thumbX + thumbS, thumbY + thumbS), typeColor, 6.0f);
        ImTextureID icn = IconRegistry::instance().get(iconKey);
        if (icn) {
            float is = thumbS * 0.60f;
            float ix = thumbX + (thumbS - is) * 0.5f;
            float iy = thumbY + (thumbS - is) * 0.5f;
            dl->AddImage(icn, ImVec2(ix, iy), ImVec2(ix + is, iy + is), ImVec2(0,0), ImVec2(1,1), COL(T.textPrimary));
        }
    }

    // Label Text
    std::string name = label;
    if (name.size() > 13) name = name.substr(0, 10) + "...";
    float nw = ImGui::CalcTextSize(name.c_str()).x;
    dl->AddText(ImVec2(p.x + (cardW - nw) * 0.5f, p.y + cardH - 32.0f), COL(T.textPrimary), name.c_str());

    // Type Text
    float tw = ImGui::CalcTextSize(typeLbl).x;
    ImU32 typeTextCol = isActive ? COL(T.accent) : COL(T.textMuted);
    dl->AddText(ImVec2(p.x + (cardW - tw) * 0.5f, p.y + cardH - 18.0f), typeTextCol, typeLbl);

    if (isActive) {
        const char* badge = "Active";
        float bw = ImGui::CalcTextSize(badge).x + 8;
        dl->AddRectFilled(ImVec2(p.x + 4, p.y + 4), ImVec2(p.x + 4 + bw, p.y + 16), COLA(0x82D9FF, 0.25f), 3.0f);
        dl->AddText(ImVec2(p.x + 8, p.y + 4), COL(T.accent), badge);
    }
}

} // namespace Editor::UI
