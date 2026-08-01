#include "ExplorerPanel.h"
#include <imgui.h>
#include "SelectionManager.h"
#include "../Undo/UndoStack.h"
#include "Engine/Core/DataModel/DataModel.h"
#include "Engine/Core/Reflection/TypeRegistry.h"
#include "Engine/Core/DataModel/Part.h"
#include "Engine/Scripting/Script.h"
#include "IconRegistry.h"
#include "NexusTheme.h"

void ExplorerPanel::draw() {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("Explorer");

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();
    float width = ImGui::GetWindowWidth();
    
    // Custom Header (Tabs)
    ImGui::PushStyleColor(ImGuiCol_ChildBg, NexusTheme::instance().panel);
    ImGui::BeginChild("##ExplorerHeader", ImVec2(width, 28), false, ImGuiWindowFlags_NoScrollbar);
    
    // Bottom border of header
    drawList->AddLine(ImVec2(p.x, p.y + 27), ImVec2(p.x + width, p.y + 27), ImGui::GetColorU32(NexusTheme::instance().border));
    
    // Tab 1
    ImGui::PushStyleColor(ImGuiCol_Button, NexusTheme::instance().bg);
    ImGui::PushStyleColor(ImGuiCol_Text, NexusTheme::instance().textPrimary);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
    ImGui::SetCursorPos(ImVec2(0, 0));
    ImGui::Button(" v Explorer ", ImVec2(80, 28));
    ImVec2 tabP = ImGui::GetItemRectMin();
    drawList->AddLine(ImVec2(tabP.x, tabP.y), ImVec2(tabP.x + 80, tabP.y), ImGui::GetColorU32(NexusTheme::instance().accent), 2.0f);
    ImGui::PopStyleColor(2);
    
    // Tab 2 & 3
    ImGui::SameLine(0, 0);
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0,0,0,0));
    ImGui::PushStyleColor(ImGuiCol_Text, NexusTheme::instance().textMuted);
    ImGui::Button(" Wld World ", ImVec2(70, 28));
    ImGui::SameLine(0, 0);
    ImGui::Button(" His History ", ImVec2(80, 28));
    ImGui::PopStyleColor(2);
    
    ImGui::PopStyleVar(); // FrameRounding
    
    // Right actions
    ImGui::SetCursorPos(ImVec2(width - 50, 4));
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0,0,0,0));
    ImGui::PushStyleColor(ImGuiCol_Text, NexusTheme::instance().textMuted);
    if (ImGui::Button("+", ImVec2(20, 20))) {
        ImGui::OpenPopup("ExplorerInsertPopup");
    }
    ImGui::SameLine(0, 0);
    ImGui::Button("...", ImVec2(20, 20));
    ImGui::PopStyleColor(2);
    
    if (ImGui::BeginPopup("ExplorerInsertPopup")) {
        drawInsertObjectMenu(DataModel::instance());
        ImGui::EndPopup();
    }
    ImGui::EndChild();
    ImGui::PopStyleColor(); // ChildBg

    // Tree Body
    ImGui::PushStyleColor(ImGuiCol_ChildBg, NexusTheme::instance().panel);
    ImGui::BeginChild("##ExplorerBody", ImVec2(width, ImGui::GetContentRegionAvail().y), false, 0);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 2));
    
    drawInstanceNode(DataModel::instance());
    
    ImGui::PopStyleVar();
    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::End();
    ImGui::PopStyleVar();
}

