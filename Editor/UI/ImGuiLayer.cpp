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
        io.Fonts->AddFontFromFileTTF(fontPath, 13.0f);
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
    ImGui::SetNextWindowPos(vp->WorkPos);
    ImGui::SetNextWindowSize(vp->WorkSize);
    ImGui::SetNextWindowViewport(vp->ID);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("##DockSpaceHost", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoResize   | ImGuiWindowFlags_NoMove     |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus |
        ImGuiWindowFlags_MenuBar);
    ImGui::PopStyleVar(3);

    m_dockspaceId = ImGui::GetID("MainDockSpace");
    ImGui::DockSpace(m_dockspaceId, ImVec2(0, 0), ImGuiDockNodeFlags_PassthruCentralNode);

    ImGuiIO& io = ImGui::GetIO();
    if (io.IniFilename && !std::filesystem::exists(io.IniFilename)) {
        buildDefaultLayout(m_dockspaceId, vp->WorkSize);
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

    ImGuiID main      = dockspaceId;
    ImGuiID left      = ImGui::DockBuilderSplitNode(main,  ImGuiDir_Left,  0.037f, nullptr, &main);
    ImGuiID right     = ImGui::DockBuilderSplitNode(main,  ImGuiDir_Right, 0.27f,  nullptr, &main);
    ImGuiID ai        = ImGui::DockBuilderSplitNode(right, ImGuiDir_Right, 0.45f,  nullptr, &right);
    ImGuiID bottom    = ImGui::DockBuilderSplitNode(main,  ImGuiDir_Down,  0.28f,  nullptr, &main);
    ImGuiID centerTop = main;
    ImGuiID centerMid = ImGui::DockBuilderSplitNode(centerTop, ImGuiDir_Down, 0.47f, nullptr, &centerTop);
    ImGuiID explorer  = right;
    ImGuiID properties = ImGui::DockBuilderSplitNode(explorer, ImGuiDir_Down, 0.62f, nullptr, &explorer);

    ImGui::DockBuilderDockWindow("##LeftToolbar",   left);
    ImGui::DockBuilderDockWindow("Viewport",        centerTop);
    ImGui::DockBuilderDockWindow("Material Editor", centerMid);
    ImGui::DockBuilderDockWindow("Asset Browser",   bottom);
    ImGui::DockBuilderDockWindow("Explorer",        explorer);
    ImGui::DockBuilderDockWindow("Properties",      properties);
    ImGui::DockBuilderDockWindow("AI Copilot",      ai);

    ImGui::DockBuilderFinish(dockspaceId);
}
