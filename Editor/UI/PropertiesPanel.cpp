#include "PropertiesPanel.h"
#include <imgui.h>
#include "SelectionManager.h"
#include "../Undo/UndoStack.h"
#include "Engine/Core/Reflection/TypeRegistry.h"
#include "Engine/Core/Reflection/EnumRegistry.h"
#include "Engine/Core/DataModel/Instance.h"
#include "Engine/Core/Math/Vector3.h"
#include "Engine/Assets/AssetDatabase.h"
#include "Engine/Assets/AssetDependencyTracker.h"
#include <string>
#include <map>
#include <vector>
#include "NexusTheme.h"

void PropertiesPanel::draw() {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("Properties");

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();
    float width = ImGui::GetWindowWidth();

    // Custom Header (Tabs)
    ImGui::PushStyleColor(ImGuiCol_ChildBg, NexusTheme::instance().panel);
    ImGui::BeginChild("##PropHeader", ImVec2(width, 28), false, ImGuiWindowFlags_NoScrollbar);
    drawList->AddLine(ImVec2(p.x, p.y + 27), ImVec2(p.x + width, p.y + 27), ImGui::GetColorU32(NexusTheme::instance().border));
    
    ImGui::PushStyleColor(ImGuiCol_Button, NexusTheme::instance().bg);
    ImGui::PushStyleColor(ImGuiCol_Text, NexusTheme::instance().textPrimary);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
    ImGui::SetCursorPos(ImVec2(0, 0));
    ImGui::Button(" v Properties ", ImVec2(90, 28));
    ImVec2 tabP = ImGui::GetItemRectMin();
    drawList->AddLine(ImVec2(tabP.x, tabP.y), ImVec2(tabP.x + 90, tabP.y), ImGui::GetColorU32(NexusTheme::instance().accent), 2.0f);
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(2);
    ImGui::EndChild();
    ImGui::PopStyleColor();

    auto selected = SelectionManager::instance().getSelected();
    if (!selected) {
        ImGui::SetCursorPos(ImVec2(8, 38));
        ImGui::TextDisabled("Nothing selected");
        ImGui::End();
        ImGui::PopStyleVar();
        return;
    }

    // Title area
    ImGui::PushStyleColor(ImGuiCol_ChildBg, NexusTheme::instance().bg);
    ImGui::BeginChild("##PropTitleArea", ImVec2(width, 36), false, ImGuiWindowFlags_NoScrollbar);
    p = ImGui::GetCursorScreenPos();
    drawList->AddLine(ImVec2(p.x, p.y + 35), ImVec2(p.x + width, p.y + 35), ImGui::GetColorU32(NexusTheme::instance().border));
    ImGui::SetCursorPos(ImVec2(8, 10));
    ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]);
    ImGui::TextColored(NexusTheme::instance().textPrimary, "%s", selected->name.c_str());
    ImGui::PopFont();
    ImGui::SameLine(width - 40);
    ImGui::SetCursorPosY(10);
    ImGui::TextDisabled(selected->getClassName().c_str());
    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::BeginChild("PropScroll");

    auto* classDesc = Engine::Reflection::TypeRegistry::instance().find(selected->getClassName());
    if (!classDesc) {
        ImGui::SetCursorPos(ImVec2(8, 8));
        ImGui::TextDisabled("Unknown class: %s", selected->getClassName().c_str());
        ImGui::EndChild();
        ImGui::End();
        ImGui::PopStyleVar();
        return;
    }

    ImGui::PushStyleColor(ImGuiCol_Header, NexusTheme::instance().panel);
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, NexusTheme::instance().panelHover);
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, NexusTheme::instance().panelHover);
    if (ImGui::CollapsingHeader("General", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::PopStyleColor(3);
        
        ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(4, 4));
        if (ImGui::BeginTable("PropTable", 2, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_Resizable)) {
            ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, 120.0f);
            ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

            for (auto& prop : classDesc->properties) {
                drawPropertyEditor(selected, &prop);
            }
            ImGui::EndTable();
        }
        ImGui::PopStyleVar();
    } else {
        ImGui::PopStyleColor(3);
    }
    
    ImGui::EndChild();

    ImGui::End();
    ImGui::PopStyleVar();
}

