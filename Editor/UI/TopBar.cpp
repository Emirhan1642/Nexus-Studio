#include "TopBar.h"
#include <imgui.h>
#include "IconRegistry.h"
#include "NexusTheme.h"
#include "EditorLayout.h"
#include "Engine/Networking/Transport/NetworkContext.h"
#include "Engine/Networking/Transport/NetworkServer.h"
#include "Engine/Networking/Transport/NetworkClient.h"

void TopBar::draw(bool isSimulating, bool& toggleSim) {
    if (!ImGui::BeginMenuBar()) return;

    // Logo
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4.0f); // Center vertically
    if (IconRegistry::instance().get("logo_nexus")) {
        ImGui::Image(IconRegistry::instance().get("logo_nexus"), ImVec2(18, 18));
    }
    ImGui::SameLine();
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
    ImGui::TextColored(NexusTheme::instance().accent, "NEXUS");

    ImGui::SameLine(0, 15.0f);
    ImGui::Separator();
    ImGui::SameLine(0, 15.0f);

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
    if (ImGui::BeginMenu("Networking")) {
        auto currentMode = Engine::Networking::NetworkContext::mode();
        std::string modeStr = "Standalone";
        if (currentMode == Engine::Networking::NetworkMode::Server) modeStr = "Server";
        else if (currentMode == Engine::Networking::NetworkMode::Client) modeStr = "Client";
        ImGui::Text("Current Mode: %s", modeStr.c_str());
        ImGui::Separator();
        
        if (ImGui::MenuItem("Host Server", nullptr, false, currentMode == Engine::Networking::NetworkMode::Standalone)) {
            Engine::Networking::NetworkContext::setMode(Engine::Networking::NetworkMode::Server);
            Engine::Networking::NetworkServer::instance().start(7777);
        }
        if (ImGui::MenuItem("Connect to localhost", nullptr, false, currentMode == Engine::Networking::NetworkMode::Standalone)) {
            Engine::Networking::NetworkContext::setMode(Engine::Networking::NetworkMode::Client);
            Engine::Networking::NetworkClient::instance().connect("127.0.0.1", 7777);
        }
        if (ImGui::MenuItem("Disconnect / Stop", nullptr, false, currentMode != Engine::Networking::NetworkMode::Standalone)) {
            if (currentMode == Engine::Networking::NetworkMode::Server) {
                Engine::Networking::NetworkServer::instance().stop();
            } else {
                Engine::Networking::NetworkClient::instance().disconnect();
            }
            Engine::Networking::NetworkContext::setMode(Engine::Networking::NetworkMode::Standalone);
        }
        ImGui::EndMenu();
    }
    
    // Additional Mock Menus from HTML
    if (ImGui::BeginMenu("Scene")) { ImGui::EndMenu(); }
    if (ImGui::BeginMenu("Object")) { ImGui::EndMenu(); }
    if (ImGui::BeginMenu("Material")) { ImGui::EndMenu(); }
    if (ImGui::BeginMenu("Physics")) { ImGui::EndMenu(); }
    if (ImGui::BeginMenu("Plugins")) { ImGui::EndMenu(); }

    // AI Copilot Button (Highlighted)
    ImGui::SameLine(0, 10.0f);
    ImGui::PushStyleColor(ImGuiCol_Button, NexusTheme::instance().accentDim);
    ImGui::PushStyleColor(ImGuiCol_Text, NexusTheme::instance().accent);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
    
    ImTextureID aiIcon = IconRegistry::instance().get("icon_ai");
    if (aiIcon) {
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 2.0f);
        if (ImGui::ImageButton("##aiBtn", aiIcon, ImVec2(14, 14))) {
            EditorLayout::instance().showAICopilot = !EditorLayout::instance().showAICopilot;
        }
        ImGui::SameLine(0, 4.0f);
    }
    
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4.0f);
    if (ImGui::Selectable("AI Assistant [MCP]", false, 0, ImVec2(120, 0))) {
        EditorLayout::instance().showAICopilot = !EditorLayout::instance().showAICopilot;
    }
    
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(2);

    // Document Tabs (Mock)
    ImGui::SameLine(0, 20.0f);
    ImGui::Separator();
    ImGui::SameLine(0, 20.0f);

    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0,0,0,0));
    ImGui::PushStyleColor(ImGuiCol_Text, NexusTheme::instance().textPrimary);
    
    if (IconRegistry::instance().get("icon_world")) {
        ImGui::Image(IconRegistry::instance().get("icon_world"), ImVec2(14, 14));
        ImGui::SameLine();
    }
    ImGui::TextColored(NexusTheme::instance().accent, "3D Viewport");
    ImGui::SameLine(0, 20.0f);

    if (IconRegistry::instance().get("icon_material_outline")) {
        ImGui::Image(IconRegistry::instance().get("icon_material_outline"), ImVec2(14, 14));
        ImGui::SameLine();
    }
    ImGui::Text("PBR_Gold.mat");
    ImGui::SameLine(0, 20.0f);

    if (IconRegistry::instance().get("icon_script")) {
        ImGui::Image(IconRegistry::instance().get("icon_script"), ImVec2(14, 14));
        ImGui::SameLine();
    }
    ImGui::Text("MainCharacter.luau");

    ImGui::PopStyleColor(2);

    // Right align play controls
    float rightWidth = 180.0f;
    ImGui::SetCursorPosX(ImGui::GetWindowWidth() - rightWidth);

    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 2.0f);
    ImGui::TextDisabled("Target:");
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button, NexusTheme::instance().bg);
    ImGui::Button("PC / DX12 v", ImVec2(80, 20));
    ImGui::PopStyleColor();

    ImGui::SameLine();

    ImGui::PushStyleColor(ImGuiCol_Button, NexusTheme::instance().accent);
    ImGui::PushStyleColor(ImGuiCol_Text, NexusTheme::HexColor(0x000000));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
    
    if (ImGui::Button(isSimulating ? "Stop" : "Play", ImVec2(50, 20))) {
        toggleSim = true;
    }
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(2);

    ImGui::EndMenuBar();
}
