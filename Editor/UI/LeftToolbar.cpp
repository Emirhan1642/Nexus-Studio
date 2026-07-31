#include "LeftToolbar.h"
#include <imgui.h>
#include "IconRegistry.h"
#include "NexusTheme.h"
#include "EditorLayout.h"

enum class Tool { Select, Move, Rotate, Scale };
static Tool currentTool = Tool::Select;
static bool gridSnap = false;

void LeftToolbar::draw() {
    if (!EditorLayout::instance().showLeftToolbar) return;

    ImGui::Begin("##LeftToolbar", nullptr, 
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar | 
        ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 iconSize(24, 24);

    auto drawToolBtn = [&](Tool tool, const char* iconName, const char* tooltip) {
        bool isActive = (currentTool == tool);
        
        ImVec2 p = ImGui::GetCursorScreenPos();
        ImVec2 q = ImVec2(p.x + 28, p.y + 28);
        
        if (isActive) {
            drawList->AddRectFilled(p, q, IM_COL32(0, 210, 255, 38), 3.0f);
            drawList->AddRect(p, q, IM_COL32(0, 210, 255, 128), 3.0f, 0, 1.0f);
        }

        ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPos().x + 2, ImGui::GetCursorPos().y + 2));
        if (ImGui::ImageButton(iconName, IconRegistry::instance().get(iconName), iconSize, ImVec2(0,0), ImVec2(1,1), ImVec4(0,0,0,0), isActive ? NexusTheme::instance().accent : NexusTheme::instance().textPrimary)) {
            currentTool = tool;
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tooltip);
        ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPos().x - 2, ImGui::GetCursorPos().y + 2));
    };

    drawToolBtn(Tool::Select, "icon_cursor", "Select (Q)");
    drawToolBtn(Tool::Move, "icon_move", "Move (W)");
    drawToolBtn(Tool::Rotate, "icon_rotate", "Rotate (E)");
    drawToolBtn(Tool::Scale, "icon_scale", "Scale (R)");
    
    ImGui::Separator();
    
    // Grid Snap
    ImVec4 snapColor = gridSnap ? NexusTheme::instance().toggleOn : NexusTheme::instance().textPrimary;
    if (ImGui::ImageButton("snapBtn", IconRegistry::instance().get("icon_snap"), iconSize, ImVec2(0,0), ImVec2(1,1), ImVec4(0,0,0,0), snapColor)) {
        gridSnap = !gridSnap;
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Grid Snap");

    ImGui::Separator();
    
    if (ImGui::ImageButton("assetBtn", IconRegistry::instance().get("icon_folder"), iconSize, ImVec2(0,0), ImVec2(1,1), ImVec4(0,0,0,0), NexusTheme::instance().textPrimary)) {
        EditorLayout::instance().showAssetBrowser = !EditorLayout::instance().showAssetBrowser;
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Asset Manager");

    if (ImGui::ImageButton("matBtn", IconRegistry::instance().get("icon_material"), iconSize, ImVec2(0,0), ImVec2(1,1), ImVec4(0,0,0,0), NexusTheme::instance().textPrimary)) {
        EditorLayout::instance().showMaterialEditor = !EditorLayout::instance().showMaterialEditor;
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Material Editor");

    ImGui::End();
}
