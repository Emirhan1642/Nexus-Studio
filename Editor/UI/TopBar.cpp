#include "TopBar.h"
#include <imgui.h>
#include "IconRegistry.h"
#include "NexusTheme.h"
#include "EditorLayout.h"
#include "Engine/Networking/Transport/NetworkContext.h"
#include "Engine/Networking/Transport/NetworkServer.h"
#include "Engine/Networking/Transport/NetworkClient.h"

void TopBar::draw(bool isSimulating, bool& toggleSim) {
    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->WorkPos);
    ImGui::SetNextWindowSize(ImVec2(vp->WorkSize.x, 40.0f));
    
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 0));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, NexusTheme::instance().panel);
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0,0,0,0)); // Transparent buttons by default for menus
    
    if (!ImGui::Begin("##TopBarReal", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoSavedSettings)) {
        ImGui::End();
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(3);
        return;
    }

    float midY = 40.0f * 0.5f;
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();
    
    // Draw bottom border
    drawList->AddLine(ImVec2(p.x - 12, p.y + 40 - 1), ImVec2(p.x + vp->WorkSize.x, p.y + 40 - 1), ImGui::GetColorU32(NexusTheme::instance().border));

    // Logo & Title
    ImGui::SetCursorPosY(midY - 10.0f);
    ImTextureID logo = IconRegistry::instance().get("logo_nexus");
    if (logo) {
        ImGui::Image(logo, ImVec2(20, 20));
        ImGui::SameLine();
    } else {
        // Mock the "V" icon from HTML
        ImGui::PushStyleColor(ImGuiCol_ChildBg, NexusTheme::instance().accent);
        ImGui::BeginChild("##LogoMock", ImVec2(20, 20), false, ImGuiWindowFlags_NoScrollbar);
        ImGui::SetCursorPos(ImVec2(6, 3));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0,0,0,1));
        ImGui::TextUnformatted("V");
        ImGui::PopStyleColor();
        ImGui::EndChild();
        ImGui::SameLine();
    }

    ImGui::SetCursorPosY(midY - 7.0f);
    // ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]); // assuming bold if available, else regular
    ImGui::TextColored(NexusTheme::instance().textPrimary, "VIBE");
    // ImGui::PopFont();

    // Right border after logo
    ImGui::SameLine(0, 12);
    p = ImGui::GetCursorScreenPos();
    drawList->AddLine(ImVec2(p.x - 6, p.y - 12), ImVec2(p.x - 6, p.y + 28), ImGui::GetColorU32(NexusTheme::instance().border));
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 4.0f);

    auto drawMenu = [](const char* name) {
        ImGui::SetCursorPosY(10.0f);
        ImGui::PushStyleColor(ImGuiCol_Text, NexusTheme::instance().textMuted);
        bool open = ImGui::BeginMenu(name);
        ImGui::PopStyleColor();
        return open;
    };

    auto menuBtn = [midY](const char* label) {
        ImGui::SetCursorPosY(midY - 10.0f);
        ImGui::PushStyleColor(ImGuiCol_Text, NexusTheme::instance().textMuted);
        bool pressed = ImGui::Button(label);
        ImGui::PopStyleColor();
        return pressed;
    };

    if (menuBtn("File")) ImGui::OpenPopup("FileMenu");
    ImGui::SameLine(); if (menuBtn("Edit")) ImGui::OpenPopup("EditMenu");
    ImGui::SameLine(); if (menuBtn("View")) ImGui::OpenPopup("ViewMenu");
    ImGui::SameLine(); if (menuBtn("Scene")) ImGui::OpenPopup("SceneMenu");
    ImGui::SameLine(); if (menuBtn("Object")) ImGui::OpenPopup("ObjectMenu");
    ImGui::SameLine(); if (menuBtn("Material")) ImGui::OpenPopup("MaterialMenu");
    ImGui::SameLine(); if (menuBtn("Physics")) ImGui::OpenPopup("PhysicsMenu");
    ImGui::SameLine(); if (menuBtn("Plugins")) ImGui::OpenPopup("PluginsMenu");
    ImGui::SameLine(); if (menuBtn("Networking")) ImGui::OpenPopup("NetworkingMenu");

    // Popups
    ImGui::PushStyleColor(ImGuiCol_PopupBg, NexusTheme::instance().panel);
    if (ImGui::BeginPopup("FileMenu")) {
        if (ImGui::Selectable("New Project")) {}
        if (ImGui::Selectable("Open Project")) {}
        ImGui::Separator();
        if (ImGui::Selectable("Save")) {}
        if (ImGui::Selectable("Exit")) {}
        ImGui::EndPopup();
    }
    if (ImGui::BeginPopup("EditMenu")) {
        if (ImGui::Selectable("Undo")) {}
        if (ImGui::Selectable("Redo")) {}
        ImGui::EndPopup();
    }
    if (ImGui::BeginPopup("ViewMenu")) {
        if (ImGui::Selectable("Left Toolbar", EditorLayout::instance().showLeftToolbar)) { EditorLayout::instance().showLeftToolbar = !EditorLayout::instance().showLeftToolbar; }
        if (ImGui::Selectable("Material Editor", EditorLayout::instance().showMaterialEditor)) { EditorLayout::instance().showMaterialEditor = !EditorLayout::instance().showMaterialEditor; }
        if (ImGui::Selectable("Asset Browser", EditorLayout::instance().showAssetBrowser)) { EditorLayout::instance().showAssetBrowser = !EditorLayout::instance().showAssetBrowser; }
        ImGui::Separator();
        if (ImGui::Selectable("Default Layout")) { EditorLayout::instance().loadPreset("Default"); }
        ImGui::Selectable("Minimal Layout");
        ImGui::EndPopup();
    }
    if (ImGui::BeginPopup("NetworkingMenu")) {
        auto currentMode = Engine::Networking::NetworkContext::mode();
        if (ImGui::Selectable("Host Server", currentMode == Engine::Networking::NetworkMode::Server)) {
            Engine::Networking::NetworkContext::setMode(Engine::Networking::NetworkMode::Server);
            Engine::Networking::NetworkServer::instance().start(7777);
        }
        if (ImGui::Selectable("Connect to localhost", currentMode == Engine::Networking::NetworkMode::Client)) {
            Engine::Networking::NetworkContext::setMode(Engine::Networking::NetworkMode::Client);
            Engine::Networking::NetworkClient::instance().connect("127.0.0.1", 7777);
        }
        if (ImGui::Selectable("Disconnect / Stop")) {
            if (currentMode == Engine::Networking::NetworkMode::Server) {
                Engine::Networking::NetworkServer::instance().stop();
            } else {
                Engine::Networking::NetworkClient::instance().disconnect();
            }
            Engine::Networking::NetworkContext::setMode(Engine::Networking::NetworkMode::Standalone);
        }
        ImGui::EndPopup();
    }
    ImGui::PopStyleColor();

    // AI Assistant Button
    ImGui::SameLine(0, 10.0f);
    ImGui::SetCursorPosY(midY - 10.0f);
    ImGui::PushStyleColor(ImGuiCol_Button, NexusTheme::instance().accent);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0,0,0,1));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
    if (ImGui::Button("✨ AI Assistant [MCP]", ImVec2(0, 20))) {
        EditorLayout::instance().showAICopilot = !EditorLayout::instance().showAICopilot;
    }
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(2);

    // Document Tabs (Center)
    ImGui::SameLine(0, 20.0f);
    p = ImGui::GetCursorScreenPos();
    drawList->AddLine(ImVec2(p.x - 10, p.y - 12), ImVec2(p.x - 10, p.y + 28), ImGui::GetColorU32(NexusTheme::instance().border));
    
    // Tab 1: 3D Viewport
    ImGui::PushStyleColor(ImGuiCol_Button, NexusTheme::instance().bg);
    ImGui::PushStyleColor(ImGuiCol_Text, NexusTheme::instance().textPrimary);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0,0));
    ImGui::SetCursorPosY(0);
    ImGui::Button(" 3D Viewport ", ImVec2(100, 40));
    // Top border highlight
    p = ImGui::GetItemRectMin();
    drawList->AddLine(ImVec2(p.x, p.y), ImVec2(p.x + 100, p.y), ImGui::GetColorU32(NexusTheme::instance().accent), 2.0f);
    // Bullet
    drawList->AddCircleFilled(ImVec2(p.x + 10, p.y + 20), 3.0f, ImGui::GetColorU32(NexusTheme::instance().accent));
    ImGui::PopStyleColor(2);
    
    // Tab 2: Material
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button, NexusTheme::instance().panelHover);
    ImGui::PushStyleColor(ImGuiCol_Text, NexusTheme::instance().accent);
    ImGui::Button(" PBR_Gold.mat [Node] ", ImVec2(150, 40));
    p = ImGui::GetItemRectMin();
    drawList->AddLine(ImVec2(p.x, p.y), ImVec2(p.x + 150, p.y), ImGui::GetColorU32(NexusTheme::HexColorAlpha(0x00d2ff, 0.4f)), 2.0f);
    ImGui::PopStyleColor(2);

    // Tab 3: Script
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button, NexusTheme::instance().panel);
    ImGui::PushStyleColor(ImGuiCol_Text, NexusTheme::instance().textMuted);
    ImGui::Button(" MainCharacter.luau ", ImVec2(140, 40));
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(2);
    
    p = ImGui::GetCursorScreenPos();
    drawList->AddLine(ImVec2(p.x, p.y - 12), ImVec2(p.x, p.y + 28), ImGui::GetColorU32(NexusTheme::instance().border));


    // Right Side: Build Target & Play Control
    float rightWidth = 180.0f;
    ImGui::SetCursorPosX(ImGui::GetWindowWidth() - rightWidth);
    ImGui::SetCursorPosY(midY - 10.0f);
    
    ImGui::PushStyleColor(ImGuiCol_Text, NexusTheme::instance().textMuted);
    ImGui::Text("Target:");
    ImGui::PopStyleColor();
    ImGui::SameLine(0, 6.0f);
    
    ImGui::PushStyleColor(ImGuiCol_Button, NexusTheme::instance().bg);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
    ImGui::Button("PC / DX12 v", ImVec2(80, 20));
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();

    ImGui::SameLine(0, 10.0f);
    ImGui::PushStyleColor(ImGuiCol_Button, NexusTheme::instance().accent);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0,0,0,1));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
    if (ImGui::Button(isSimulating ? "Stop" : "Play", ImVec2(50, 20))) {
        toggleSim = true;
    }
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(2);

    ImGui::End();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(3);
}
