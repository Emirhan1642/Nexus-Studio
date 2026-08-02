#include "ImGuiLayer.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <backends/imgui_impl_glfw.h>
#include "Backend/imgui_impl_bgfx.h"
#include <filesystem>
#include <widgets/gizmo.h>
#include "Engine/Assets/AssetDatabase.h"
#include "NexusTheme.h"

#define VIEW_ID_IMGUI 255

void ImGuiLayer::init(GLFWwindow* window) {
    m_window = window;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    // ImGui 1.92.6: GLFW -> bgfx -> fontlar sirasi zorunlu
    // 1. Platform backend
    ImGui_ImplGlfw_InitForOther(window, true);
    // 2. Renderer backend (ImGuiBackendFlags_RendererHasTextures set edilir)
    ImGui_ImplBgfx_Init(VIEW_ID_IMGUI);

    // 3. Fontlari ekle
    std::string projectRoot = Engine::Assets::AssetDatabase::instance().getProjectRoot();
    std::string fontPath = projectRoot + "/Assets/Fonts/Inter-Regular.ttf";
    std::string fontBoldPath = projectRoot + "/Assets/Fonts/Roboto-Medium.ttf";
    
    if (!std::filesystem::exists(fontPath)) fontPath = "Assets/Fonts/Inter-Regular.ttf";
    if (!std::filesystem::exists(fontBoldPath)) fontBoldPath = "Assets/Fonts/Roboto-Medium.ttf";

    if (std::filesystem::exists(fontPath)) {
        ImFontConfig fontCfg;
        fontCfg.OversampleH = 3;
        fontCfg.OversampleV = 1;
        fontCfg.PixelSnapH = false;
        
        // Font 0: Default (14px)
        io.Fonts->AddFontFromFileTTF(fontPath.c_str(), 14.0f, &fontCfg);
        
        // Font 1: Small (12px)
        io.Fonts->AddFontFromFileTTF(fontPath.c_str(), 12.0f, &fontCfg);
        
        // Font 2: Tiny (10px)
        io.Fonts->AddFontFromFileTTF(fontPath.c_str(), 10.0f, &fontCfg);
        
        // Font 3: Medium/Bold (12px)
        if (std::filesystem::exists(fontBoldPath)) {
            io.Fonts->AddFontFromFileTTF(fontBoldPath.c_str(), 12.0f, &fontCfg);
        } else {
            io.Fonts->AddFontFromFileTTF(fontPath.c_str(), 12.0f, &fontCfg);
        }
        
        io.FontDefault = io.Fonts->Fonts[0];
    }

    // NOT: Build() cagirma - ImGui 1.92.6 RendererHasTextures ile otomatik halleder!

    // 4. Tema uygula
    NexusTheme::instance().apply();

    ImGuizmo::SetImGuiContext(ImGui::GetCurrentContext());
    ImGuizmo::Create();
}

void ImGuiLayer::shutdown() {
    ImGuizmo::Destroy();
    ImGui_ImplBgfx_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

void ImGuiLayer::beginFrame() {
    ImGui_ImplBgfx_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    ImGuizmo::BeginFrame();

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
    
    // Left Dock (Toolbar) - fixed roughly 44px, ratio depends on screen, let's say ~0.03f
    ImGuiID left = ImGui::DockBuilderSplitNode(main, ImGuiDir_Left, 0.025f, nullptr, &main);
    
    // Right Dock (Explorer + Properties + AICopilot)
    ImGuiID right = ImGui::DockBuilderSplitNode(main, ImGuiDir_Right, 0.35f, nullptr, &main);
    
    // Center Bottom (Asset Browser) - 28%
    ImGuiID bottom = ImGui::DockBuilderSplitNode(main, ImGuiDir_Down, 0.28f, nullptr, &main);
    
    // Center Top vs Middle (Viewport vs Material Editor)
    ImGuiID centerTop = main;
    ImGuiID centerMid = ImGui::DockBuilderSplitNode(centerTop, ImGuiDir_Down, 0.47f, nullptr, &centerTop);
    
    // Right Split: AI Copilot on the far right (w=288px ~ 45% of right panel)
    ImGuiID ai = ImGui::DockBuilderSplitNode(right, ImGuiDir_Right, 0.45f, nullptr, &right);
    
    // Right Split remaining (Explorer 38%, Properties 62%)
    ImGuiID explorer = right;
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
