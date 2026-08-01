#include <GLFW/glfw3.h>
#include <bgfx/bgfx.h>
#include <iostream>
#include <fstream>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <winsock2.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#elif defined(__APPLE__)
#define GLFW_EXPOSE_NATIVE_COCOA
#elif defined(__linux__)
#define GLFW_EXPOSE_NATIVE_X11
#endif

#include <GLFW/glfw3native.h>

#include <imgui.h>
#include <map>

#include "Engine/Core/Reflection/TypeRegistry.h"
#include "Engine/Core/DataModel/DataModel.h"
#include "Engine/Core/DataModel/Part.h"
#include "Engine/Core/DataModel/DataModelSerializer.h"
#include "Engine/Renderer/Renderer.h"
#include "Engine/Renderer/Camera.h"
#include "Engine/Scripting/LuauRuntime/LuauVM.h"
#include "Engine/Scripting/LuauRuntime/ScriptScheduler.h"
#include "Engine/Scripting/LuauRuntime/InstanceBinding.h"
#include "Engine/Scripting/Script.h"

#include "Engine/Physics/PhysicsWorld.h"

// UI Headers
#include "UI/ImGuiLayer.h"
#include "UI/ViewportPanel.h"
#include "UI/ExplorerPanel.h"
#include "UI/PropertiesPanel.h"
#include "UI/AssetBrowserPanel.h"
#include "UI/MaterialEditorPanel.h"
#include "UI/TopBar.h"
#include "UI/LeftToolbar.h"
#include "UI/AICopilotPanel.h"
#include "Undo/UndoStack.h"

#include "Engine/Assets/AssetDatabase.h"
#include "Engine/Assets/ThumbnailCache.h"
#include "Engine/Assets/AssetImportPipeline.h"
#include "UI/IconRegistry.h"

// Networking Headers
#include "Engine/Networking/Transport/NetworkContext.h"
#include "Engine/Networking/Transport/NetworkServer.h"
#include "Engine/Networking/Transport/NetworkClient.h"
#include "Engine/Networking/Replication/ReplicationManager.h"
#include <string_view>

