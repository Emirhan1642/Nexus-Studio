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

    const char* opTitle = "Adjust Last Operation";
    if (ctx.lastOp == LastOpType::Extrude) opTitle = "Extrude Parameters";
    else if (ctx.lastOp == LastOpType::Inset) opTitle = "Inset Parameters";
    else if (ctx.lastOp == LastOpType::Bevel) opTitle = "Bevel / Chamfer Parameters";
    else if (ctx.lastOp == LastOpType::LoopCut) opTitle = "Loop Cut Parameters";
    else if (ctx.lastOp == LastOpType::Subdivide) opTitle = "Subdivide Parameters";
    else if (ctx.lastOp == LastOpType::ShrinkFatten) opTitle = "Shrink / Fatten Parameters";
    else if (ctx.lastOp == LastOpType::EdgeSlide) opTitle = "Edge Slide Parameters";

    ImVec2 panelPos = ImVec2(viewportPos.x + 20.0f, viewportPos.y + viewportSize.y - 195.0f);
    ImGui::SetNextWindowPos(panelPos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(320.0f, 0.0f), ImGuiCond_Always);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoSavedSettings |
                             ImGuiWindowFlags_AlwaysAutoResize |
                             ImGuiWindowFlags_NoDocking |
                             ImGuiWindowFlags_NoCollapse;

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.08f, 0.09f, 0.12f, 0.96f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.00f, 0.70f, 1.00f, 0.75f));
    ImGui::PushStyleColor(ImGuiCol_TitleBg, ImVec4(0.12f, 0.15f, 0.20f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_TitleBgActive, ImVec4(0.14f, 0.18f, 0.25f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.14f, 0.16f, 0.20f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.18f, 0.22f, 0.28f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_SliderGrab, ImVec4(0.00f, 0.78f, 1.00f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, ImVec4(0.40f, 0.88f, 1.00f, 1.0f));

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0f, 10.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 6.0f));

    bool isOpen = true;
    if (ImGui::Begin(opTitle, &isOpen, flags)) {
        bool changed = false;

        auto DrawRowHeader = [](const char* label) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::AlignTextToFramePadding();
            ImGui::TextColored(ImVec4(0.85f, 0.90f, 0.95f, 1.0f), "%s", label);
            ImGui::TableNextColumn();
            ImGui::SetNextItemWidth(-1.0f);
        };

        if (ImGui::BeginTable("##OpParamsTable", 2, ImGuiTableFlags_None)) {
            ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 105.0f);
            ImGui::TableSetupColumn("Control", ImGuiTableColumnFlags_WidthStretch);

            if (ctx.lastOp == LastOpType::Extrude) {
                DrawRowHeader("Distance");
                changed |= ImGui::DragFloat("##Distance", &ctx.opDistance, 0.02f, -50.0f, 50.0f, "%.2f m");
                DrawRowHeader("Individual Faces");
                changed |= ImGui::Checkbox("##IndividualExtrude", &ctx.opIndividual);
            } else if (ctx.lastOp == LastOpType::Inset) {
                DrawRowHeader("Thickness");
                changed |= ImGui::SliderFloat("##Thickness", &ctx.opThickness, 0.0f, 0.95f, "%.2f");
                DrawRowHeader("Depth");
                changed |= ImGui::DragFloat("##Depth", &ctx.opDepth, 0.02f, -10.0f, 10.0f, "%.2f m");
                DrawRowHeader("Individual Faces");
                changed |= ImGui::Checkbox("##IndividualInset", &ctx.opIndividual);
            } else if (ctx.lastOp == LastOpType::Bevel) {
                DrawRowHeader("Width");
                changed |= ImGui::DragFloat("##Width", &ctx.opWidth, 0.01f, 0.001f, 10.0f, "%.3f m");
                DrawRowHeader("Segments");
                changed |= ImGui::SliderInt("##Segments", &ctx.opSegments, 1, 8);
                DrawRowHeader("Profile");
                changed |= ImGui::SliderFloat("##Profile", &ctx.opProfile, 0.0f, 1.0f, "%.2f");
            } else if (ctx.lastOp == LastOpType::LoopCut) {
                DrawRowHeader("Slide Offset");
                changed |= ImGui::SliderFloat("##Slide", &ctx.opSlide, -0.90f, 0.90f, "%.2f");
                DrawRowHeader("Cuts Count");
                changed |= ImGui::SliderInt("##Cuts", &ctx.opCuts, 1, 6);
            } else if (ctx.lastOp == LastOpType::Subdivide) {
                DrawRowHeader("Number of Cuts");
                changed |= ImGui::SliderInt("##SubdivCuts", &ctx.opCuts, 1, 4);
                DrawRowHeader("Smoothness");
                changed |= ImGui::SliderFloat("##Smoothness", &ctx.opSmoothness, 0.0f, 1.0f, "%.2f");
            } else if (ctx.lastOp == LastOpType::ShrinkFatten) {
                DrawRowHeader("Offset");
                changed |= ImGui::DragFloat("##Offset", &ctx.opDistance, 0.01f, -10.0f, 10.0f, "%.3f m");
            } else if (ctx.lastOp == LastOpType::EdgeSlide) {
                DrawRowHeader("Slide Factor");
                changed |= ImGui::SliderFloat("##SlideFactor", &ctx.opSlide, -1.0f, 1.0f, "%.2f");
            }

            ImGui::EndTable();
        }

        ImGui::Spacing();
        if (ImGui::Button("Apply & Close", ImVec2(-1.0f, 24.0f))) {
            ctx.lastOp = LastOpType::None;
            ctx.baseSnapshotMesh = nullptr;
            ctx.opTargetVertices.clear();
            ctx.opTargetEdges.clear();
            ctx.opTargetFaces.clear();
        }

        if (changed) {
            ctx.reapplyLastOperation();
        }
    }
    ImGui::End();

    if (!isOpen) {
        ctx.lastOp = LastOpType::None;
        ctx.baseSnapshotMesh = nullptr;
        ctx.opTargetVertices.clear();
        ctx.opTargetEdges.clear();
        ctx.opTargetFaces.clear();
    }

    ImGui::PopStyleVar(4);
    ImGui::PopStyleColor(8);
}

} // namespace Editor::UI
