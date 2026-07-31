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

    auto drawToolBtn = [&](Tool toolType, const char* iconName, const char* tooltip) {
        bool isActive = (currentTool == toolType);
        
        ImGui::SameLine(0, 4.0f);
        ImTextureID tex = IconRegistry::instance().get(iconName);
        if (tex) {
            if (ImGui::ImageButton(iconName, tex, iconSize, ImVec2(0,0), ImVec2(1,1), ImVec4(0,0,0,0), isActive ? NexusTheme::instance().accent : NexusTheme::instance().textPrimary)) {
                currentTool = toolType;
            }
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0,0,0,0));
            if (ImGui::Button(iconName, iconSize)) {
                currentTool = toolType;
            }
            ImGui::PopStyleColor();
        }
        
        if (ImGui::IsItemHovered()) {ImGui::SetTooltip("%s", tooltip);}
        ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPos().x - 2, ImGui::GetCursorPos().y + 2));
    };

    drawToolBtn(Tool::Select, "icon_cursor", "Select (Q)");
    drawToolBtn(Tool::Move, "icon_move", "Move (W)");
    drawToolBtn(Tool::Rotate, "icon_rotate", "Rotate (E)");
    drawToolBtn(Tool::Scale, "icon_scale", "Scale (R)");
    
    ImGui::Separator();
    
    ImGui::SameLine(0, 10.0f);
    
    ImTextureID snapTex = IconRegistry::instance().get("icon_snap");
    ImVec4 snapColor = gridSnap ? NexusTheme::instance().accent : NexusTheme::instance().textMuted;
    
    if (snapTex) {
        if (ImGui::ImageButton("snapBtn", snapTex, iconSize, ImVec2(0,0), ImVec2(1,1), ImVec4(0,0,0,0), snapColor)) {
            gridSnap = !gridSnap;
        }
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0,0,0,0));
        if (ImGui::Button("SNP", iconSize)) {
            gridSnap = !gridSnap;
        }
        ImGui::PopStyleColor();
    }
    
    if (ImGui::IsItemHovered()) {ImGui::SetTooltip("Grid Snap");}

    ImGui::Separator();
    
    if (ImGui::ImageButton("assetBtn", IconRegistry::instance().get("icon_folder"), iconSize, ImVec2(0,0), ImVec2(1,1), ImVec4(0,0,0,0), NexusTheme::instance().textPrimary)) {
        EditorLayout::instance().showAssetBrowser = !EditorLayout::instance().showAssetBrowser;
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Asset Manager");

    if (ImGui::ImageButton("matBtn", IconRegistry::instance().get("icon_material_outline"), iconSize, ImVec2(0,0), ImVec2(1,1), ImVec4(0,0,0,0), NexusTheme::instance().textPrimary)) {
        EditorLayout::instance().showMaterialEditor = !EditorLayout::instance().showMaterialEditor;
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Material Editor");

    ImGui::Separator();
    
    // Bottom padding simulation by pushing cursor or we can just draw settings
    // Since ImGui doesn't have flex-1 easily without a child window, we can calculate available space
    float availY = ImGui::GetContentRegionAvail().y;
    float settingsHeight = iconSize.y + ImGui::GetStyle().FramePadding.y * 2.0f;
    if (availY > settingsHeight + 10.0f) {
        ImGui::SetCursorPosY(ImGui::GetWindowHeight() - 40);
    }
    ImGui::SameLine(0, 4.0f);
    
    ImTextureID settingTex = IconRegistry::instance().get("icon_setting");
    if (settingTex) {
        if (ImGui::ImageButton("settingsBtn", settingTex, iconSize, ImVec2(0,0), ImVec2(1,1), ImVec4(0,0,0,0), NexusTheme::instance().textMuted)) {
            // Open settings
        }
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0,0,0,0));
        if (ImGui::Button("SET", iconSize)) {
            // Open settings
        }
        ImGui::PopStyleColor();
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Project Settings");

    ImGui::End();
}
