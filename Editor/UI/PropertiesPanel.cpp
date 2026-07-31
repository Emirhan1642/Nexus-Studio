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

    auto selected = SelectionManager::instance().getSelected();
    if (!selected) {
        ImGui::Text("Nothing selected");
        ImGui::End();
        ImGui::PopStyleVar();
        return;
    }

    // Styled Header
    ImGui::PushStyleColor(ImGuiCol_ChildBg, NexusTheme::instance().panel);
    ImGui::BeginChild("PropHeader", ImVec2(0, 30), false);
    ImGui::SetCursorPos(ImVec2(8, 8));
    ImGui::TextColored(NexusTheme::instance().textPrimary, "PROPERTIES - %s", selected->name.c_str());
    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::SetCursorPos(ImVec2(8, 38));
    ImGui::BeginChild("PropScroll");

    auto* classDesc = Engine::Reflection::TypeRegistry::instance().find(selected->getClassName());
    if (!classDesc) {
        ImGui::Text("Unknown class: %s", selected->getClassName().c_str());
        ImGui::EndChild();
        ImGui::End();
        ImGui::PopStyleVar();
        return;
    }

    if (ImGui::CollapsingHeader("General", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::BeginTable("PropTable", 2, ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_Resizable)) {
            ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, 100.0f);
            ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

            for (auto& prop : classDesc->properties) {
                drawPropertyEditor(selected, &prop);
            }
            ImGui::EndTable();
        }
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
