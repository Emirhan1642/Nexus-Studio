#include "ModelingOperatorPanel.h"
#include "Editor/Modeling/ModelingContext.h"
#include "NexusTheme.h"
#include <imgui.h>

namespace Editor::UI {

using Editor::Modeling::LastOpType;
using Editor::Modeling::ModelingContext;

void ModelingOperatorPanel::render(const ImVec2& viewportPos, const ImVec2& viewportSize) {
    auto& ctx = ModelingContext::instance();
    if (ctx.lastOp == LastOpType::None) return;

    const char* opTitle = "Operator Settings";
    if (ctx.lastOp == LastOpType::Extrude) opTitle = "Extrude Region";
    else if (ctx.lastOp == LastOpType::Inset) opTitle = "Inset Faces";
    else if (ctx.lastOp == LastOpType::Bevel) opTitle = "Bevel";
    else if (ctx.lastOp == LastOpType::LoopCut) opTitle = "Loop Cut and Slide";
    else if (ctx.lastOp == LastOpType::Subdivide) opTitle = "Subdivide";

    ImVec2 panelPos = ImVec2(viewportPos.x + 15.0f, viewportPos.y + viewportSize.y - 170.0f);
    ImGui::SetNextWindowPos(panelPos, ImGuiCond_Appearing);
    ImGui::SetNextWindowSize(ImVec2(240.0f, 0.0f), ImGuiCond_Always);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoSavedSettings |
                             ImGuiWindowFlags_AlwaysAutoResize |
                             ImGuiWindowFlags_NoDocking;

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.10f, 0.11f, 0.13f, 0.94f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.25f, 0.35f, 0.50f, 0.80f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 8.0f));

    bool isOpen = true;
    if (ImGui::Begin(opTitle, &isOpen, flags)) {
        bool changed = false;

        if (ctx.lastOp == LastOpType::Extrude) {
            changed |= ImGui::DragFloat("Distance", &ctx.opDistance, 0.05f, -50.0f, 50.0f, "%.2f m");
        } else if (ctx.lastOp == LastOpType::Inset) {
            changed |= ImGui::SliderFloat("Thickness", &ctx.opThickness, 0.0f, 0.95f, "%.2f");
            changed |= ImGui::DragFloat("Depth", &ctx.opDepth, 0.05f, -10.0f, 10.0f, "%.2f m");
        } else if (ctx.lastOp == LastOpType::Bevel) {
            changed |= ImGui::DragFloat("Width", &ctx.opWidth, 0.02f, 0.001f, 10.0f, "%.2f m");
            changed |= ImGui::SliderInt("Segments", &ctx.opSegments, 1, 8);
            changed |= ImGui::SliderFloat("Profile", &ctx.opProfile, 0.0f, 1.0f, "%.2f");
        } else if (ctx.lastOp == LastOpType::LoopCut) {
            changed |= ImGui::SliderFloat("Slide Factor", &ctx.opSlide, -0.95f, 0.95f, "%.2f");
            changed |= ImGui::SliderInt("Number of Cuts", &ctx.opCuts, 1, 8);
        } else if (ctx.lastOp == LastOpType::Subdivide) {
            changed |= ImGui::SliderInt("Number of Cuts", &ctx.opCuts, 1, 6);
            changed |= ImGui::SliderFloat("Smoothness", &ctx.opSmoothness, 0.0f, 1.0f, "%.2f");
        }

        if (changed) {
            ctx.reapplyLastOperation();
        }
    }
    ImGui::End();

    if (!isOpen) {
        ctx.lastOp = LastOpType::None;
    }

    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);
}

} // namespace Editor::UI
