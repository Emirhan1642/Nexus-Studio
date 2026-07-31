#include "AICopilotPanel.h"
#include <imgui.h>
#include "EditorLayout.h"
#include "NexusTheme.h"
#include "IconRegistry.h"

void AICopilotPanel::draw() {
    if (!EditorLayout::instance().showAICopilot) return;

    ImGui::Begin("AI Copilot", &EditorLayout::instance().showAICopilot);

    // Header
    ImGui::Text("AI Copilot");
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0,1,0,1), "• Connected");
    ImGui::Separator();

    // Context strip
    ImGui::TextColored(NexusTheme::instance().textMuted, "Context: None Selected");
    ImGui::Separator();

    // Chat Area
    ImGui::BeginChild("ChatHistory", ImVec2(0, -35), true);
    ImGui::TextWrapped("Hello! I am your AI Copilot. I can help you write code, modify properties, and manage your scene.");
    ImGui::EndChild();

    // Input Bar
    static char inputBuf[256] = "";
    ImGui::PushItemWidth(-50);
    ImGui::InputText("##AIChatInput", inputBuf, IM_ARRAYSIZE(inputBuf));
    ImGui::PopItemWidth();
    
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button, NexusTheme::instance().accent);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0,0,0,1));
    if (ImGui::Button("Send", ImVec2(40, 0))) {
        // AI function placeholder
        inputBuf[0] = '\0';
    }
    ImGui::PopStyleColor(2);

    ImGui::End();
}
