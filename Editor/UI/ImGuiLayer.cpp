#include "ImGuiLayer.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <backends/imgui_impl_glfw.h>
#include "Backend/imgui_impl_bgfx.h"
#include <filesystem>

#define VIEW_ID_IMGUI 255

void ImGuiLayer::init(GLFWwindow* window) {
    m_window = window;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    // Font loading (Inter or fallback)
    const char* fontPath = "Assets/Fonts/Inter-Regular.ttf";
    if (std::filesystem::exists(fontPath)) {
        ImFontConfig fontCfg;
        fontCfg.OversampleH = 3;
        fontCfg.OversampleV = 3;
        fontCfg.PixelSnapH = true;
        io.Fonts->AddFontFromFileTTF(fontPath, 14.0f, &fontCfg);
    }

    ImGui_ImplGlfw_InitForOther(window, true);
    ImGui_ImplBgfx_Init(VIEW_ID_IMGUI);
}

void ImGuiLayer::shutdown() {
    ImGui_ImplBgfx_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

void ImGuiLayer::beginFrame() {
    ImGui_ImplBgfx_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImVec2 workPos = vp->WorkPos;
    ImVec2 workSize = vp->WorkSize;
    float topBarHeight = 40.0f;
    
    // Offset dockspace for our custom 40px TopBar
    workPos.y += topBarHeight;
    workSize.y -= topBarHeight;

    ImGui::SetNextWindowPos(workPos);
    ImGui::SetNextWindowSize(workSize);
    ImGui::SetNextWindowViewport(vp->ID);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("##DockSpaceHost", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoResize   | ImGuiWindowFlags_NoMove     |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus);
    ImGui::PopStyleVar(3);

    m_dockspaceId = ImGui::GetID("MainDockSpace");
    ImGui::DockSpace(m_dockspaceId, ImVec2(0, 0), ImGuiDockNodeFlags_PassthruCentralNode);

    ImGuiIO& io = ImGui::GetIO();
    if (io.IniFilename && !std::filesystem::exists(io.IniFilename)) {
        buildDefaultLayout(m_dockspaceId, workSize);
    }
}

void ImGuiLayer::endFrame() {
    ImGui::End(); // Close DockSpaceHost
    ImGui::Render();
    ImGui_ImplBgfx_RenderDrawData(ImGui::GetDrawData());
}

void ImGuiLayer::buildDefaultLayout(ImGuiID dockspaceId, ImVec2 size) {
    ImGui::DockBuilderRemoveNode(dockspaceId);
    ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockspaceId, size);

    ImGuiID main = dockspaceId;
    
    // Left Dock (Toolbar) - very narrow
    ImGuiID left = ImGui::DockBuilderSplitNode(main, ImGuiDir_Left, 0.035f, nullptr, &main);
    
    // Right Dock (Explorer + Properties) - ~300px roughly or 25%
    ImGuiID right = ImGui::DockBuilderSplitNode(main, ImGuiDir_Right, 0.25f, nullptr, &main);
    
    // Center Bottom (Asset Browser) - 28%
    ImGuiID bottom = ImGui::DockBuilderSplitNode(main, ImGuiDir_Down, 0.28f, nullptr, &main);
    
    // Center Top vs Middle (Viewport vs Material Editor)
    ImGuiID centerTop = main;
    ImGuiID centerMid = ImGui::DockBuilderSplitNode(centerTop, ImGuiDir_Down, 0.47f, nullptr, &centerTop);
    
    // Right Split (Explorer 38%, Properties 62%)
    ImGuiID explorer = right;
    ImGuiID properties = ImGui::DockBuilderSplitNode(explorer, ImGuiDir_Down, 0.62f, nullptr, &explorer);

    // AI Copilot dock fallback (hidden usually)
    ImGuiID ai = ImGui::DockBuilderSplitNode(explorer, ImGuiDir_Right, 0.2f, nullptr, &explorer);

    ImGui::DockBuilderDockWindow("##LeftToolbar",   left);
    ImGui::DockBuilderDockWindow("Viewport",        centerTop);
    ImGui::DockBuilderDockWindow("Material Editor", centerMid);
    ImGui::DockBuilderDockWindow("Asset Browser",   bottom);
    ImGui::DockBuilderDockWindow("Explorer",        explorer);
    ImGui::DockBuilderDockWindow("Properties",      properties);
    ImGui::DockBuilderDockWindow("AI Copilot",      ai);

    ImGui::DockBuilderFinish(dockspaceId);
}