void PropertiesPanel::drawPropertyEditor(const std::shared_ptr<Instance>& inst, const Engine::Reflection::PropertyDescriptor* prop) {
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    
    ImGui::AlignTextToFramePadding();
    ImGui::TextColored(NexusTheme::instance().textMuted, "%s", prop->name.c_str());
    
    ImGui::TableNextColumn();
    
    ImGui::PushID(prop->name.c_str());
    ImGui::SetNextItemWidth(-FLT_MIN);
    
    ImGui::BeginDisabled(prop->readOnly);

    std::any current = prop->getter(inst.get());

    if (current.type() == typeid(std::string)) {
        std::string v = std::any_cast<std::string>(current);
        char buffer[256];
        strncpy(buffer, v.c_str(), sizeof(buffer));
        if (ImGui::InputText("##v", buffer, sizeof(buffer))) {
            UndoStack::instance().pushPropertyChangeCommand(inst, prop->name, current, std::string(buffer));
            prop->setter(inst.get(), std::string(buffer));
        }

        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_GUID")) {
                Engine::Assets::AssetGuid dragGuid = *(const Engine::Assets::AssetGuid*)payload->Data;
                const auto* meta = Engine::Assets::AssetDatabase::instance().find(dragGuid);
                if (meta) {
                    UndoStack::instance().pushPropertyChangeCommand(inst, prop->name, current, meta->relativePath);
                    prop->setter(inst.get(), meta->relativePath);
                    
                    // Register usage
                    Engine::Assets::AssetDependencyTracker::instance().registerUsage(dragGuid, inst->getInstanceId());
                }
            }
            ImGui::EndDragDropTarget();
        }
    }
    else if (current.type() == typeid(float)) {
        float v = std::any_cast<float>(current);
        if (ImGui::DragFloat("##v", &v, 0.1f)) {
            UndoStack::instance().pushPropertyChangeCommand(inst, prop->name, current, v);
            prop->setter(inst.get(), v);
        }
    }
    else if (current.type() == typeid(Engine::Math::Vector3)) {
        Engine::Math::Vector3 v = std::any_cast<Engine::Math::Vector3>(current);
        float arr[3] = {v.x, v.y, v.z};
        if (ImGui::DragFloat3("##v", arr, 0.1f)) {
            Engine::Math::Vector3 newVal{arr[0], arr[1], arr[2]};
            UndoStack::instance().pushPropertyChangeCommand(inst, prop->name, current, newVal);
            prop->setter(inst.get(), newVal);
        }
    }
    else if (current.type() == typeid(bool)) {
        bool v = std::any_cast<bool>(current);
        if (ImGui::Checkbox("##v", &v)) {
            UndoStack::instance().pushPropertyChangeCommand(inst, prop->name, current, v);
            prop->setter(inst.get(), v);
        }
    }
    else if (prop->kind == Engine::Reflection::PropertyDescriptor::Kind::Enum) {
        if (current.type() == typeid(int)) {
            int v = std::any_cast<int>(current);
            // Enum adını EnumRegistry'den bulmak için:
            // Biz enumProperty kaydederken extra info olarak enum adını vermiştik!
            // Ama PropertyDescriptor içinde enum adı nerede?
            // "category" içine saklamamıştık, enum için string saklamamıştık.
            // Şimdilik sadece sayı olarak gösterelim.
            if (ImGui::DragInt("##v", &v, 1)) {
                prop->setter(inst.get(), v);
            }
        }
    }

    ImGui::EndDisabled();
    ImGui::PopID();
}
