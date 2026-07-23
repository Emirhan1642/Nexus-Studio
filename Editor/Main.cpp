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

#include <dear-imgui/imgui.h>
#include <map>

#include "Engine/Core/Reflection/TypeRegistry.h"
#include "Engine/Core/DataModel/DataModel.h"
#include "Engine/Core/DataModel/Part.h"
#include "Engine/Core/DataModel/DataModelSnapshot.h"
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
#include "Undo/UndoStack.h"

#include "Engine/Assets/AssetDatabase.h"
#include "Engine/Assets/ThumbnailCache.h"
#include "Engine/Assets/AssetImportPipeline.h"

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

    // GLFW Callbacks for ImGui
    glfwSetScrollCallback(window, [](GLFWwindow*, double xoffset, double yoffset) {
        ImGuiLayer::instance().onScroll(yoffset);
    });
    glfwSetCharCallback(window, [](GLFWwindow*, unsigned int codepoint) {
        ImGuiLayer::instance().onChar(codepoint);
    });

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
    Editor::UI::AssetBrowserPanel::instance().initialize();

    // Create Panels
    std::cout << "[INIT] Create Panels" << std::endl;
    ViewportPanel viewport;
    ExplorerPanel explorer;
    PropertiesPanel properties;

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

    std::cout << "[INIT] Setup DeltaTime" << std::endl;
    Engine::Renderer::Camera camera;
    camera.position = {0.0f, 2.0f, -10.0f};
    camera.forward = {0.0f, -0.2f, 1.0f};

    double lastTime = glfwGetTime();

    // 4. Main Loop
    bool isSimulating = false;
    DataModelSnapshot snapshot;

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

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

        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu("Simulation")) {
                if (ImGui::MenuItem(isSimulating ? "Stop (F5)" : "Play (F5)")) {
                    toggleSim = true;
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
            ImGui::EndMainMenuBar();
        }

        // F5 shortcut to toggle simulation
        static bool f5Pressed = false;
        if (glfwGetKey(window, GLFW_KEY_F5) == GLFW_PRESS) {
            if (!f5Pressed) { toggleSim = true; f5Pressed = true; }
        } else { f5Pressed = false; }

        if (toggleSim) {
            if (!isSimulating) {
                // START PLAY: snapshot entire DataModel
                snapshot.capture(DataModel::instance());
                isSimulating = true;
            } else {
                // STOP PLAY: restore entire DataModel
                isSimulating = false;
                snapshot.restore(DataModel::instance());
            }
        }

        if (frameCount < 5) std::cout << "[DEBUG] Frame " << frameCount << ": Draw Viewport" << std::endl;
        viewport.draw(camera);
        explorer.draw();
        properties.draw();
        Editor::UI::AssetBrowserPanel::instance().draw();

        if (frameCount < 5) std::cout << "[DEBUG] Frame " << frameCount << ": End ImGui" << std::endl;
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
