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

    // 1. Platform backend
    ImGui_ImplGlfw_InitForOther(window, true);

    // 2. Fontlari ekle (backend init'TEN ONCE! Backend init sirasinda atlas olusturuluyor)
    std::string fontPath = "C:/Windows/Fonts/arial.ttf";

    if (std::filesystem::exists(fontPath)) {
        ImFontConfig fontCfg;
        fontCfg.FontDataOwnedByAtlas = true;

        io.Fonts->AddFontFromFileTTF(fontPath.c_str(), 14.0f, &fontCfg);
        io.Fonts->AddFontFromFileTTF(fontPath.c_str(), 12.0f, &fontCfg);
        io.Fonts->AddFontFromFileTTF(fontPath.c_str(), 10.0f, &fontCfg);

        io.FontDefault = io.Fonts->Fonts[0];
    } else {
        io.Fonts->AddFontDefault();
    }

    // Atlasi tamamen derle (backend init sirasinda hazir olmali)
    io.Fonts->Build();

    // 3. Renderer backend (font atlas'i RGBA32 olarak GPU'ya yukler)
    ImGui_ImplBgfx_Init(VIEW_ID_IMGUI);

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
    float topBarHeight = 30.0f;
    float bottomBarHeight = 20.0f;
    // Offset dockspace for our custom 30px TopBar and 20px BottomBar
    workPos.y += topBarHeight;
    workSize.y -= (topBarHeight + bottomBarHeight);

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

    // 1. Left toolbar — fixed ~50px
    //    ratio = 50 / size.x (approx 0.035 at 1440px)
    float leftRatio = 50.0f / size.x;
    ImGuiID left = ImGui::DockBuilderSplitNode(main, ImGuiDir_Left, leftRatio, nullptr, &main);

    // 2. Copilot — far right, fixed ~270px
    float copilotRatio = 270.0f / (size.x - 50.0f);
    ImGuiID copilot = ImGui::DockBuilderSplitNode(main, ImGuiDir_Right, copilotRatio, nullptr, &main);

    // 3. Explorer + Properties column — right of center, fixed ~270px
    float rightRatio = 270.0f / (size.x - 50.0f - 270.0f);
    ImGuiID right = ImGui::DockBuilderSplitNode(main, ImGuiDir_Right, rightRatio, nullptr, &main);

    // 4. Asset Manager — bottom of center, fixed ~244px
    float assetRatio = 244.0f / size.y;
    ImGuiID bottom = ImGui::DockBuilderSplitNode(main, ImGuiDir_Down, assetRatio, nullptr, &main);

    // 5. FileListBar — top of center, fixed ~30px
    float fileListRatio = 30.0f / size.y;
    ImGuiID fileList = ImGui::DockBuilderSplitNode(main, ImGuiDir_Up, fileListRatio, nullptr, &main);

    // 6. Viewport fills the remaining center
    ImGuiID centerTop = main;

    // 7. Right column: Explorer top (~38%), Properties bottom (~62%)
    ImGuiID explorer   = right;
    ImGuiID properties = ImGui::DockBuilderSplitNode(explorer, ImGuiDir_Down, 0.57f, nullptr, &explorer);

    // ── Dock windows ─────────────────────────────────────────────────
    ImGui::DockBuilderDockWindow("##LeftToolbar", left);
    ImGui::DockBuilderDockWindow("FileListBar",   fileList);
    ImGui::DockBuilderDockWindow("Viewport",      centerTop);
    ImGui::DockBuilderDockWindow("Asset Browser", bottom);
    ImGui::DockBuilderDockWindow("Explorer",      explorer);
    ImGui::DockBuilderDockWindow("Properties",    properties);
    ImGui::DockBuilderDockWindow("AI Copilot",    copilot);
    // Material Editor shares the Asset Browser tab group by default
    ImGui::DockBuilderDockWindow("Material Editor", bottom);

    ImGui::DockBuilderFinish(dockspaceId);
}