void ExplorerPanel::drawInstanceNode(const std::shared_ptr<Instance>& inst) {
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
    if (inst == DataModel::instance()) flags |= ImGuiTreeNodeFlags_DefaultOpen;
    
    bool isSelected = (inst == SelectionManager::instance().getSelected());
    if (isSelected) {
        flags |= ImGuiTreeNodeFlags_Selected;
    }
    
    if (inst->getChildren().empty()) {
        flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    }

    ImVec2 pos = ImGui::GetCursorScreenPos();
    
    ImGui::PushStyleColor(ImGuiCol_Header, NexusTheme::instance().panelHover);
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, NexusTheme::instance().panelHover);
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, NexusTheme::instance().panelHover);
    
    bool open = ImGui::TreeNodeEx((void*)inst.get(), flags, "");
    
    ImGui::PopStyleColor(3);

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 rowMin = ImGui::GetItemRectMin();
    ImVec2 rowMax = ImGui::GetItemRectMax();

    if (isSelected) {
        drawList->AddRectFilled(rowMin, rowMax, ImGui::GetColorU32(NexusTheme::HexColorAlpha(0x00d2ff, 0.15f)));
        drawList->AddRect(rowMin, rowMax, ImGui::GetColorU32(NexusTheme::HexColorAlpha(0x00d2ff, 0.4f)), 4.0f);
        drawList->AddLine(ImVec2(rowMin.x + 2, rowMin.y + 4), ImVec2(rowMin.x + 2, rowMax.y - 4), ImGui::GetColorU32(NexusTheme::instance().accent), 2.0f);
    }

    ImU32 typeColor = IM_COL32(100, 100, 100, 255); // default
    const char* iconName = "icon_folder";
    
    if (inst == DataModel::instance()) { typeColor = IM_COL32(150, 150, 150, 255); iconName = "icon_world"; }
    else if (inst->getClassName() == "Part") { typeColor = IM_COL32(0, 210, 255, 255); iconName = "icon_mesh"; }
    else if (inst->getClassName() == "Script") { typeColor = IM_COL32(45, 212, 191, 255); iconName = "icon_script"; }
    else if (inst->getClassName() == "Camera") { typeColor = IM_COL32(249, 115, 22, 255); iconName = "icon_camera"; }

    // Color bar tag
    if (inst->getClassName() != "DataModel" && inst != DataModel::instance()) {
        drawList->AddRectFilled(ImVec2(rowMin.x + ImGui::GetTreeNodeToLabelSpacing() - 14, rowMin.y + 4), 
                                ImVec2(rowMin.x + ImGui::GetTreeNodeToLabelSpacing() - 10, rowMax.y - 4), typeColor, 2.0f);
    }

    ImGui::SameLine();
    ImTextureID tex = IconRegistry::instance().get(iconName);
    if (tex) {
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (ImGui::GetItemRectSize().y - 14) * 0.5f);
        ImGui::Image(tex, ImVec2(14, 14));
        ImGui::SameLine(0, 4.0f);
    }
    
    ImGui::TextColored(isSelected ? NexusTheme::instance().textPrimary : NexusTheme::instance().textMuted, inst->name.c_str());

    if (isSelected && inst->getClassName() == "Part") {
        ImGui::SameLine(ImGui::GetWindowWidth() - 40);
        ImGui::PushStyleColor(ImGuiCol_Text, NexusTheme::instance().accent);
        ImGui::Text("PBR");
        ImGui::PopStyleColor();
    }

    if (ImGui::IsItemClicked()) {
        SelectionManager::instance().select(inst);
    }

    if (ImGui::BeginPopupContextItem(inst->name.c_str())) {
        drawInsertObjectMenu(inst);
        
        if (inst != DataModel::instance()) {
            ImGui::Separator();
            if (ImGui::MenuItem("Delete")) {
                inst->setParent(nullptr);
                SelectionManager::instance().clear();
            }
        }
        ImGui::EndPopup();
    }

    if (open && !inst->getChildren().empty()) {
        auto children = inst->getChildren();
        for (auto& child : children) {
            drawInstanceNode(child);
        }
        ImGui::TreePop();
    }
}

void ExplorerPanel::drawInsertObjectMenu(const std::shared_ptr<Instance>& parent) {
    if (ImGui::BeginMenu("Insert Object")) {
        // Hardcode for now, since TypeRegistry doesn't have getAllClasses yet in our MVP
        if (ImGui::MenuItem("Part")) {
            auto part = std::make_shared<Part>();
            UndoStack::instance().pushCreateCommand(part, parent);
        }
        if (ImGui::MenuItem("Script")) {
            auto script = std::make_shared<Script>();
            UndoStack::instance().pushCreateCommand(script, parent);
        }
        ImGui::EndMenu();
    }
}