int main(int argc, char** argv) {
    // Parse arguments
    for (int i = 1; i < argc; ++i) {
        std::string_view arg(argv[i]);
        if (arg == "--server") {
            Engine::Networking::NetworkContext::setMode(Engine::Networking::NetworkMode::Server);
        } else if (arg == "--client") {
            Engine::Networking::NetworkContext::setMode(Engine::Networking::NetworkMode::Client);
        }
    }

    if (Engine::Networking::NetworkContext::mode() == Engine::Networking::NetworkMode::Server) {
        std::cout << "[Main] Starting in SERVER mode.\n";
        Engine::Networking::NetworkServer::instance().start(7777);
    } else if (Engine::Networking::NetworkContext::mode() == Engine::Networking::NetworkMode::Client) {
        std::cout << "[Main] Starting in CLIENT mode.\n";
        Engine::Networking::NetworkClient::instance().connect("127.0.0.1", 7777);
    } else {
        std::cout << "[Main] Starting in STANDALONE mode.\n";
    }

    Engine::Physics::PhysicsWorld::initJolt();

    // 1. Initialize Reflection System
    try {
        Engine::Reflection::TypeRegistry::instance().finalize();
    } catch (const std::exception& e) {
        std::cerr << "Fatal Error: " << e.what() << "\n";
        return -1;
    }

    // 1.5 Initialize Luau
    Engine::Scripting::LuauVM::instance().init();

    // 2. Initialize GLFW
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW\n";
        return -1;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // bgfx manages its own API
    GLFWwindow* window = glfwCreateWindow(1280, 720, "Nexus Studio - Editor", nullptr, nullptr);
    if (!window) { 
        std::cerr << "Failed to create GLFW window\n";
        glfwTerminate(); 
        return -1; 
    }

    // GLFW Callbacks for ImGui are handled by ImGui_ImplGlfw

    // 3. Initialize bgfx
    bgfx::PlatformData pd{};
#if defined(_WIN32)
    pd.nwh = glfwGetWin32Window(window);
#elif defined(__APPLE__)
    pd.nwh = glfwGetCocoaWindow(window);
#elif defined(__linux__)
    pd.nwh = (void*)(uintptr_t)glfwGetX11Window(window);
    pd.ndt = glfwGetX11Display();
#endif

    bgfx::Init init;
    init.platformData = pd;
    init.type = bgfx::RendererType::Count; // Auto-select best API
    init.resolution.width = 1280;
    init.resolution.height = 720;
    init.resolution.reset = BGFX_RESET_VSYNC;
    
    if (!bgfx::init(init)) {
        std::cerr << "Failed to initialize bgfx\n";
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    // Initialize Renderer System
    std::cout << "[INIT] RendererSystem::init()" << std::endl;
    Engine::Renderer::RendererSystem::instance().init();

    // Initialize Physics System
    std::cout << "[INIT] PhysicsWorld::initialize()" << std::endl;
    Engine::Physics::PhysicsWorld::instance().initialize();

    // Initialize UI
    std::cout << "[INIT] ImGuiLayer::init()" << std::endl;
    ImGuiLayer::instance().init(window);

    // Initialize Assets System
    std::cout << "[INIT] Asset System" << std::endl;
    Engine::Assets::AssetDatabase::instance().initialize("C:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus Studio");
    Engine::Assets::AssetImportPipeline::instance().initialize();
    Engine::Assets::ThumbnailCache::instance().initialize();
    Editor::UI::AssetBrowserPanel assetBrowser;
    assetBrowser.initialize();

    // Create Panels
    std::cout << "[INIT] Create Panels" << std::endl;
    ViewportPanel viewport;
    ExplorerPanel explorer;
    PropertiesPanel properties;
    Editor::UI::MaterialEditorPanel materialEditor;
    TopBar topBar;
    LeftToolbar leftToolbar;
    AICopilotPanel aiCopilot;

    // Create Test Parts
    std::cout << "[INIT] Create Test Parts" << std::endl;
    auto part1 = std::make_shared<Part>();
    part1->name = "MyCube1";
    part1->setPosition({0, 5, 0});
    std::cout << "[INIT] Add part1 to Workspace" << std::endl;
    part1->setParent(DataModel::instance());

    auto part2 = std::make_shared<Part>();
    part2->name = "Ground";
    part2->setSize({10.0f, 1.0f, 10.0f});
    part2->setPosition({0, -1.0f, 0});
    part2->setAnchored(true);
    std::cout << "[INIT] Add part2 to Workspace" << std::endl;
    part2->setParent(DataModel::instance());

    std::string projectRoot = Engine::Assets::AssetDatabase::instance().getProjectRoot();
    std::cout << "[INIT] Project Root: " << projectRoot << std::endl;
    
    // Load Icons
    IconRegistry::instance().loadAll(projectRoot + "/Assets/Icons");

    // UI Panels
    std::cout << "[INIT] Setup DeltaTime" << std::endl;
    Engine::Renderer::Camera camera;
    camera.position = {0.0f, 2.0f, -10.0f};
    camera.forward = {0.0f, -0.2f, 1.0f};

    double lastTime = glfwGetTime();
    double lastInputTime = glfwGetTime();
    double cursorX = 0.0, cursorY = 0.0;

    // 4. Main Loop
    nlohmann::json snapshotJson;
    bool isSimulating = false;

    while (!glfwWindowShouldClose(window)) {
        // --- Power Saving / Idle Throttling ---
        double timeNow = glfwGetTime();
        bool isIdle = !isSimulating && (timeNow - lastInputTime > 1.0);
        
        if (isIdle) {
            // Tamamen 0 FPS'e dusur. Olay (Event) gelene kadar döngüyü tamamen dondur.
            // (10 FPS limit, G-Sync/FreeSync (VRR) olan monitorlerde parlaklik dalgalanmasina sebep oluyordu).
            glfwWaitEvents(); 
        } else {
            glfwPollEvents();
        }

        double currentTime = glfwGetTime();
        float deltaTime = static_cast<float>(currentTime - lastTime);
        lastTime = currentTime;

        // Simple Undo/Redo shortcuts (Ctrl+Z / Ctrl+Y)
        bool ctrlPressed = (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) ||
                           (glfwGetKey(window, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS);
        
        static bool zPressed = false;
        static bool yPressed = false;

        if (ctrlPressed) {
            if (glfwGetKey(window, GLFW_KEY_Z) == GLFW_PRESS) {
                if (!zPressed) { UndoStack::instance().undo(); zPressed = true; }
            } else { zPressed = false; }
            
            if (glfwGetKey(window, GLFW_KEY_Y) == GLFW_PRESS) {
                if (!yPressed) { UndoStack::instance().redo(); yPressed = true; }
            } else { yPressed = false; }
        }

        // Handle window resize
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);
        if (width > 0 && height > 0) {
            bgfx::reset(width, height, BGFX_RESET_VSYNC);
        }

        // Clear default backbuffer View 255 for ImGui
        bgfx::setViewClear(255, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x111111ff, 1.0f, 0);
        bgfx::setViewRect(255, 0, 0, bgfx::BackbufferRatio::Equal);
        bgfx::touch(255);

        // Update Physics & Scripts only if simulating
        static int frameCount = 0;
        if (isSimulating) {
            Engine::Physics::PhysicsWorld::instance().step(deltaTime);
            Engine::Scripting::ScriptScheduler::instance().update(deltaTime);
            
            if (Engine::Networking::NetworkContext::mode() == Engine::Networking::NetworkMode::Server) {
                Engine::Networking::ReplicationManager::instance().flushToAllClients(deltaTime);
            }
        }

        // Network Polling
        if (Engine::Networking::NetworkContext::mode() == Engine::Networking::NetworkMode::Server) {
            Engine::Networking::NetworkServer::instance().poll();
        } else if (Engine::Networking::NetworkContext::mode() == Engine::Networking::NetworkMode::Client) {
            Engine::Networking::NetworkClient::instance().poll();
        }

        // UI Frame
        ImGuiLayer::instance().beginFrame();

        bool toggleSim = false;



        // F5 shortcut to toggle simulation
        static bool f5Pressed = false;
        if (glfwGetKey(window, GLFW_KEY_F5) == GLFW_PRESS) {
            if (!f5Pressed) { toggleSim = true; f5Pressed = true; }
        } else { f5Pressed = false; }

        if (toggleSim) {
            auto workspace = DataModel::instance()->findFirstChild("Workspace");
            if (!isSimulating) {
                // START PLAY: snapshot Workspace
                if (workspace) {
                    snapshotJson = Engine::Core::DataModelSerializer::serialize(workspace);
                }
                isSimulating = true;
            } else {
                // STOP PLAY: restore Workspace
                isSimulating = false;
                
                if (workspace) {
                    workspace->destroy();
                }
                
                if (!snapshotJson.is_null()) {
                    auto restoredWorkspace = Engine::Core::DataModelSerializer::deserialize(snapshotJson);
                    if (restoredWorkspace) {
                        restoredWorkspace->setParent(DataModel::instance());
                    }
                }
            }
        }

        if (frameCount < 5) std::cout << "[DEBUG] Frame " << frameCount << ": Draw Viewport" << std::endl;
        topBar.draw(isSimulating, toggleSim);
        leftToolbar.draw();
        aiCopilot.draw();
        viewport.draw(camera);
        explorer.draw();
        properties.draw();
        assetBrowser.draw();
        
        static bool showMaterialEditor = true;
        materialEditor.draw(&showMaterialEditor);

        if (frameCount < 5) std::cout << "[DEBUG] Frame " << frameCount << ": End ImGui" << std::endl;
        
        // Input Activity Tracking
        bool hasInput = false;
        double newCursorX, newCursorY;
        glfwGetCursorPos(window, &newCursorX, &newCursorY);
        if (newCursorX != cursorX || newCursorY != cursorY) {
            hasInput = true;
            cursorX = newCursorX;
            cursorY = newCursorY;
        }
        if (!hasInput) {
            for (int i = 32; i < 348; i++) {
                if (glfwGetKey(window, i) == GLFW_PRESS) { hasInput = true; break; }
            }
        }
        if (!hasInput) {
            for (int i = 0; i < 8; i++) {
                if (glfwGetMouseButton(window, i) == GLFW_PRESS) { hasInput = true; break; }
            }
        }
        
        if (hasInput) {
            lastInputTime = currentTime;
        }

        ImGuiLayer::instance().endFrame();

        // Submit the frame
        if (frameCount < 5) std::cout << "[DEBUG] Frame " << frameCount << ": BGFX frame" << std::endl;
        bgfx::frame();
        frameCount++;
    }

    std::cout << "[DEBUG] Main loop ended." << std::endl;

    // 5. Cleanup
    ImGuiLayer::instance().shutdown();
    std::cout << "[DEBUG] ImGui shutdown.\n";
    Engine::Physics::PhysicsWorld::instance().shutdown();
    std::cout << "[DEBUG] Physics shutdown.\n";
    Engine::Scripting::LuauVM::instance().shutdown();
    std::cout << "[DEBUG] Luau shutdown.\n";
    Engine::Assets::ThumbnailCache::instance().shutdown();
    std::cout << "[DEBUG] Assets shutdown.\n";
    Engine::Renderer::RendererSystem::instance().shutdown();
    std::cout << "[DEBUG] Renderer shutdown.\n";
    bgfx::shutdown();
    std::cout << "[DEBUG] BGFX shutdown.\n";
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
