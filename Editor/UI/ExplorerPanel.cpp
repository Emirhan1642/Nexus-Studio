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

    // Header buttons
    ImGui::SetCursorPos(ImVec2(ImGui::GetWindowWidth() - 50, 4));
    if (ImGui::Button("+", ImVec2(20, 20))) {
        // Open insert menu
        ImGui::OpenPopup("ExplorerInsertPopup");
    }
    ImGui::SameLine();
    if (ImGui::Button("...", ImVec2(20, 20))) {}
    
    if (ImGui::BeginPopup("ExplorerInsertPopup")) {
        drawInsertObjectMenu(DataModel::instance());
        ImGui::EndPopup();
    }

    // Tabs
    ImGui::SetCursorPosY(28);
    if (ImGui::BeginTabBar("ExplorerTabs")) {
        if (ImGui::BeginTabItem("Explorer")) {
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4);
            drawInstanceNode(DataModel::instance());
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("World")) {
            ImGui::TextDisabled("World Settings");
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("History")) {
            ImGui::TextDisabled("History Log");
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    ImGui::End();
    ImGui::PopStyleVar();
}

void ExplorerPanel::drawInstanceNode(const std::shared_ptr<Instance>& inst) {
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth;
    
    bool isSelected = (inst == SelectionManager::instance().getSelected());
    if (isSelected) {
        flags |= ImGuiTreeNodeFlags_Selected;
    }
    
    if (inst->getChildren().empty()) {
        flags |= ImGuiTreeNodeFlags_Leaf;
    }

    ImVec2 pos = ImGui::GetCursorScreenPos();
    bool open = ImGui::TreeNodeEx((void*)inst.get(), flags, "");

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 rowMin = ImGui::GetItemRectMin();
    ImVec2 rowMax = ImGui::GetItemRectMax();

    if (isSelected) {
        drawList->AddRectFilled(rowMin, rowMax, IM_COL32(0, 210, 255, 38));
        drawList->AddLine(rowMin, ImVec2(rowMin.x, rowMax.y), IM_COL32(0, 210, 255, 255), 2.0f);
    }

    ImU32 typeColor = IM_COL32(100, 100, 100, 255); // default
    const char* iconName = "icon_folder";
    
    if (inst == DataModel::instance()) { typeColor = IM_COL32(150, 150, 150, 255); iconName = "icon_world"; }
    else if (inst->getClassName() == "Part") { typeColor = IM_COL32(50, 150, 255, 255); iconName = "icon_mesh"; }
    else if (inst->getClassName() == "Script") { typeColor = IM_COL32(100, 200, 100, 255); iconName = "icon_script"; }
    else if (inst->getClassName() == "Camera") { typeColor = IM_COL32(255, 150, 50, 255); iconName = "icon_camera"; }

    drawList->AddRectFilled(rowMin, ImVec2(rowMin.x + 3, rowMin.y + 16), typeColor, 1.0f);

    ImGui::SameLine(0, 2.0f);
    ImTextureID tex = IconRegistry::instance().get(iconName);
    if (tex) {
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 2.0f);
        ImGui::Image(tex, ImVec2(14, 14));
        ImGui::SameLine(0, 4.0f);
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
    }
    
    ImGui::TextColored(isSelected ? NexusTheme::instance().textPrimary : NexusTheme::instance().textMuted, inst->name.c_str());

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

    if (open) {
        // Copy children vector because it might be modified during iteration
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
