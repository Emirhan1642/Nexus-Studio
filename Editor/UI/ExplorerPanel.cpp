#include "ExplorerPanel.h"
#include <dear-imgui/imgui.h>
#include "SelectionManager.h"
#include "../Undo/UndoStack.h"
#include "Engine/Core/DataModel/DataModel.h"
#include "Engine/Core/Reflection/TypeRegistry.h"
#include "Engine/Core/DataModel/Part.h"
#include "Engine/Scripting/Script.h"

void ExplorerPanel::draw() {
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImVec2 workPos = viewport->WorkPos;
    ImVec2 workSize = viewport->WorkSize;

    ImGui::SetNextWindowPos(ImVec2(workPos.x + workSize.x * 0.75f, workPos.y), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(workSize.x * 0.25f, workSize.y * 0.5f), ImGuiCond_Always);
    ImGui::Begin("Explorer", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoBringToFrontOnFocus);

    // Draw the root node (DataModel)
    drawInstanceNode(DataModel::instance());

    ImGui::End();
}

void ExplorerPanel::drawInstanceNode(const std::shared_ptr<Instance>& inst) {
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_DefaultOpen;
    
    if (inst == SelectionManager::instance().getSelected()) {
        flags |= ImGuiTreeNodeFlags_Selected;
    }
    
    if (inst->getChildren().empty()) {
        flags |= ImGuiTreeNodeFlags_Leaf;
    }

    bool open = ImGui::TreeNodeEx(inst->name.c_str(), flags);

    if (ImGui::IsItemClicked()) {
        SelectionManager::instance().select(inst);
    }

    if (ImGui::BeginPopupContextItem()) {
        drawInsertObjectMenu(inst);
        
        if (inst != DataModel::instance()) {
            ImGui::Separator();
            if (ImGui::MenuItem("Delete")) {
                // Delete logic (should be undoable, but for MVP just setParent(nullptr))
                auto cmd = std::make_unique<CreateInstanceCommand>(inst, nullptr); // setting parent to nullptr
                // Actually CreateInstanceCommand works for deletion too!
                // Wait, if it sets parent to nullptr, undo sets it to nullptr. That's wrong.
                // For MVP, just directly delete it from workspace:
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
