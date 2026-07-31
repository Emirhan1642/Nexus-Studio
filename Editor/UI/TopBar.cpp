#include "TopBar.h"
#include <imgui.h>
#include "IconRegistry.h"
#include "NexusTheme.h"
#include "EditorLayout.h"

void TopBar::draw(bool isSimulating, bool& toggleSim) {
    if (!ImGui::BeginMenuBar()) return;

    // Logo
    ImGui::Image(IconRegistry::instance().get("logo_nexus"), ImVec2(18, 18));
    ImGui::SameLine();
    ImGui::TextColored(NexusTheme::instance().accent, "NEXUS");

    ImGui::Separator();

    // Menus
    if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("New Project")) {}
        if (ImGui::MenuItem("Open Project")) {}
        ImGui::Separator();
        if (ImGui::MenuItem("Save")) {}
        if (ImGui::MenuItem("Exit")) {}
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Edit")) {
        if (ImGui::MenuItem("Undo", "CTRL+Z")) {}
        if (ImGui::MenuItem("Redo", "CTRL+Y")) {}
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("View")) {
        if (ImGui::MenuItem("Left Toolbar", nullptr, &EditorLayout::instance().showLeftToolbar)) {}
        if (ImGui::MenuItem("Material Editor", nullptr, &EditorLayout::instance().showMaterialEditor)) {}
        if (ImGui::MenuItem("Asset Browser", nullptr, &EditorLayout::instance().showAssetBrowser)) {}
        ImGui::Separator();
        if (ImGui::BeginMenu("Layout Presets")) {
            if (ImGui::MenuItem("Default")) { EditorLayout::instance().loadPreset("Default"); }
            if (ImGui::MenuItem("Minimal")) { EditorLayout::instance().loadPreset("Minimal"); }
            ImGui::EndMenu();
        }
        ImGui::EndMenu();
    }
    
    // AI Copilot Button
    ImGui::Separator();
    ImGui::PushStyleColor(ImGuiCol_Text, NexusTheme::instance().accent);
    if (ImGui::MenuItem("AI Copilot", nullptr, &EditorLayout::instance().showAICopilot)) {}
    ImGui::PopStyleColor();

    // Right align play controls
    float rightWidth = 220.0f;
    ImGui::SetCursorPosX(ImGui::GetWindowWidth() - rightWidth);

    ImGui::PushStyleColor(ImGuiCol_Button, NexusTheme::instance().panel);
    ImGui::Button("PC / DX12  v", ImVec2(90, 0));
    ImGui::PopStyleColor();

    ImGui::SameLine();

    ImGui::PushStyleColor(ImGuiCol_Button, NexusTheme::instance().accent);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0, 0, 0, 1));
    if (ImGui::Button(isSimulating ? "  Stop" : "  Play", ImVec2(70, 0))) {
        toggleSim = true;
    }
    ImGui::PopStyleColor(2);

    ImGui::EndMenuBar();
}
