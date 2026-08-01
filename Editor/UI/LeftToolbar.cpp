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

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 8));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, NexusTheme::instance().panel);
    ImGui::Begin("##LeftToolbar", nullptr, 
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar | 
        ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 iconSize(20, 20);
    float windowWidth = ImGui::GetWindowWidth();
    float btnSize = 32.0f;

    auto drawToolBtn = [&](Tool toolType, const char* label, const char* tooltip, bool isAccentHover = false) {
        bool isActive = (currentTool == toolType);
        
        ImGui::SetCursorPosX((windowWidth - btnSize) * 0.5f);
        
        if (isActive) {
            ImGui::PushStyleColor(ImGuiCol_Button, NexusTheme::instance().accentDim);
            ImGui::PushStyleColor(ImGuiCol_Border, NexusTheme::HexColorAlpha(0x00d2ff, 0.5f));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0,0,0,0));
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0,0,0,0));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
        }
        
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, NexusTheme::instance().panelHover);
        
        // No actual icons since we are mocking/reusing, let's just use text for now or IconRegistry if available
        ImTextureID tex = IconRegistry::instance().get(label);
        ImVec4 tint = isActive ? NexusTheme::instance().accent : NexusTheme::instance().textMuted;
        
        if (tex) {
            if (ImGui::ImageButton(label, tex, ImVec2(btnSize, btnSize), ImVec2(0,0), ImVec2(1,1), ImVec4(0,0,0,0), tint)) {
                currentTool = toolType;
            }
        } else {
            ImGui::PushStyleColor(ImGuiCol_Text, tint);
            if (ImGui::Button(label, ImVec2(btnSize, btnSize))) {
                currentTool = toolType;
            }
            ImGui::PopStyleColor();
        }
        
        if (isActive) {
            ImGui::PopStyleVar();
            ImGui::PopStyleColor(2);
        } else {
            ImGui::PopStyleVar();
            ImGui::PopStyleColor(2);
        }
        ImGui::PopStyleColor(); // hover
        
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tooltip);
        ImGui::Dummy(ImVec2(0, 2));
    };

    drawToolBtn(Tool::Select, "SEL", "Select Tool (Q)");
    drawToolBtn(Tool::Move, "MOV", "Move Tool (W)", true);
    drawToolBtn(Tool::Rotate, "ROT", "Rotate Tool (E)");
    drawToolBtn(Tool::Scale, "SCL", "Scale Tool (R)");
    
    // Grid Snap
    ImGui::SetCursorPosX((windowWidth - btnSize) * 0.5f);
    ImVec4 snapColor = gridSnap ? NexusTheme::instance().toggleOn : NexusTheme::instance().textMuted;
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0,0,0,0));
    ImGui::PushStyleColor(ImGuiCol_Text, snapColor);
    if (ImGui::Button("SNP", ImVec2(btnSize, btnSize))) {
        gridSnap = !gridSnap;
    }
    ImGui::PopStyleColor(2);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Grid Snap (Active) 1m / 15 deg");
    
    ImGui::Dummy(ImVec2(0, 6));
    ImVec2 p = ImGui::GetCursorScreenPos();
    drawList->AddLine(ImVec2(p.x + (windowWidth - 20)/2, p.y), ImVec2(p.x + (windowWidth + 20)/2, p.y), ImGui::GetColorU32(NexusTheme::instance().border));
    ImGui::Dummy(ImVec2(0, 6));

    // Asset Manager
    ImGui::SetCursorPosX((windowWidth - btnSize) * 0.5f);
    ImGui::PushStyleColor(ImGuiCol_Button, NexusTheme::instance().accentDim);
    ImGui::PushStyleColor(ImGuiCol_Border, NexusTheme::HexColorAlpha(0x00d2ff, 0.4f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
    ImGui::PushStyleColor(ImGuiCol_Text, NexusTheme::instance().accent);
    if (ImGui::Button("AST", ImVec2(btnSize, btnSize))) { EditorLayout::instance().showAssetBrowser = !EditorLayout::instance().showAssetBrowser; }
    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar();
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Asset Manager");
    ImGui::Dummy(ImVec2(0, 2));

    // Material Editor
    ImGui::SetCursorPosX((windowWidth - btnSize) * 0.5f);
    ImGui::PushStyleColor(ImGuiCol_Button, NexusTheme::instance().panelHover);
    ImGui::PushStyleColor(ImGuiCol_Border, NexusTheme::instance().border);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
    ImGui::PushStyleColor(ImGuiCol_Text, NexusTheme::instance().textPrimary);
    if (ImGui::Button("MAT", ImVec2(btnSize, btnSize))) { EditorLayout::instance().showMaterialEditor = !EditorLayout::instance().showMaterialEditor; }
    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar();
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Material Editor");
    ImGui::Dummy(ImVec2(0, 2));
    
    // Bottom Project Settings
    float availY = ImGui::GetContentRegionAvail().y;
    if (availY > btnSize + 10.0f) {
        ImGui::SetCursorPosY(ImGui::GetWindowHeight() - btnSize - 10.0f);
    }
    
    ImGui::SetCursorPosX((windowWidth - btnSize) * 0.5f);
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0,0,0,0));
    ImGui::PushStyleColor(ImGuiCol_Text, NexusTheme::instance().textMuted);
    ImGui::Button("SET", ImVec2(btnSize, btnSize));
    ImGui::PopStyleColor(2);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Project Settings");

    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
}
